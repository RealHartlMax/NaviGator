#include "ui/CPropertiesPanel.hpp"
#include "tracks/UTrackPoint.hpp"
#include <imgui.h>

CPropertiesPanel::CPropertiesPanel()
    : mSelectedNode(nullptr), mEditPosition(0.0f) {
}

CPropertiesPanel::~CPropertiesPanel() {
}

void CPropertiesPanel::Draw(ImGuiID dockspaceId) {
    if (ImGui::Begin("##PropertiesPanel", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        if (mSelectedNode) {
            ImGui::Text("Properties");
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::CollapsingHeader("Position##header", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawPositionProperties();
            }

            DrawSeparator();

            if (ImGui::CollapsingHeader("Station##header", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawStationProperties();
            }

            DrawSeparator();

            if (ImGui::CollapsingHeader("Curve##header", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawCurveProperties();
            }

            DrawSeparator();

            if (ImGui::CollapsingHeader("Advanced##header")) {
                DrawAdvancedProperties();
            }
        } else {
            ImGui::Spacing();
            ImGui::TextDisabled("No point selected");
            ImGui::TextDisabled("Click on a point in the");
            ImGui::TextDisabled("hierarchy to view properties");
        }
        ImGui::End();
    }
}

void CPropertiesPanel::SetSelectedNode(std::shared_ptr<UTracks::UTrackPoint> node) {
    mSelectedNode = node;
    if (node) {
        mEditPosition = node->GetPosition();
    }
}

void CPropertiesPanel::ClearSelection() {
    mSelectedNode = nullptr;
}

void CPropertiesPanel::DrawPositionProperties() {
    if (!mSelectedNode) return;

    ImGui::Text("Position");
    ImGui::Spacing();

    float pos[3] = {mEditPosition.x, mEditPosition.y, mEditPosition.z};
    bool changed = false;

    ImGui::SetNextItemWidth(-50);
    if (ImGui::DragFloat("##pos_x", &pos[0], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
        mEditPosition.x = pos[0];
        changed = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::Text("X");

    ImGui::SetNextItemWidth(-50);
    if (ImGui::DragFloat("##pos_y", &pos[1], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
        mEditPosition.y = pos[1];
        changed = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::Text("Y");

    ImGui::SetNextItemWidth(-50);
    if (ImGui::DragFloat("##pos_z", &pos[2], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
        mEditPosition.z = pos[2];
        changed = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::Text("Z");

    if (changed) {
        mSelectedNode->SetPosition(mEditPosition);
        if (mOnPositionChanged) {
            mOnPositionChanged(mEditPosition);
        }
    }
}

void CPropertiesPanel::DrawStationProperties() {
    if (!mSelectedNode) return;

    ImGui::Text("Station");
    ImGui::Spacing();

    // Station Type
    int stationType = static_cast<int>(mSelectedNode->GetStationType());
    const char* stationTypes[] = {"None", "Left Side", "Right Side"};

    if (ImGui::Combo("Type", &stationType, stationTypes, 3)) {
        mSelectedNode->GetStationTypeForEditor() = static_cast<UTracks::ENodeStationType>(stationType);
        if (mOnStationTypeChanged) {
            mOnStationTypeChanged(stationType);
        }
    }

    // Station Argument
    std::string* argument = mSelectedNode->GetArgumentForEditor();
    static char argBuffer[256] = {};
    strncpy_s(argBuffer, argument->c_str(), sizeof(argBuffer) - 1);

    if (ImGui::InputText("Station Name", argBuffer, sizeof(argBuffer))) {
        *argument = argBuffer;
    }
}

void CPropertiesPanel::DrawCurveProperties() {
    if (!mSelectedNode) return;

    ImGui::Text("Curve");
    ImGui::Spacing();

    // Is Curve flag
    bool* isCurve = mSelectedNode->GetIsCurveForEditor();
    bool oldCurve = *isCurve;

    if (ImGui::Checkbox("Is Curve Point", isCurve)) {
        if (mOnCurveChanged) {
            mOnCurveChanged(*isCurve);
        }
    }

    if (*isCurve) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Handle A
        ImGui::Text("Handle A");
        glm::vec3& handleA = mSelectedNode->GetHandleAForEditor();
        float handleA_arr[3] = {handleA.x, handleA.y, handleA.z};

        ImGui::SetNextItemWidth(-50);
        if (ImGui::DragFloat("##handle_a_x", &handleA_arr[0], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
            handleA.x = handleA_arr[0];
        }
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::Text("X");

        ImGui::SetNextItemWidth(-50);
        if (ImGui::DragFloat("##handle_a_y", &handleA_arr[1], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
            handleA.y = handleA_arr[1];
        }
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::Text("Y");

        ImGui::SetNextItemWidth(-50);
        if (ImGui::DragFloat("##handle_a_z", &handleA_arr[2], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
            handleA.z = handleA_arr[2];
        }
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::Text("Z");

        ImGui::Spacing();

        // Handle B
        ImGui::Text("Handle B");
        glm::vec3& handleB = mSelectedNode->GetHandleBForEditor();
        float handleB_arr[3] = {handleB.x, handleB.y, handleB.z};

        ImGui::SetNextItemWidth(-50);
        if (ImGui::DragFloat("##handle_b_x", &handleB_arr[0], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
            handleB.x = handleB_arr[0];
        }
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::Text("X");

        ImGui::SetNextItemWidth(-50);
        if (ImGui::DragFloat("##handle_b_y", &handleB_arr[1], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
            handleB.y = handleB_arr[1];
        }
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::Text("Y");

        ImGui::SetNextItemWidth(-50);
        if (ImGui::DragFloat("##handle_b_z", &handleB_arr[2], 0.1f, -999999.0f, 999999.0f, "%.2f")) {
            handleB.z = handleB_arr[2];
        }
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::Text("Z");
    }
}

void CPropertiesPanel::DrawAdvancedProperties() {
    if (!mSelectedNode) return;

    ImGui::Text("Advanced");
    ImGui::Spacing();

    // Tunnel flag
    bool* isTunnel = mSelectedNode->GetIsTunnelForEditor();
    if (ImGui::Checkbox("Is Tunnel", isTunnel)) {
        if (mOnTunnelChanged) {
            mOnTunnelChanged(*isTunnel);
        }
    }

    // Junction flag
    bool* isJunction = mSelectedNode->GetIsJunctionForEditor();
    if (ImGui::Checkbox("Is Junction", isJunction)) {
        if (mOnJunctionChanged) {
            mOnJunctionChanged(*isJunction);
        }
    }

    // Scalar value
    ImGui::Spacing();
    float* scalar = mSelectedNode->GetScalarForEditor();
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("Distance to Next", scalar, 0.1f, 0.0f, 999999.0f, "%.2f");
}

void CPropertiesPanel::DrawSeparator() {
    ImGui::Spacing();
}
