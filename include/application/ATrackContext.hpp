#pragma once

#include "types.h"
#include "application/ACamera.hpp"

namespace UTracks {
    class UTrack;
    class UTrackPoint;
    enum ENodeStationType : uint8_t;
}

enum ETrackNodePickType : uint8_t {
    Position,
    Handle_A,
    Handle_B
};

class CPathRenderer;

struct APointSelection {
    uint16_t TrackIdx, PointIdx;

    void Get(uint16_t& trackIdx, uint16_t& pointIdx) {
        trackIdx = TrackIdx;
        pointIdx = PointIdx;
    }
};

class ATrackContext {
    struct APointState {
        glm::vec3 Position;
        glm::vec3 HandleA;
        glm::vec3 HandleB;
        float Scalar;
        UTracks::ENodeStationType StationType;
        bool IsTunnel;
        bool IsJunction;
        bool IsCurve;
        std::string Argument;
        std::string ParentTrackName;
        int JunctionTrackIdx;
        int JunctionPointIdx;
    };

    struct ATrackState {
        std::string ConfigName;
        std::string GameFilename;
        bool StopsAtStations;
        bool Loops;
        bool Hidden;
        uint32_t BrakingDist;
        std::vector<APointState> Points;
    };

    struct AEditorSnapshot {
        std::vector<ATrackState> Tracks;
        std::vector<APointSelection> Selection;
        ETrackNodePickType PickType;
    };

    shared_vector<UTracks::UTrack> mTracks;

    using points_vector = std::vector<shared_vector<UTracks::UTrackPoint>>;
    points_vector mTrackPoints;

    shared_vector<CPathRenderer> mPathRenderers;

    bool bGLInitialized;
    uint32_t mPntVBO, mPntIBO, mPntVAO, mSimpleProgram, mBaseColorUniform;

    std::weak_ptr<UTracks::UTrack> mSelectedTrack;
    std::vector<APointSelection> mSelectedPoints;

    ETrackNodePickType mSelectedPickType;

    std::string mPendingNewTrackName;
    bool bTrackDialogOpen;
    bool bCanDuplicatePoint;
    bool bWasUsingGizmo;
    bool bBoxSelecting;
    bool bBoxSelectionActive;
    bool bBoxSelectionRequireFullContainment;
    float mBoxSelectionNodePickRadius;
    glm::vec2 mBoxSelectionStartScreen;
    glm::vec2 mBoxSelectionEndScreen;

    std::vector<AEditorSnapshot> mUndoStack;
    std::vector<AEditorSnapshot> mRedoStack;
    static constexpr size_t MAX_HISTORY_STATES = 64;

    bool bSelectingJunctionPartner;

    void InitSimpleShader();
    void DestroyGLResources();

    void RenderTrackDataEditor(std::shared_ptr<UTracks::UTrack> track);
    void RenderPointDataEditorSingle(std::shared_ptr<UTracks::UTrackPoint> point, uint16_t trackIdx, uint16_t pointIdx);
    void RenderPointDataEditorMulti();

    void RenderNewTrackDialog();

    void RenderPickingBuffer(ASceneCamera& camera);
    void HandleUndoRedoShortcuts();
    void HandleViewportMouse(ASceneCamera& camera, const glm::vec2& viewportPos, const glm::vec2& viewportSize);
    void ApplyBoxSelection(ASceneCamera& camera, const glm::vec2& viewportPos, const glm::vec2& viewportSize, bool additive, bool toggle);
    void DrawBoxSelectionOverlay() const;
    bool ProjectPointToViewport(ASceneCamera& camera, const glm::vec3& position, const glm::vec2& viewportPos, const glm::vec2& viewportSize, glm::vec2& outScreenPos) const;
    bool IsPointSelected(uint16_t trackIdx, uint16_t pointIdx) const;
    void RecalculateCurveNeighborhoodForSelection();

    void PostprocessNodes();

    void SetCurveState(uint16_t trackIdx, uint16_t pointIdx, bool isCurve);
    void RecalculateCurveHandles(uint16_t trackIdx, uint16_t pointIdx);
    void ClearSelectedPoints();
    void SyncPathRenderersFromTrackPoints();
    void InsertNodeRelative(uint16_t trackIdx, uint16_t pointIdx, bool insertAfter);
    void DeleteSelectedNode(uint16_t trackIdx, uint16_t pointIdx);

    AEditorSnapshot CaptureSnapshot() const;
    void RestoreSnapshot(const AEditorSnapshot& snapshot);
    void PushUndoSnapshot();
    bool Undo();
    bool Redo();

public:
    ATrackContext();
    ~ATrackContext();

    void InitGLResources();

    void RenderTreeView();
    void RenderDataEditor();
    void Render(ASceneCamera& camera);
    void RenderUI(ASceneCamera& camera, const glm::vec2& viewportPos, const glm::vec2& viewportSize);

    void OnMouseHover(ASceneCamera& camera, int32_t pX, int32_t pY);
    void OnMouseClick(ASceneCamera& camera, int32_t pX, int32_t pY);

    void LoadTracks(std::filesystem::path filePath);
    void SaveTracks(std::filesystem::path dirPath);

    bool IsLoaded() const { return mTracks.size() != 0; }
    
    // Get all track points from the first track (for UI panels)
    shared_vector<UTracks::UTrackPoint> GetAllTrackPoints() const {
        if (mTrackPoints.empty() || mTrackPoints[0].empty()) {
            return shared_vector<UTracks::UTrackPoint>();
        }
        return mTrackPoints[0];
    }
};
