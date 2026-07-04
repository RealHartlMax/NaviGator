#pragma once

#include "types.h"
#include "application/ACamera.hpp"
#include "util/ImGuizmo.hpp"

class CPropertiesPanel;
class CHierarchyView;

namespace UTracks {
    class UTrackPoint;
}

class UViewport {
    ASceneCamera mCamera;
    std::string mViewportName;

    uint32_t mFBO;
    uint32_t mTexIds[2];

    glm::vec2 mViewportPos;
    glm::vec2 mViewportSize;

    bool bIsOpen;
    bool bShowGroundGrid = true;
    bool bShowNodeLegend = true;
    bool bShowEditorHints = true;
    float mGroundGridSize = 250.0f;
    
    // Gizmo settings
    ImGuizmo::MODE mGizmoMode = ImGuizmo::WORLD;
    ImGuizmo::OPERATION mGizmoOperation = ImGuizmo::TRANSLATE;

    // Grid rendering infrastructure
    uint32_t mGridVAO = 0;
    uint32_t mGridVBO = 0;
    uint32_t mGridShaderProgram = 0;
    uint32_t mGridLineCount = 0;
    bool bGridInitialized = false;

    // New panels
    CPropertiesPanel* mPropertiesPanel = nullptr;
    CHierarchyView* mHierarchyView = nullptr;
    std::shared_ptr<UTracks::UTrackPoint> mSelectedTrackPoint;

    void CreateFramebuffer();
    void ResizeViewport();
    void Clear();
    void InitializeGrid();
    void GenerateGridLines(float gridSize, std::vector<glm::vec3>& outLines);

public:
    UViewport() : UViewport("Viewport") { }
    UViewport(std::string name);
    ~UViewport();

    bool IsOpen() const { return bIsOpen; }

    glm::vec2 GetViewportSize() const { return mViewportSize; }
    glm::vec2 GetViewportPosition() const { return mViewportPos; }

    ASceneCamera& GetCamera() { return mCamera; }

    // Panel management
    void InitializePanels();
    void SetTrackPoints(const shared_vector<UTracks::UTrackPoint>& points);
    void SetSelectedTrackPoint(std::shared_ptr<UTracks::UTrackPoint> point);

    CPropertiesPanel* GetPropertiesPanel() const { return mPropertiesPanel; }
    CHierarchyView* GetHierarchyView() const { return mHierarchyView; }

    void BindViewport();
    void UnbindViewport();
    void RenderGridLines(); // Render grid lines in 3D space

    void RenderUI(float deltaTime);
    //void RenderScene(AJ3DContext* ctx, float deltaTime);
};