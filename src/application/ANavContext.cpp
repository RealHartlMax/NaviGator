#include "application/ANavContext.hpp"
#include "ubo/common.hpp"
#include "ubo/litsimple.hpp"
#include "util/fileutil.hpp"

#include <glad/glad.h>
#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

constexpr uint32_t VERTEX_ATTRIB_INDEX = 0;
constexpr uint32_t NORMAL_ATTRIB_INDEX = 1;
constexpr uint32_t COLOR_ATTRIB_INDEX = 2;

namespace {
    struct Rsc8HeaderInfo {
        bool valid = false;
        uint8_t compressorId = 0;
        uint32_t virtualFlags = 0;
        uint32_t physicalFlags = 0;
        uint32_t version = 0;
    };

    std::string Trim(std::string value) {
        const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    }

    std::string ToLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return char(std::tolower(c)); });
        return value;
    }

    std::string ReadValueOrText(const pugi::xml_node& node) {
        if (!node) {
            return "";
        }

        const pugi::xml_attribute valueAttr = node.attribute("value");
        if (valueAttr) {
            return valueAttr.as_string();
        }

        return node.text().as_string();
    }

    std::vector<std::string> SplitFlagTokens(const std::string& rawFlags) {
        std::string normalized = rawFlags;
        std::replace(normalized.begin(), normalized.end(), ';', ',');
        std::replace(normalized.begin(), normalized.end(), '|', ',');

        std::vector<std::string> tokens;
        std::stringstream ss(normalized);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token = ToLower(Trim(token));
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }

        return tokens;
    }

    uint8_t ParsePolygonFlagsNumericToken(const std::string& token) {
        if (token.empty()) {
            return 0;
        }

        bool looksNumeric = std::isdigit(static_cast<unsigned char>(token[0])) != 0;
        looksNumeric = looksNumeric || (token.size() > 2 && token[0] == '0' && token[1] == 'x');
        if (!looksNumeric) {
            return 0;
        }

        try {
            const unsigned long value = std::stoul(token, nullptr, 0);
            return static_cast<uint8_t>(value &
                (EPolygonFlags::SMALL |
                    EPolygonFlags::LARGE |
                    EPolygonFlags::PAVED |
                    EPolygonFlags::SHELTERED |
                    EPolygonFlags::STEEP |
                    EPolygonFlags::WATER));
        }
        catch (...) {
            return 0;
        }
    }

    uint8_t ParsePolygonFlags(const std::string& flagsText) {
        uint8_t flags = 0;
        const std::vector<std::string> tokens = SplitFlagTokens(flagsText);
        for (const std::string& token : tokens) {
            if (token == "small") {
                flags |= EPolygonFlags::SMALL;
            }
            else if (token == "large") {
                flags |= EPolygonFlags::LARGE;
            }
            else if (token == "pavement" || token == "boardwalk" || token == "road") {
                flags |= EPolygonFlags::PAVED;
            }
            else if (token == "sheltered" || token == "sheltered3") {
                flags |= EPolygonFlags::SHELTERED;
            }
            else if (token == "steep" || token == "steepslope") {
                flags |= EPolygonFlags::STEEP;
            }
            else if (token == "water" || token == "shallowwater") {
                flags |= EPolygonFlags::WATER;
            }
            else {
                flags |= ParsePolygonFlagsNumericToken(token);
            }
        }
        return flags;
    }

    CNavPolygonInfo3 ParsePolygonInfo3(const pugi::xml_node& polyItem, const std::string& flagsText, ANavXmlMetadata& ioMetadata) {
        CNavPolygonInfo3 info{};
        info.mAudioProperties = uint16_t(polyItem.child("Audio").attribute("value").as_uint(0));
        info.mPedDensity = uint8_t(polyItem.child("PedDensity").attribute("value").as_uint(0));

        const std::vector<std::string> tokens = SplitFlagTokens(flagsText);
        for (const std::string& token : tokens) {
            if (token == "networkspawncandidate") {
                info.bNetworkSpawnCandidate = true;
                ioMetadata.networkSpawnCandidateCount++;
            }
            else if (token == "road") {
                info.bIsRoad = true;
                ioMetadata.roadPolygonCount++;
            }
            else if (token == "interior") {
                info.bInterior = true;
                ioMetadata.interiorPolygonCount++;
            }
            else if (token == "isolated") {
                info.bIsolated = true;
                ioMetadata.isolatedPolygonCount++;
            }
            else if (token == "shallowwater") {
                info.bIsShallowWater = true;
                ioMetadata.shallowWaterPolygonCount++;
            }
            else if (token == "traintrack" || token == "rail") {
                info.bIsTrainTrack = true;
            }
            else if (token == "nearvehicle") {
                info.bNearVehicle = true;
            }
        }

        info.bNetworkSpawnCandidate = info.bNetworkSpawnCandidate || polyItem.child("NetworkSpawnCandidate").attribute("value").as_bool(false);
        info.bIsRoad = info.bIsRoad || polyItem.child("IsRoad").attribute("value").as_bool(false);
        info.bInterior = info.bInterior || polyItem.child("Interior").attribute("value").as_bool(false);
        info.bIsolated = info.bIsolated || polyItem.child("Isolated").attribute("value").as_bool(false);
        info.bIsShallowWater = info.bIsShallowWater || polyItem.child("IsShallowWater").attribute("value").as_bool(false);
        info.bNearVehicle = info.bNearVehicle || polyItem.child("NearVehicle").attribute("value").as_bool(false);
        info.bIsTrainTrack = info.bIsTrainTrack || polyItem.child("IsTrainTrack").attribute("value").as_bool(false);

        return info;
    }

    bool TryParseYnvXml(
        const std::filesystem::path& filePath,
        std::vector<std::vector<Vector3>>& outPolygons,
        std::vector<uint8_t>& outPolygonFlags,
        std::vector<CNavPolygonInfo3>& outPolygonInfo3,
        std::vector<uint32_t>& outAdjacentAreaIds,
        Vector3& outExtents,
        uint32_t& outAreaId,
        uint32_t& outBuildId,
        ANavXmlMetadata& outXmlMetadata,
        std::string& outError) {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(filePath.c_str());
        if (!result) {
            outError = "XML parse error: " + std::string(result.description());
            return false;
        }

        pugi::xml_node root = doc.child("RDR2Navmesh");
        if (!root) {
            outError = "Missing root node 'RDR2Navmesh'.";
            return false;
        }

        pugi::xml_node areaNode = root.child("AreaID");
        pugi::xml_node buildNode = root.child("BuildID");
        pugi::xml_node extentsNode = root.child("Extents");
        if (!areaNode || !buildNode || !extentsNode) {
            outError = "Missing one of required nodes: AreaID, BuildID, Extents.";
            return false;
        }

        outAreaId = areaNode.attribute("value").as_uint(0);
        outBuildId = buildNode.attribute("value").as_uint(0);
        outExtents = {
            extentsNode.attribute("x").as_float(0.0f),
            extentsNode.attribute("y").as_float(0.0f),
            extentsNode.attribute("z").as_float(0.0f)
        };

        outAdjacentAreaIds.clear();
        outXmlMetadata = ANavXmlMetadata{};
        outXmlMetadata.available = true;
        std::stringstream adjStream(root.child("AdjAreaIDs").text().as_string());
        uint32_t adjId = 0;
        while (adjStream >> adjId) {
            outAdjacentAreaIds.push_back(adjId);
        }

        pugi::xml_node polygonsNode = root.child("Polygons");
        if (!polygonsNode) {
            outError = "Missing 'Polygons' node.";
            return false;
        }

        outPolygons.clear();
        outPolygonFlags.clear();
        outPolygonInfo3.clear();
        for (pugi::xml_node polyItem : polygonsNode.children("Item")) {
            std::vector<Vector3> poly;

            pugi::xml_node verticesNode = polyItem.child("Vertices");
            for (pugi::xml_node v : verticesNode.children("Item")) {
                poly.push_back({
                    v.attribute("x").as_float(0.0f),
                    v.attribute("y").as_float(0.0f),
                    v.attribute("z").as_float(0.0f)
                });
            }

            if (poly.size() < 3) {
                continue;
            }

            const std::string flagsText = ReadValueOrText(polyItem.child("Flags"));
            outPolygons.push_back(poly);
            outPolygonFlags.push_back(ParsePolygonFlags(flagsText));
            outPolygonInfo3.push_back(ParsePolygonInfo3(polyItem, flagsText, outXmlMetadata));
        }

        outXmlMetadata.polygonCount = uint32_t(outPolygons.size());

        pugi::xml_node specialLinksNode = root.child("SpecialLinks");
        if (specialLinksNode) {
            for (pugi::xml_node linkItem : specialLinksNode.children("Item")) {
                outXmlMetadata.specialLinkCount++;
                const uint32_t typeValue = linkItem.child("Type").attribute("value").as_uint(0);
                outXmlMetadata.specialLinkTypeCounts[typeValue]++;
            }
        }

        if (outPolygons.empty()) {
            outError = "No polygon entries with at least 3 vertices were found.";
            return false;
        }

        outError.clear();
        return true;
    }

    Rsc8HeaderInfo ReadRsc8Header(const std::filesystem::path& filePath) {
        Rsc8HeaderInfo info;
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            return info;
        }

        uint8_t header[16]{};
        file.read(reinterpret_cast<char*>(header), sizeof(header));
        if (file.gcount() < 16) {
            return info;
        }

        const bool rsc8 =
            (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == '8');
        if (!rsc8) {
            return info;
        }

        uint32_t version =
            uint32_t(header[4]) |
            (uint32_t(header[5]) << 8) |
            (uint32_t(header[6]) << 16) |
            (uint32_t(header[7]) << 24);
        uint32_t virtualFlags =
            uint32_t(header[8]) |
            (uint32_t(header[9]) << 8) |
            (uint32_t(header[10]) << 16) |
            (uint32_t(header[11]) << 24);
        uint32_t physicalFlags =
            uint32_t(header[12]) |
            (uint32_t(header[13]) << 8) |
            (uint32_t(header[14]) << 16) |
            (uint32_t(header[15]) << 24);

        info.valid = true;
        info.version = version;
        info.virtualFlags = virtualFlags;
        info.physicalFlags = physicalFlags;
        info.compressorId = uint8_t(((version >> 8) & 0x1F) + 1);
        return info;
    }
}

