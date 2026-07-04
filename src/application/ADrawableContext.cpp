#include "application/ADrawableContext.hpp"

#include <librdr3.hpp>
#include <drawable/drawable.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <pugixml.hpp>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include "ubo/common.hpp"
#include "util/fileutil.hpp"

ADrawableContext::ADrawableContext() {

}

ADrawableContext::~ADrawableContext() {
    DestroyGLResources();
}

bool ADrawableContext::IsRDRPath(const std::string& path) const {
    return path.find(".rsc") != std::string::npos || 
           path.find("RDR") != std::string::npos ||
           path.find("rdr") != std::string::npos;
}

// Helper: Parse space-separated floats from text
static std::vector<float> ParseFloatArray(const std::string& text) {
    std::vector<float> result;
    std::istringstream iss(text);
    float value;
    while (iss >> value) {
        result.push_back(value);
    }
    return result;
}

// Helper: Parse space-separated integers from text
static std::vector<uint32_t> ParseIntArray(const std::string& text) {
    std::vector<uint32_t> result;
    std::istringstream iss(text);
    uint32_t value;
    while (iss >> value) {
        result.push_back(value);
    }
    return result;
}

// Helper: Extract positions from raw vertex data (PNXXCCTTT format)
// Each vertex has multiple floats; position is always first 3
static std::vector<glm::vec3> ExtractPositions(const std::vector<float>& allVertexFloats, uint32_t vertexCount) {
    std::vector<glm::vec3> positions;
    positions.reserve(vertexCount);
    
    if (vertexCount == 0) {
        return positions;
    }
    
    // Calculate floats per vertex
    uint32_t floatsPerVertex = allVertexFloats.size() / vertexCount;
    
    if (floatsPerVertex < 3) {
        std::cerr << "ERROR: floatsPerVertex=" << floatsPerVertex << " is less than 3!" << std::endl;
        return positions;
    }
    
    std::cout << "  FloatsPerVertex: " << floatsPerVertex << " (total=" << allVertexFloats.size() 
              << ", vertexCount=" << vertexCount << ")" << std::endl;
    
    // Extract position (first 3 floats) for each vertex with correct stride
    for (uint32_t i = 0; i < vertexCount; ++i) {
        uint32_t baseIndex = i * floatsPerVertex;
        
        if (baseIndex + 2 < allVertexFloats.size()) {
            positions.emplace_back(
                allVertexFloats[baseIndex],
                allVertexFloats[baseIndex + 1],
                allVertexFloats[baseIndex + 2]
            );
        }
    }
    
    std::cout << "  Extracted " << positions.size() << " positions (stride=" << floatsPerVertex << ")" << std::endl;
    
    return positions;
}

CDrawableMetadata ADrawableContext::ExtractMetadata(const std::string& path, const std::shared_ptr<CDrawable>& drawable) {
    CDrawableMetadata meta;
    meta.filePath = path;
    meta.isRDR2 = IsRDRPath(path);
    meta.loadedFromXml = path.find(".xml") != std::string::npos;
    
    // Extract filename without extension
    std::filesystem::path p(path);
    meta.name = p.stem().string();
    
    // Note: Full metadata extraction would require deeper librdr3 inspection
    // For now, we track basic info
    
    return meta;
}

