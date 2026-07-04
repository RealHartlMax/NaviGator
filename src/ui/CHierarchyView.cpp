#include "ui/CHierarchyView.hpp"
#include "tracks/UTrackPoint.hpp"
#include <imgui.h>
#include <cstring>

CHierarchyView::CHierarchyView()
    : mSelectedNode(nullptr), mSelectedIndex(0) {
}

CHierarchyView::~CHierarchyView() {
}

void CHierarchyView::Draw(ImGuiID dockspaceId) {
    if (ImGui::Begin("##HierarchyView", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        if (!mPoints.empty()) {
            ImGui::Text("Track Points");
            ImGui::Separator();
            ImGui::Spacing();
            
            // Search bar
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##search", mSearchBuffer, sizeof(mSearchBuffer))) {
                UpdateSearch();
            }

            ImGui::Spacing();

            // Filter buttons
            if (ImGui::SmallButton("Curve")) {
                bShowOnlyCurve = !bShowOnlyCurve;
                bShowOnlyLinear = false;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Linear")) {
                bShowOnlyLinear = !bShowOnlyLinear;
                bShowOnlyCurve = false;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("All")) {
                bShowOnlyCurve = false;
                bShowOnlyLinear = false;
            }

            ImGui::Separator();

            // Tree display
            if (ImGui::TreeNodeEx("Track", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoAutoOpenOnLog)) {
                DrawTreeItems();
                ImGui::TreePop();
            }
        } else {
            ImGui::Spacing();
            ImGui::TextDisabled("No track loaded");
        }
        ImGui::End();
    }
}

void CHierarchyView::SetTrack(const shared_vector<UTracks::UTrackPoint>& points) {
    mPoints = points;
    mSelectedNode = nullptr;
    mSelectedIndex = 0;
    memset(mSearchBuffer, 0, sizeof(mSearchBuffer));
    RefreshDisplay();
}

void CHierarchyView::Clear() {
    mPoints.clear();
    mSelectedNode = nullptr;
    mSelectedIndex = 0;
}

void CHierarchyView::RefreshDisplay() {
    // Trigger redraw by marking as dirty
    // ImGui will handle redraw automatically next frame
}

void CHierarchyView::DrawTreeItems() {
    for (size_t i = 0; i < mPoints.size(); i++) {
        auto point = mPoints[i];

        // Apply filters
        if (bShowOnlyCurve && !point->IsCurve()) continue;
        if (bShowOnlyLinear && point->IsCurve()) continue;

        // Apply search filter
        if (mSearchBuffer[0] != '\0') {
            char label[256];
            snprintf(label, sizeof(label), "Point_%03zu", i);
            if (strstr(label, mSearchBuffer) == nullptr) {
                continue;
            }
        }

        // Build label
        const char* typeStr = point->IsCurve() ? "[C]" : "[L]";
        const char* stationType = "";
        switch (point->GetStationType()) {
            case UTracks::ENodeStationType::None: stationType = ""; break;
            case UTracks::ENodeStationType::Left_Side: stationType = " (L-Station)"; break;
            case UTracks::ENodeStationType::Right_Side: stationType = " (R-Station)"; break;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (mSelectedNode == point) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        char label[256];
        snprintf(label, sizeof(label), "%s Point_%03zu%s", typeStr, i, stationType);

        ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", label);

        // Handle selection
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            mSelectedNode = point;
            mSelectedIndex = i;
            if (mOnSelectionChanged) {
                mOnSelectionChanged(point);
            }
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Select")) {
                mSelectedNode = point;
                mSelectedIndex = i;
                if (mOnSelectionChanged) {
                    mOnSelectionChanged(point);
                }
            }
            ImGui::EndPopup();
        }
    }
}

void CHierarchyView::UpdateSearch() {
    // Search updates are applied in DrawTreeItems
}
