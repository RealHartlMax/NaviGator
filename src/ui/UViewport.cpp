#include "ui/UViewport.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "util/ImGuizmo.hpp"

#include <string>


constexpr int TEX_COLOR = 0;
constexpr int TEX_DEPTH = 1;

constexpr float COLOR_RESET[] = { 0.20f, 0.20f, 0.20f, 1.0f };
constexpr float DEPTH_RESET = 1.0f;


UViewport::UViewport(std::string name) : mViewportName(name), mViewportSize(1, 1) {
    CreateFramebuffer();
}

UViewport::~UViewport() {
    Clear();
}

void UViewport::Clear() {
    glDeleteFramebuffers(1, &mFBO);
    glDeleteTextures(2, mTexIds);
}

void UViewport::CreateFramebuffer() {
    // Generate framebuffer
    glCreateFramebuffers(1, &mFBO);

    // Generate color texture
    glCreateTextures(GL_TEXTURE_2D, 2, mTexIds);
    glTextureStorage2D(mTexIds[TEX_COLOR], 1, GL_RGB8, GLsizei(mViewportSize.x), GLsizei(mViewportSize.y));
    glTextureParameteri(mTexIds[TEX_COLOR], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(mTexIds[TEX_COLOR], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Generate depth texture
    glTextureStorage2D(mTexIds[TEX_DEPTH], 1, GL_DEPTH_COMPONENT32F, GLsizei(mViewportSize.x), GLsizei(mViewportSize.y));

    // Attach textures to framebuffer
    glNamedFramebufferTexture(mFBO, GL_COLOR_ATTACHMENT0, mTexIds[TEX_COLOR], 0);
    glNamedFramebufferTexture(mFBO, GL_DEPTH_ATTACHMENT, mTexIds[TEX_DEPTH], 0);

    // Specify color buffer
    glNamedFramebufferDrawBuffer(mFBO, GL_COLOR_ATTACHMENT0);
}

void UViewport::ResizeViewport() {
    ImVec2 contentRegionMax = ImGui::GetWindowContentRegionMax();
    ImVec2 contentRegionMin = ImGui::GetWindowContentRegionMin();
    glm::vec2 nextViewportSize(
        contentRegionMax.x - contentRegionMin.x,
        contentRegionMax.y - contentRegionMin.y
    );

    ImVec2 windowPos = ImGui::GetCursorScreenPos();
    mViewportPos.x = windowPos.x;
    mViewportPos.y = windowPos.y;

    if (nextViewportSize.x <= 0 || nextViewportSize.y <= 0) {
        return;
    }

    if (nextViewportSize == mViewportSize) {
        return;
    }

    mViewportSize = nextViewportSize;

    Clear();
    CreateFramebuffer();

    mCamera.SetViewportSize(mViewportSize.x, mViewportSize.y);
}

void UViewport::RenderUI(float deltaTime) {
    std::string name = mViewportName;

    if (name.empty()) {
        name = "Viewport##" + std::to_string(mTexIds[TEX_COLOR]);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

// Window begin
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&window_class);

    ImGui::Begin(name.c_str(), &bIsOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleVar();

    ImGui::BeginMenuBar();
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Perspective/Orthographic")) {
            if (mCamera.GetViewMode() == CAM_VIEW_PROJ) {
                mCamera.SetViewMode(CAM_VIEW_ORTHO);
                ImGuizmo::SetOrthographic(true);
            }
            else {
                mCamera.SetViewMode(CAM_VIEW_PROJ);
                ImGuizmo::SetOrthographic(false);
            }
        }
        if (ImGui::BeginMenu("Viewpoint...")) {
            if (ImGui::MenuItem("Top")) {
                mCamera.SetViewMode(CAM_VIEW_ORTHO);
                mCamera.SetView(UNIT_Y * 10.0f, ZERO, -UNIT_Z);
                ImGuizmo::SetOrthographic(true);
            }
            if (ImGui::MenuItem("Bottom")) {
                mCamera.SetViewMode(CAM_VIEW_ORTHO);
                mCamera.SetView(-UNIT_Y * 10.0f, ZERO, UNIT_Z);
                ImGuizmo::SetOrthographic(true);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Front")) {
                mCamera.SetViewMode(CAM_VIEW_ORTHO);
                mCamera.SetView(UNIT_Z * 10.0f, ZERO, -UNIT_Y);
                ImGuizmo::SetOrthographic(true);
            }
            if (ImGui::MenuItem("Back")) {
                mCamera.SetViewMode(CAM_VIEW_ORTHO);
                mCamera.SetView(-UNIT_Z * 10.0f, ZERO, UNIT_Y);
                ImGuizmo::SetOrthographic(true);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Left")) {
                mCamera.SetViewMode(CAM_VIEW_ORTHO);
                mCamera.SetView(UNIT_X * 10.0f, ZERO, -UNIT_Y);
                ImGuizmo::SetOrthographic(true);
            }
            if (ImGui::MenuItem("Right")) {
                mCamera.SetViewMode(CAM_VIEW_ORTHO);
                mCamera.SetView(-UNIT_X * 10.0f, ZERO, UNIT_Y);
                ImGuizmo::SetOrthographic(true);
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Show Ground Grid", nullptr, &bShowGroundGrid);
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderFloat("Grid Size", &mGroundGridSize, 25.0f, 2000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
        ImGui::MenuItem("Show Node Color Legend", nullptr, &bShowNodeLegend);
        ImGui::MenuItem("Show Editor Hints", nullptr, &bShowEditorHints);

        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

    ImGui::BeginChild("actualViewport");
    ImGuizmo::SetDrawlist();
    ResizeViewport();

    const ImGuiIO& io = ImGui::GetIO();
    const bool viewportFocused = ImGui::IsWindowFocused();
    const bool viewportWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool mouseOverViewportImage =
        io.MousePos.x >= mViewportPos.x &&
        io.MousePos.y >= mViewportPos.y &&
        io.MousePos.x <= (mViewportPos.x + mViewportSize.x) &&
        io.MousePos.y <= (mViewportPos.y + mViewportSize.y);

    // Keyboard can stay focus-based, but mouse input (zoom/pan/look) is strictly image-region based.
    const bool acceptKeyboardInput = viewportFocused;
    const bool acceptMouseInput = viewportWindowHovered && mouseOverViewportImage;
    if (acceptKeyboardInput || acceptMouseInput) {
        mCamera.Update(deltaTime, mViewportSize.x, mViewportSize.y, acceptKeyboardInput, acceptMouseInput);
    }

    ImGui::Image((void*)size_t(mTexIds[TEX_COLOR]), { mViewportSize.x, mViewportSize.y }, { 0, 1 }, { 1, 0 });

    if (bShowGroundGrid && mViewportSize.x > 1.0f && mViewportSize.y > 1.0f) {
        glm::mat4 view = mCamera.GetViewMatrix();
        glm::mat4 proj = mCamera.GetProjectionMatrix();
        glm::mat4 identity = glm::identity<glm::mat4>();

        ImGuizmo::SetRect(mViewportPos.x, mViewportPos.y, mViewportSize.x, mViewportSize.y);
        ImGuizmo::DrawGrid(&view[0][0], &proj[0][0], &identity[0][0], mGroundGridSize);
    }

    if (bShowNodeLegend && mViewportSize.x > 1.0f && mViewportSize.y > 1.0f) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec2 panelMin(mViewportPos.x + 12.0f, mViewportPos.y + 12.0f);
        const ImVec2 panelMax(panelMin.x + 230.0f, panelMin.y + 98.0f);
        drawList->AddRectFilled(panelMin, panelMax, IM_COL32(12, 16, 24, 190), 6.0f);
        drawList->AddRect(panelMin, panelMax, IM_COL32(140, 160, 190, 120), 6.0f);

        const ImVec2 textStart(panelMin.x + 12.0f, panelMin.y + 10.0f);
        drawList->AddText(textStart, IM_COL32(230, 236, 245, 255), "Node Colors");

        const float rowStartY = panelMin.y + 32.0f;
        const float rowGap = 20.0f;

        const ImU32 blue = IM_COL32(0, 64, 191, 255);
        const ImU32 green = IM_COL32(38, 217, 64, 255);
        const ImU32 yellow = IM_COL32(242, 217, 51, 255);

        drawList->AddCircleFilled(ImVec2(panelMin.x + 18.0f, rowStartY), 5.0f, blue);
        drawList->AddText(ImVec2(panelMin.x + 30.0f, rowStartY - 7.0f), IM_COL32(220, 225, 235, 255), "Blue: Standard");

        drawList->AddCircleFilled(ImVec2(panelMin.x + 18.0f, rowStartY + rowGap), 5.0f, green);
        drawList->AddText(ImVec2(panelMin.x + 30.0f, rowStartY + rowGap - 7.0f), IM_COL32(220, 225, 235, 255), "Green: Junction");

        drawList->AddCircleFilled(ImVec2(panelMin.x + 18.0f, rowStartY + rowGap * 2.0f), 5.0f, yellow);
        drawList->AddText(ImVec2(panelMin.x + 30.0f, rowStartY + rowGap * 2.0f - 7.0f), IM_COL32(220, 225, 235, 255), "Yellow: Station");
    }

    if (bShowEditorHints && mViewportSize.x > 1.0f && mViewportSize.y > 1.0f) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec2 panelMax(mViewportPos.x + mViewportSize.x - 12.0f, mViewportPos.y + 118.0f);
        const ImVec2 panelMin(panelMax.x - 360.0f, mViewportPos.y + 12.0f);

        drawList->AddRectFilled(panelMin, panelMax, IM_COL32(14, 20, 28, 190), 6.0f);
        drawList->AddRect(panelMin, panelMax, IM_COL32(140, 160, 190, 120), 6.0f);

        drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 10.0f), IM_COL32(230, 236, 245, 255), "Track Editor Shortcuts");
        drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 34.0f), IM_COL32(220, 225, 235, 255), "LMB Drag: Box select");
        drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 52.0f), IM_COL32(220, 225, 235, 255), "Shift: Add to selection");
        drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 70.0f), IM_COL32(220, 225, 235, 255), "Ctrl: Toggle selection");
        drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 88.0f), IM_COL32(220, 225, 235, 255), "Insert/Delete: Add/Remove node");
    }

    ImGui::EndChild();
    ImGui::End();
// Window end

    ImGui::PopStyleVar();
}

void UViewport::BindViewport() {
    // Bind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    glViewport(0, 0, GLsizei(mViewportSize.x), GLsizei(mViewportSize.y));

    glDepthMask(GL_TRUE);
    glClearBufferfv(GL_COLOR, 0, COLOR_RESET);
    glClearBufferfv(GL_DEPTH, 0, &DEPTH_RESET);
}

void UViewport::UnbindViewport() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//void UViewport::RenderScene(AJ3DContext* ctx, float deltaTime) {
//    // Bind FBO
//    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
//    glViewport(0, 0, mViewportSize.x, mViewportSize.y);
//
//    glDepthMask(GL_TRUE);
//    glClearBufferfv(GL_COLOR, 0, COLOR_RESET);
//    glClearBufferfv(GL_DEPTH, 0, &DEPTH_RESET);
//
//    ctx->Render(mCamera, deltaTime);
//
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}
