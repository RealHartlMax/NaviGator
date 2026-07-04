#pragma once

#include "types.h"
#include <functional>
#include <imgui.h>

namespace UTracks {
    class UTrackPoint;
    class UTrack;
}

class CHierarchyView {
public:
    CHierarchyView();
    ~CHierarchyView();

    // Main rendering function
    void Draw(ImGuiID dockspaceId);

    // Set the track to display
    void SetTrack(const shared_vector<UTracks::UTrackPoint>& points);
    void Clear();

    // Selection callback
    using SelectionCallback = std::function<void(std::shared_ptr<UTracks::UTrackPoint>)>;
    void SetOnSelectionChanged(SelectionCallback cb) { mOnSelectionChanged = cb; }

    std::shared_ptr<UTracks::UTrackPoint> GetSelectedNode() const { return mSelectedNode; }
    size_t GetSelectedIndex() const { return mSelectedIndex; }

    // Refresh when data changes
    void RefreshDisplay();

private:
    shared_vector<UTracks::UTrackPoint> mPoints;
    std::shared_ptr<UTracks::UTrackPoint> mSelectedNode;
    size_t mSelectedIndex = 0;
    
    SelectionCallback mOnSelectionChanged;

    // UI state
    bool bExpandTrack = true;
    char mSearchBuffer[256] = {};
    bool bShowOnlyCurve = false;
    bool bShowOnlyLinear = false;

    // Drawing
    void DrawTreeItems();
    void UpdateSearch();
};
