#pragma once

#include "types.h"
#include "application/ACamera.hpp"

#include <string>
#include <map>
#include <memory>
#include <filesystem>

struct CDrawable;

struct CGeometryItem {
    std::vector<glm::vec3> positions;  // Extracted positions from vertices
    std::vector<uint32_t> indices;
    uint32_t VBO = 0;
    uint32_t EBO = 0;
    uint32_t VAO = 0;
    uint32_t indexCount = 0;
};

struct CDrawableMetadata {
    std::string name;
    std::string filePath;
    uint32_t shaderCount = 0;
    uint32_t modelCount = 0;
    uint32_t boneCount = 0;
    bool isRDR2 = false;  // Track game type
    std::string lastError;
    bool loadedFromXml = false;
    
    // Bounding box for rendering
    glm::vec3 bbMin = glm::vec3(0.0f);
    glm::vec3 bbMax = glm::vec3(0.0f);
    
    // Geometry data for mesh rendering
    std::vector<CGeometryItem> geometries;
};

class ADrawableContext {
    shared_vector<CDrawable> mDrawables;
    std::vector<CDrawableMetadata> mDrawableMetadata;
    std::string mLastLoadError;
    std::string mLastLoadInfo;

    // GL resources for bounding box rendering
    uint32_t mBBVBO = 0;      // Vertex buffer for wireframe cubes
    uint32_t mBBVAO = 0;      // Vertex array object
    uint32_t mSimpleProgram = 0;  // Simple shader program for lines
    uint32_t mBaseColorUniform = 0;  // Color uniform location
    bool bGLInitialized = false;

    // Helper methods
    bool IsRDRPath(const std::string& path) const;
    CDrawableMetadata ExtractMetadata(const std::string& path, const std::shared_ptr<CDrawable>& drawable);
    bool TryLoadSollumzXml(const std::filesystem::path& xmlPath);
    void InitGLResources();
    void DestroyGLResources();

public:
    ADrawableContext();
    ~ADrawableContext();

    bool LoadDrawable(std::filesystem::path filePath);
    void Render(ASceneCamera& camera);  // Render bounding boxes with camera
    size_t GetLoadedDrawableCount() const { return mDrawables.size(); }
    std::string GetLastLoadError() const { return mLastLoadError; }
    std::string GetLastLoadInfo() const { return mLastLoadInfo; }
    const std::vector<CDrawableMetadata>& GetMetadata() const { return mDrawableMetadata; }
};