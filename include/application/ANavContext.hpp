#pragma once

#include "types.h"
#include "application/ACamera.hpp"

#include <librdr3.hpp>

struct ANavmeshRenderData {
    uint32_t mNavIndexCount;
    uint32_t mNavVBO, mNavIBO, mNavVAO;

    bool CreateNavResources(const std::shared_ptr<CNavmeshData>& data, uint32_t highlightMode, bool hideNonMatching, std::string& outError);
    void DeleteNavResources();

    ANavmeshRenderData();
    ~ANavmeshRenderData();

    void Render(bool outlineHighlight);
};

struct ANavXmlMetadata {
    bool available = false;
    uint32_t polygonCount = 0;
    uint32_t networkSpawnCandidateCount = 0;
    uint32_t roadPolygonCount = 0;
    uint32_t interiorPolygonCount = 0;
    uint32_t isolatedPolygonCount = 0;
    uint32_t shallowWaterPolygonCount = 0;
    uint32_t specialLinkCount = 0;
    std::map<uint32_t, uint32_t> specialLinkTypeCounts;
};

class ANavContext {
    std::vector<std::shared_ptr<ANavmeshRenderData>> mLoadedNavmeshes;
    std::vector<std::shared_ptr<CNavmeshData>> mLoadedNavmeshData;
    std::vector<std::string> mLoadedNavmeshLabels;
    std::vector<std::filesystem::path> mLoadedNavmeshPaths;
    std::vector<ANavXmlMetadata> mLoadedNavmeshXmlMetadata;
    std::vector<bool> mLoadedNavmeshDirty;
    std::string mLastLoadError;
    std::string mLastLoadInfo;
    uint32_t mHighlightMode;
    bool mHideNonMatchingFaces;
    bool mOutlineHighlight;
    uint32_t mLitSimpleProgram;
    float f = 0;

public:
    ANavContext();
    ~ANavContext();

    bool LoadNavmesh(std::filesystem::path filePath);
    void ClearLoadedNavmeshes();
    bool RemoveLoadedNavmesh(size_t index);

    size_t GetLoadedNavmeshCount() const { return mLoadedNavmeshes.size(); }
    const std::string& GetLoadedNavmeshLabel(size_t index) const;
    std::shared_ptr<CNavmeshData> GetLoadedNavmeshData(size_t index) const;
    const ANavXmlMetadata* GetLoadedNavmeshXmlMetadata(size_t index) const;
    bool IsLoadedNavmeshDirty(size_t index) const;
    void SetLoadedNavmeshDirty(size_t index, bool dirty);
    uint32_t GetHighlightMode() const { return mHighlightMode; }
    void SetHighlightMode(uint32_t mode);
    bool GetHideNonMatchingFaces() const { return mHideNonMatchingFaces; }
    void SetHideNonMatchingFaces(bool hide);
    bool GetOutlineHighlight() const { return mOutlineHighlight; }
    void SetOutlineHighlight(bool enabled);
    const std::string& GetLastLoadError() const { return mLastLoadError; }
    const std::string& GetLastLoadInfo() const { return mLastLoadInfo; }

    void Render(ASceneCamera& camera);
    
    void OnGLInitialized();
};