ANavmeshRenderData::ANavmeshRenderData() : mNavIndexCount(0), mNavVBO(0), mNavIBO(0), mNavVAO(0) {

}

ANavmeshRenderData::~ANavmeshRenderData() {
    DeleteNavResources();
}

bool ANavmeshRenderData::CreateNavResources(const std::shared_ptr<CNavmeshData>& data, uint32_t highlightMode, bool hideNonMatching, std::string& outError) {
    DeleteNavResources();

    if (!data) {
        outError = "Import returned no navmesh data.";
        mNavIndexCount = 0;
        return false;
    }

    uint32_t numVertices = 0;
    float* vertexData = nullptr;
    uint32_t* indexData = nullptr;
    data->GetVerticesFlagColored(vertexData, indexData, numVertices, mNavIndexCount, highlightMode, hideNonMatching);

    if (numVertices == 0 || mNavIndexCount == 0) {
        delete[] vertexData;
        delete[] indexData;
        outError = "Navmesh has no vertices or indices.";
        mNavIndexCount = 0;
        return false;
    }

    if (vertexData == nullptr || indexData == nullptr) {
        delete[] vertexData;
        delete[] indexData;
        outError = "Navmesh vertex/index buffers are empty.";
        mNavIndexCount = 0;
        return false;
    }

    //data->GetIndicesForPolys(indexData, mNavIndexCount);

    glCreateBuffers(1, &mNavVBO);
    glCreateBuffers(1, &mNavIBO);

    glNamedBufferStorage(mNavVBO, numVertices    * (sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3)), vertexData, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    glNamedBufferStorage(mNavIBO, mNavIndexCount * sizeof(uint32_t),  indexData,  GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

    delete[] vertexData;
    delete[] indexData;

    glCreateVertexArrays(1, &mNavVAO);
    glVertexArrayVertexBuffer(mNavVAO, 0, mNavVBO, 0, (sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3)));
    glVertexArrayElementBuffer(mNavVAO, mNavIBO);

    glEnableVertexArrayAttrib(mNavVAO,  VERTEX_ATTRIB_INDEX);
    glVertexArrayAttribBinding(mNavVAO, VERTEX_ATTRIB_INDEX, 0);
    glVertexArrayAttribFormat(mNavVAO,  VERTEX_ATTRIB_INDEX, glm::vec3::length(), GL_FLOAT, GL_FALSE, 0);

    glEnableVertexArrayAttrib(mNavVAO, NORMAL_ATTRIB_INDEX);
    glVertexArrayAttribBinding(mNavVAO, NORMAL_ATTRIB_INDEX, 0);
    glVertexArrayAttribFormat(mNavVAO, NORMAL_ATTRIB_INDEX, glm::vec3::length(), GL_FLOAT, GL_FALSE, sizeof(glm::vec3));

    glEnableVertexArrayAttrib(mNavVAO, COLOR_ATTRIB_INDEX);
    glVertexArrayAttribBinding(mNavVAO, COLOR_ATTRIB_INDEX, 0);
    glVertexArrayAttribFormat(mNavVAO, COLOR_ATTRIB_INDEX, glm::vec3::length(), GL_FLOAT, GL_FALSE, sizeof(glm::vec3) + sizeof(glm::vec3));

    return true;
}