// Try to parse Sollumz RDR2Drawable XML format
bool ADrawableContext::TryLoadSollumzXml(const std::filesystem::path& xmlPath) {
    try {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(xmlPath.c_str());
        
        if (!result) {
            mLastLoadError = "Failed to parse Sollumz XML: " + std::string(result.description());
            return false;
        }
        
        // Check if this is a RDR2Drawable (Sollumz format)
        auto root = doc.child("RDR2Drawable");
        if (!root) {
            return false;  // Not Sollumz format
        }
        
        // Extract basic info
        auto nameNode = root.child("Name");
        std::string drawableName = nameNode ? nameNode.text().as_string("Unknown") : "Unknown";
        
        // Extract bounding box
        glm::vec3 bbMin(0.0f), bbMax(0.0f);
        auto bbMinNode = root.child("BoundingBoxMin");
        auto bbMaxNode = root.child("BoundingBoxMax");
        
        if (bbMinNode) {
            bbMin.x = bbMinNode.attribute("x").as_float(0.0f);
            bbMin.y = bbMinNode.attribute("y").as_float(0.0f);
            bbMin.z = bbMinNode.attribute("z").as_float(0.0f);
        }
        
        if (bbMaxNode) {
            bbMax.x = bbMaxNode.attribute("x").as_float(0.0f);
            bbMax.y = bbMaxNode.attribute("y").as_float(0.0f);
            bbMax.z = bbMaxNode.attribute("z").as_float(0.0f);
        }
        
        // Count shaders and models from LOD hierarchy
        uint32_t shaderCount = 0;
        for (auto shader : root.child("ShaderGroup").children("Shaders")) {
            for (auto item : shader.children("Item")) {
                shaderCount++;
            }
        }
        
        uint32_t modelCount = 0;
        for (auto lod : root.children()) {
            if (std::string(lod.name()).find("Lod") != std::string::npos) {
                for (auto model : lod.child("Models").children("Item")) {
                    modelCount++;
                }
            }
        }
        
        // Create drawable with geometry
        auto drawable = std::make_shared<CDrawable>();
        if (drawable) {
            mDrawables.push_back(drawable);
            auto meta = ExtractMetadata(xmlPath.generic_string(), drawable);
            meta.loadedFromXml = true;
            meta.shaderCount = shaderCount;
            meta.modelCount = modelCount;
            meta.bbMin = bbMin;
            meta.bbMax = bbMax;
            
            // Parse geometry from LOD hierarchy
            for (auto lod : root.children()) {
                std::string lodName(lod.name());
                if (lodName.find("Lod") != std::string::npos) {
                    auto modelsNode = lod.child("Models");
                    if (!modelsNode) continue;
                    
                    for (auto modelItem : modelsNode.children("Item")) {
                        auto geometriesNode = modelItem.child("Geometries");
                        if (!geometriesNode) continue;
                        
                        for (auto geomItem : geometriesNode.children("Item")) {
                            CGeometryItem geom;
                            
                            // Parse vertices
                            auto verticesNode = geomItem.child("Vertices");
                            if (verticesNode) {
                                std::string vertexText = verticesNode.text().as_string("");
                                auto floatData = ParseFloatArray(vertexText);
                                
                                // Parse indices to determine vertex count
                                auto indicesNode = geomItem.child("Indices");
                                if (indicesNode) {
                                    std::string indicesText = indicesNode.text().as_string("");
                                    geom.indices = ParseIntArray(indicesText);
                                    geom.indexCount = geom.indices.size();
                                    
                                    if (!geom.indices.empty()) {
                                        // Find max index to determine vertex count
                                        uint32_t maxIndex = *std::max_element(geom.indices.begin(), geom.indices.end());
                                        uint32_t vertexCount = maxIndex + 1;
                                        
                                        // Extract positions (first 3 floats per vertex in NonInterleaved format)
                                        geom.positions = ExtractPositions(floatData, vertexCount);
                                        
                                        std::cout << "    Geometry: " << geom.indices.size() << " indices, " 
                                                  << geom.positions.size() << " positions, max_index=" << maxIndex << std::endl;
                                        
                                        if (!geom.positions.empty() && !geom.indices.empty()) {
                                            meta.geometries.push_back(geom);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            mLastLoadInfo = "Loaded Sollumz YDR: " + drawableName + 
                           " (" + std::to_string(shaderCount) + " shaders, " + 
                           std::to_string(meta.geometries.size()) + " geometries)";
            
            std::cout << "  " << mLastLoadInfo << std::endl;
            
            mDrawableMetadata.push_back(meta);
            return true;
        }
        
        return false;
        
    } catch (const std::exception& ex) {
        mLastLoadError = "Exception parsing Sollumz XML: " + std::string(ex.what());
        return false;
    }
}

bool ADrawableContext::LoadDrawable(std::filesystem::path filePath) {
    ZoneNamed(LoadDrawable, true);

    std::string filePathStr = filePath.generic_string();
    bool isRDR = IsRDRPath(filePathStr);
    
    // Check if we're already given an XML file
    bool isXmlInput = (filePath.extension() == ".xml");
    
    if (isXmlInput) {
        // Try Sollumz format first
        if (TryLoadSollumzXml(filePath)) {
            return true;
        }
        
        // Try standard librdr3 XML import
        auto drawable = librdr3::ImportYdr(filePathStr);
        if (drawable) {
            mDrawables.push_back(drawable);
            auto meta = ExtractMetadata(filePathStr, drawable);
            meta.loadedFromXml = true;
            mDrawableMetadata.push_back(meta);
            mLastLoadInfo = "Loaded drawable (XML): " + filePath.filename().string() + (isRDR ? " (RDR2)" : " (GTA5)");
            return true;
        }
        
        mLastLoadError = "Failed to load drawable XML: " + filePath.filename().string();
        return false;
    }

    // Try native format first (.ydr)
    auto drawable = librdr3::ImportYdr(filePathStr);
    if (drawable) {
        mDrawables.push_back(drawable);
        mDrawableMetadata.push_back(ExtractMetadata(filePathStr, drawable));
        mLastLoadInfo = "Loaded drawable: " + filePath.filename().string() + (isRDR ? " (RDR2)" : " (GTA5)");
        return true;
    }

    // Fall back to XML format (.ydr.xml)
    std::filesystem::path xmlPath = filePath.string() + ".xml";
    if (std::filesystem::exists(xmlPath)) {
        // Try Sollumz format first
        if (TryLoadSollumzXml(xmlPath)) {
            return true;
        }
        
        drawable = librdr3::ImportYdr(xmlPath.generic_string());
        if (drawable) {
            mDrawables.push_back(drawable);
            auto meta = ExtractMetadata(xmlPath.generic_string(), drawable);
            meta.loadedFromXml = true;
            mDrawableMetadata.push_back(meta);
            mLastLoadInfo = "Loaded drawable (XML): " + filePath.filename().string() + (isRDR ? " (RDR2)" : " (GTA5)");
            return true;
        }
    }

    mLastLoadError = "Failed to load drawable: " + filePath.filename().string() + 
                    " (tried .ydr, .ydr.xml, and Sollumz formats)";
    return false;
}

void ADrawableContext::InitGLResources() {
    if (bGLInitialized) {
        return;
    }

    // Create VBO for wireframe cube geometry (12 lines = 24 vertices)
    // Each line segment is defined by 2 vertices
    glCreateBuffers(1, &mBBVBO);
    glCreateVertexArrays(1, &mBBVAO);
    glVertexArrayVertexBuffer(mBBVAO, 0, mBBVBO, 0, sizeof(glm::vec3));
    glEnableVertexArrayAttrib(mBBVAO, 0);  // VERTEX_ATTRIB_INDEX
    glVertexArrayAttribBinding(mBBVAO, 0, 0);
    glVertexArrayAttribFormat(mBBVAO, 0, glm::vec3::length(), GL_FLOAT, GL_FALSE, 0);

    // Compile simple shader for line rendering
    std::string vertTxt = UFileUtil::LoadShaderText("simple.vert");
    const char* vertTxtChars = vertTxt.data();

    uint32_t vertHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertHandle, 1, &vertTxtChars, NULL);
    glCompileShader(vertHandle);

    int32_t success = 0;
    glGetShaderiv(vertHandle, GL_COMPILE_STATUS, &success);
    if (!success) {
        int32_t logSize = 0;
        glGetShaderiv(vertHandle, GL_INFO_LOG_LENGTH, &logSize);
        std::vector<char> log(logSize);
        glGetShaderInfoLog(vertHandle, logSize, nullptr, &log[0]);
        std::cout << "Drawable simple.vert compile error: " << std::string(log.data()) << std::endl;
        glDeleteShader(vertHandle);
        return;
    }

    std::string fragTxt = UFileUtil::LoadShaderText("simple.frag");
    const char* fragTxtChars = fragTxt.data();

    uint32_t fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragHandle, 1, &fragTxtChars, NULL);
    glCompileShader(fragHandle);

    success = 0;
    glGetShaderiv(fragHandle, GL_COMPILE_STATUS, &success);
    if (!success) {
        int32_t logSize = 0;
        glGetShaderiv(fragHandle, GL_INFO_LOG_LENGTH, &logSize);
        std::vector<char> log(logSize);
        glGetShaderInfoLog(fragHandle, logSize, nullptr, &log[0]);
        std::cout << "Drawable simple.frag compile error: " << std::string(log.data()) << std::endl;
        glDeleteShader(vertHandle);
        glDeleteShader(fragHandle);
        return;
    }

    mSimpleProgram = glCreateProgram();
    glAttachShader(mSimpleProgram, vertHandle);
    glAttachShader(mSimpleProgram, fragHandle);
    glLinkProgram(mSimpleProgram);

    glDetachShader(mSimpleProgram, vertHandle);
    glDetachShader(mSimpleProgram, fragHandle);
    glDeleteShader(vertHandle);
    glDeleteShader(fragHandle);

    UCommonUniformBuffer::LinkShaderToUBO(mSimpleProgram);
    mBaseColorUniform = glGetUniformLocation(mSimpleProgram, "uBaseColor");

    bGLInitialized = true;
    std::cout << "ADrawableContext GL resources initialized." << std::endl;
}

void ADrawableContext::DestroyGLResources() {
    if (!bGLInitialized) {
        return;
    }

    // Clean up per-geometry resources
    for (auto& metadata : mDrawableMetadata) {
        for (auto& geom : metadata.geometries) {
            if (geom.VBO) glDeleteBuffers(1, &geom.VBO);
            if (geom.EBO) glDeleteBuffers(1, &geom.EBO);
            if (geom.VAO) glDeleteVertexArrays(1, &geom.VAO);
            geom.VBO = geom.EBO = geom.VAO = 0;
        }
    }

    // Clean up general resources
    glDeleteBuffers(1, &mBBVBO);
    glDeleteVertexArrays(1, &mBBVAO);
    glDeleteProgram(mSimpleProgram);

    mBBVBO = 0;
    mBBVAO = 0;
    mSimpleProgram = 0;
    bGLInitialized = false;
}

void ADrawableContext::Render(ASceneCamera& camera) {
    if (mDrawableMetadata.empty()) {
        return;
    }

    if (!bGLInitialized) {
        InitGLResources();
    }

    // Set up common uniforms
    UCommonUniformBuffer::SetProjAndViewMatrices(camera.GetProjectionMatrix(), camera.GetViewMatrix());
    UCommonUniformBuffer::SubmitUBO();

    glUseProgram(mSimpleProgram);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render each drawable's geometry
    for (auto& metadata : mDrawableMetadata) {
        // Color code by game type: RDR2 = red, GTA5 = blue
        glm::vec4 color = metadata.isRDR2 ? glm::vec4(1.0f, 0.2f, 0.2f, 0.8f) : glm::vec4(0.2f, 0.5f, 1.0f, 0.8f);
        glUniform4fv(mBaseColorUniform, 1, glm::value_ptr(color));

        // Render each geometry in this drawable
        for (auto& geom : metadata.geometries) {
            // Initialize GL resources for this geometry if not done yet
            if (!geom.VAO && !geom.positions.empty() && !geom.indices.empty()) {
                // Create VAO
                glCreateVertexArrays(1, &geom.VAO);
                
                // Create VBO for positions
                glCreateBuffers(1, &geom.VBO);
                glNamedBufferData(geom.VBO, geom.positions.size() * sizeof(glm::vec3), geom.positions.data(), GL_STATIC_DRAW);
                
                // Create EBO for indices
                glCreateBuffers(1, &geom.EBO);
                glNamedBufferData(geom.EBO, geom.indices.size() * sizeof(uint32_t), geom.indices.data(), GL_STATIC_DRAW);
                
                // Set up vertex attribute format
                glVertexArrayVertexBuffer(geom.VAO, 0, geom.VBO, 0, sizeof(glm::vec3));
                glEnableVertexArrayAttrib(geom.VAO, 0);  // VERTEX_ATTRIB_INDEX
                glVertexArrayAttribBinding(geom.VAO, 0, 0);
                glVertexArrayAttribFormat(geom.VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
                
                // Bind element buffer
                glVertexArrayElementBuffer(geom.VAO, geom.EBO);
            }
            
            // Render if geometry is valid
            if (geom.VAO && geom.indexCount > 0) {
                glBindVertexArray(geom.VAO);
                glDrawElements(GL_TRIANGLES, geom.indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }
    }

    // Also render bounding boxes as wireframe for reference (optional)
    glLineWidth(1.5f);
    glBindVertexArray(mBBVAO);
    
    for (const auto& metadata : mDrawableMetadata) {
        if (metadata.bbMin == metadata.bbMax || metadata.geometries.empty()) {
            continue;  // Skip if no geometry or invalid BB
        }

        // Use a lighter shade for wireframe
        glm::vec4 bbColor = metadata.isRDR2 ? glm::vec4(1.0f, 0.5f, 0.5f, 0.4f) : glm::vec4(0.5f, 0.7f, 1.0f, 0.4f);
        glUniform4fv(mBaseColorUniform, 1, glm::value_ptr(bbColor));

        // Build wireframe cube vertices for bounding box
        glm::vec3 min = metadata.bbMin;
        glm::vec3 max = metadata.bbMax;

        glm::vec3 bbVertices[] = {
            // Bottom face (z = min)
            min, glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, max.y, min.z), glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, max.y, min.z), min,
            
            // Top face (z = max)
            glm::vec3(min.x, min.y, max.z), glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z),
            glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z),
            glm::vec3(min.x, max.y, max.z), glm::vec3(min.x, min.y, max.z),
            
            // Vertical edges
            min, glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z), max,
            glm::vec3(min.x, max.y, min.z), glm::vec3(min.x, max.y, max.z)
        };

        // Upload and render wireframe
        glNamedBufferData(mBBVBO, sizeof(bbVertices), bbVertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 24);
    }

    glUseProgram(0);
    glBindVertexArray(0);
    glLineWidth(1.0f);
}
