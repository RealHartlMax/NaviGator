#pragma once

#include "types.h"
#include <functional>
#include <imgui.h>

namespace UTracks {
    class UTrackPoint;
}

enum class EPropertyPanelSection {
    Position,
    Station,
    Curve,
    Advanced
};

class CPropertiesPanel {
public:
    CPropertiesPanel();
    ~CPropertiesPanel();

    // Main rendering function
    void Draw(ImGuiID dockspaceId);

    // Update selected object
    void SetSelectedNode(std::shared_ptr<UTracks::UTrackPoint> node);
    void ClearSelection();

    // Callbacks for external updates
    using PositionChangedCallback = std::function<void(const glm::vec3&)>;
    using FlagChangedCallback = std::function<void(bool)>;
    using StationTypeChangedCallback = std::function<void(int)>;

    void SetOnPositionChanged(PositionChangedCallback cb) { mOnPositionChanged = cb; }
    void SetOnCurveChanged(FlagChangedCallback cb) { mOnCurveChanged = cb; }
    void SetOnStationTypeChanged(StationTypeChangedCallback cb) { mOnStationTypeChanged = cb; }
    void SetOnTunnelChanged(FlagChangedCallback cb) { mOnTunnelChanged = cb; }
    void SetOnJunctionChanged(FlagChangedCallback cb) { mOnJunctionChanged = cb; }

    std::shared_ptr<UTracks::UTrackPoint> GetSelectedNode() const { return mSelectedNode; }

private:
    std::shared_ptr<UTracks::UTrackPoint> mSelectedNode;
    glm::vec3 mEditPosition{};
    
    // Callbacks
    PositionChangedCallback mOnPositionChanged;
    FlagChangedCallback mOnCurveChanged;
    StationTypeChangedCallback mOnStationTypeChanged;
    FlagChangedCallback mOnTunnelChanged;
    FlagChangedCallback mOnJunctionChanged;

    // Drawing sections
    void DrawPositionProperties();
    void DrawStationProperties();
    void DrawCurveProperties();
    void DrawAdvancedProperties();
    void DrawSeparator();
};