void ANavmeshRenderData::DeleteNavResources() {
    uint32_t buffers[]{ mNavVBO, mNavIBO };

    glDeleteBuffers(2, buffers);
    glDeleteVertexArrays(1, &mNavVAO);

    mNavVBO = 0;
    mNavIBO = 0;
    mNavVAO = 0;
}

void ANavmeshRenderData::Render(bool outlineHighlight) {
    glBindVertexArray(mNavVAO);

    ULitSimpleUniformBuffer::SetAmbientColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
    ULitSimpleUniformBuffer::SubmitUBO();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    //glPolygonOffset(-5.0f, 1.0f);

    glDrawElements(GL_TRIANGLES, mNavIndexCount, GL_UNSIGNED_INT, 0);

    if (outlineHighlight) {
        ULitSimpleUniformBuffer::SetAmbientColor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        ULitSimpleUniformBuffer::SubmitUBO();

        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glLineWidth(2.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawElements(GL_TRIANGLES, mNavIndexCount, GL_UNSIGNED_INT, 0);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);

        ULitSimpleUniformBuffer::SetAmbientColor(glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
        ULitSimpleUniformBuffer::SubmitUBO();
    }

    //ULitSimpleUniformBuffer::SetAmbientColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    //ULitSimpleUniformBuffer::SubmitUBO();

    //glDisable(GL_BLEND);

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glPolygonOffset(-10.0f, 1.0f);

    //glDrawElements(GL_TRIANGLES, mNavIndexCount, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}

ANavContext::ANavContext() :
    mHighlightMode(NAV_HIGHLIGHT_ALL_FLAGS),
    mHideNonMatchingFaces(false),
    mOutlineHighlight(false),
    mLitSimpleProgram(0) {

}

ANavContext::~ANavContext() {
    glDeleteProgram(mLitSimpleProgram);
    mLitSimpleProgram = 0;
}

bool ANavContext::LoadNavmesh(std::filesystem::path filePath) {
    mLastLoadError.clear();
    mLastLoadInfo.clear();

    try {
        std::shared_ptr<CNavmeshData> imported;
        ANavXmlMetadata loadedXmlMetadata;
        bool usedXmlFallback = false;
        std::string xmlFallbackSource;

        auto BuildFromXmlFile = [this, &loadedXmlMetadata](
            const std::filesystem::path& xmlPath,
            std::shared_ptr<CNavmeshData>& outData,
            std::string& outError) -> bool {
            std::vector<std::vector<Vector3>> polygons;
            std::vector<uint8_t> polygonFlags;
            std::vector<CNavPolygonInfo3> polygonInfo3;
            std::vector<uint32_t> adjacentAreaIds;
            Vector3 extents{0.0f, 0.0f, 0.0f};
            uint32_t areaId = 0;
            uint32_t buildId = 0;
            std::string parseError;
            ANavXmlMetadata xmlMetadata;

            if (!TryParseYnvXml(xmlPath, polygons, polygonFlags, polygonInfo3, adjacentAreaIds, extents, areaId, buildId, xmlMetadata, parseError)) {
                outError = "Failed to parse navmesh XML '" + xmlPath.filename().string() + "': " + parseError;
                return false;
            }

            outData = std::make_shared<CNavmeshData>();
            std::string buildError;
            if (!outData->BuildFromPolygonSoup(polygons, polygonFlags, polygonInfo3, adjacentAreaIds, extents, areaId, buildId, buildError)) {
                outError = "Failed to build navmesh from XML '" + xmlPath.filename().string() + "': " + buildError;
                return false;
            }

            loadedXmlMetadata = xmlMetadata;

            return true;
        };

        const bool isYnvXml =
            filePath.extension() == ".xml" &&
            filePath.stem().extension() == ".ynv";

        if (isYnvXml) {
            std::string xmlError;
            if (!BuildFromXmlFile(filePath, imported, xmlError)) {
                mLastLoadError = xmlError;
                return false;
            }
        }
        else {
            imported = librdr3::ImportYnv(filePath.generic_string());
            if (!imported) {
                const std::string importError = librdr3::GetLastImportError();
                if (!importError.empty()) {
                    mLastLoadError = "Failed to load native navmesh '" + filePath.filename().string() + "': " + importError;
                }

                const Rsc8HeaderInfo rsc8Info = ReadRsc8Header(filePath);
                if (mLastLoadError.empty() && rsc8Info.valid) {
                    std::ostringstream oss;
                    oss << "Failed to load native navmesh '" << filePath.filename().string() << "': file uses RSC8 container (compressor "
                        << int(rsc8Info.compressorId)
                        << "). Current importer could not decode this payload.";
                    if (rsc8Info.compressorId == 2) {
                        oss << " Compressor 2 is Oodle/Kraken; place oo2core_5_win64.dll near navigator.exe or set OODLE_DLL_PATH.";
                    }
                    mLastLoadError = oss.str();
                }

                if (mLastLoadError.empty()) {
                    mLastLoadError = "Failed to load native navmesh '" + filePath.filename().string() + "': unknown import failure.";
                }

                const std::filesystem::path xmlFallbackPath = filePath.string() + ".xml";
                if (std::filesystem::exists(xmlFallbackPath)) {
                    std::shared_ptr<CNavmeshData> xmlImported;
                    std::string xmlError;
                    if (BuildFromXmlFile(xmlFallbackPath, xmlImported, xmlError)) {
                        imported = xmlImported;
                        usedXmlFallback = true;
                        xmlFallbackSource = xmlFallbackPath.filename().string();
                        mLastLoadError.clear();
                    }
                    else {
                        mLastLoadError += " XML fallback failed: " + xmlError;
                    }
                }
            }
        }

        std::shared_ptr<ANavmeshRenderData> newData = std::make_shared<ANavmeshRenderData>();
        std::string validationError;
        if (!newData->CreateNavResources(imported, mHighlightMode, mHideNonMatchingFaces, validationError)) {
            if (mLastLoadError.empty()) {
                mLastLoadError = "Failed to load navmesh '" + filePath.filename().string() + "': " + validationError;
            }
            return false;
        }

        mLoadedNavmeshes.push_back(newData);
        mLoadedNavmeshData.push_back(imported);
        mLoadedNavmeshPaths.push_back(filePath);
        mLoadedNavmeshLabels.push_back(filePath.filename().string());
        mLoadedNavmeshXmlMetadata.push_back(loadedXmlMetadata);
        mLoadedNavmeshDirty.push_back(false);
        if (usedXmlFallback) {
            mLastLoadInfo = "Loaded navmesh '" + filePath.filename().string() + "' via XML fallback ('" + xmlFallbackSource + "') (total: " + std::to_string(mLoadedNavmeshes.size()) + ").";
        }
        else {
            mLastLoadInfo = "Loaded navmesh '" + filePath.filename().string() + "' (total: " + std::to_string(mLoadedNavmeshes.size()) + ").";
        }
        return true;
    }
    catch (const std::exception& ex) {
        mLastLoadError = "Exception while loading navmesh '" + filePath.filename().string() + "': " + ex.what();
    }
    catch (...) {
        mLastLoadError = "Unknown exception while loading navmesh '" + filePath.filename().string() + "'.";
    }

    return false;
}

void ANavContext::ClearLoadedNavmeshes() {
    mLoadedNavmeshes.clear();
    mLoadedNavmeshData.clear();
    mLoadedNavmeshPaths.clear();
    mLoadedNavmeshLabels.clear();
    mLoadedNavmeshXmlMetadata.clear();
    mLoadedNavmeshDirty.clear();
    mLastLoadError.clear();
    mLastLoadInfo = "Cleared loaded navmeshes.";
}

bool ANavContext::RemoveLoadedNavmesh(size_t index) {
    if (index >= mLoadedNavmeshes.size() ||
        index >= mLoadedNavmeshData.size() ||
        index >= mLoadedNavmeshPaths.size() ||
        index >= mLoadedNavmeshLabels.size() ||
        index >= mLoadedNavmeshXmlMetadata.size() ||
        index >= mLoadedNavmeshDirty.size()) {
        return false;
    }

    const std::string removedName = mLoadedNavmeshLabels[index];
    mLoadedNavmeshes.erase(mLoadedNavmeshes.begin() + index);
    mLoadedNavmeshData.erase(mLoadedNavmeshData.begin() + index);
    mLoadedNavmeshPaths.erase(mLoadedNavmeshPaths.begin() + index);
    mLoadedNavmeshLabels.erase(mLoadedNavmeshLabels.begin() + index);
    mLoadedNavmeshXmlMetadata.erase(mLoadedNavmeshXmlMetadata.begin() + index);
    mLoadedNavmeshDirty.erase(mLoadedNavmeshDirty.begin() + index);

    mLastLoadError.clear();
    mLastLoadInfo = "Removed navmesh '" + removedName + "'.";
    return true;
}

const std::string& ANavContext::GetLoadedNavmeshLabel(size_t index) const {
    static const std::string EMPTY_LABEL;
    if (index >= mLoadedNavmeshLabels.size()) {
        return EMPTY_LABEL;
    }

    return mLoadedNavmeshLabels[index];
}

std::shared_ptr<CNavmeshData> ANavContext::GetLoadedNavmeshData(size_t index) const {
    if (index >= mLoadedNavmeshData.size()) {
        return nullptr;
    }

    return mLoadedNavmeshData[index];
}

const ANavXmlMetadata* ANavContext::GetLoadedNavmeshXmlMetadata(size_t index) const {
    if (index >= mLoadedNavmeshXmlMetadata.size()) {
        return nullptr;
    }

    return &mLoadedNavmeshXmlMetadata[index];
}

bool ANavContext::IsLoadedNavmeshDirty(size_t index) const {
    if (index >= mLoadedNavmeshDirty.size()) {
        return false;
    }

    return mLoadedNavmeshDirty[index];
}

void ANavContext::SetLoadedNavmeshDirty(size_t index, bool dirty) {
    if (index >= mLoadedNavmeshDirty.size()) {
        return;
    }

    mLoadedNavmeshDirty[index] = dirty;
    if (!dirty) {
        return;
    }

    if (index >= mLoadedNavmeshData.size() || index >= mLoadedNavmeshes.size() || index >= mLoadedNavmeshLabels.size()) {
        return;
    }

    std::string rebuildError;
    if (!mLoadedNavmeshes[index]->CreateNavResources(mLoadedNavmeshData[index], mHighlightMode, mHideNonMatchingFaces, rebuildError)) {
        mLastLoadError = "Failed to refresh navmesh render data for '" + mLoadedNavmeshLabels[index] + "': " + rebuildError;
    }
}

void ANavContext::SetHighlightMode(uint32_t mode) {
    if (mHighlightMode == mode) {
        return;
    }

    mHighlightMode = mode;
    for (size_t i = 0; i < mLoadedNavmeshData.size() && i < mLoadedNavmeshes.size(); i++) {
        std::string rebuildError;
        if (!mLoadedNavmeshes[i]->CreateNavResources(mLoadedNavmeshData[i], mHighlightMode, mHideNonMatchingFaces, rebuildError)) {
            mLastLoadError = "Failed to rebuild navmesh highlight for '" + mLoadedNavmeshLabels[i] + "': " + rebuildError;
            return;
        }
    }

    mLastLoadInfo = "Updated navmesh highlight mode.";
}

void ANavContext::SetHideNonMatchingFaces(bool hide) {
    if (mHideNonMatchingFaces == hide) {
        return;
    }

    mHideNonMatchingFaces = hide;
    for (size_t i = 0; i < mLoadedNavmeshData.size() && i < mLoadedNavmeshes.size(); i++) {
        std::string rebuildError;
        if (!mLoadedNavmeshes[i]->CreateNavResources(mLoadedNavmeshData[i], mHighlightMode, mHideNonMatchingFaces, rebuildError)) {
            mLastLoadError = "Failed to rebuild navmesh visibility filter for '" + mLoadedNavmeshLabels[i] + "': " + rebuildError;
            return;
        }
    }

    mLastLoadInfo = mHideNonMatchingFaces ?
        "Highlight filter now hides non-matching faces." :
        "Highlight filter now dims non-matching faces.";
}

void ANavContext::SetOutlineHighlight(bool enabled) {
    mOutlineHighlight = enabled;
    mLastLoadInfo = mOutlineHighlight ?
        "Outline highlight enabled." :
        "Outline highlight disabled.";
}

void ANavContext::Render(ASceneCamera& camera) {
    if (mLoadedNavmeshes.size() == 0) {
        return;
    }

    UCommonUniformBuffer::SetProjAndViewMatrices(camera.GetProjectionMatrix(), camera.GetViewMatrix());
    UCommonUniformBuffer::SetModelMatrix(glm::identity<glm::mat4>());
    UCommonUniformBuffer::SubmitUBO();

    ULitSimpleUniformBuffer::SetLight(glm::vec4(0.0f, 10000.0f, 0.0f, 1.0f), glm::vec4(0.75f, 0.75f, 0.75f, 1.0f), 1.0f, 0.0f);
    ULitSimpleUniformBuffer::SetViewPos(glm::vec4(camera.GetPosition(), 1.0f));

    glUseProgram(mLitSimpleProgram);

    for (std::shared_ptr<ANavmeshRenderData> r : mLoadedNavmeshes) {
        r->Render(mOutlineHighlight);
    }

    glUseProgram(0);
}

void ANavContext::OnGLInitialized() {
    try {
        // Compile vertex shader
        std::cout << "Loading vertex shader..." << std::endl;
        std::string vertTxt = UFileUtil::LoadShaderText("lit_simple.vert");
        if (vertTxt.empty()) {
            std::cerr << "Error: lit_simple.vert is empty or not found" << std::endl;
            throw std::runtime_error("Failed to load lit_simple.vert");
        }
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
            if (logSize > 0) {
                glGetShaderInfoLog(vertHandle, logSize, nullptr, &log[0]);
                std::cout << "Vertex shader compilation error: " << std::string(log.data()) << std::endl;
            }

            glDeleteShader(vertHandle);
            throw std::runtime_error("Failed to compile vertex shader");
        }

        // Compile fragment shader
        std::cout << "Loading fragment shader..." << std::endl;
        std::string fragTxt = UFileUtil::LoadShaderText("lit_simple.frag");
        if (fragTxt.empty()) {
            std::cerr << "Error: lit_simple.frag is empty or not found" << std::endl;
            throw std::runtime_error("Failed to load lit_simple.frag");
        }
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
            if (logSize > 0) {
                glGetShaderInfoLog(fragHandle, logSize, nullptr, &log[0]);
                std::cout << "Fragment shader compilation error: " << std::string(log.data()) << std::endl;
            }

            glDeleteShader(fragHandle);
            throw std::runtime_error("Failed to compile fragment shader");
        }

        // Generate shader program
        std::cout << "Linking shader program..." << std::endl;
        mLitSimpleProgram = glCreateProgram();
        glAttachShader(mLitSimpleProgram, vertHandle);
        glAttachShader(mLitSimpleProgram, fragHandle);
        glLinkProgram(mLitSimpleProgram);

        // Clean up
        glDetachShader(mLitSimpleProgram, vertHandle);
        glDetachShader(mLitSimpleProgram, fragHandle);
        glDeleteShader(vertHandle);
        glDeleteShader(fragHandle);

        UCommonUniformBuffer::LinkShaderToUBO(mLitSimpleProgram);
        ULitSimpleUniformBuffer::LinkShaderToUBO(mLitSimpleProgram);
        
        std::cout << "Shader program created successfully" << std::endl;
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception in ANavContext::OnGLInitialized: " << ex.what() << std::endl;
        throw;
    }
}
