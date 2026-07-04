#include "ui/UViewport.hpp"
#include "ui/CPropertiesPanel.hpp"
#include "ui/CHierarchyView.hpp"
#include "tracks/UTrackPoint.hpp"

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
    if (mPropertiesPanel) {
        delete mPropertiesPanel;
        mPropertiesPanel = nullptr;
    }
    if (mHierarchyView) {
        delete mHierarchyView;
        mHierarchyView = nullptr;
    }
    Clear();
}

void UViewport::Clear() {
    glDeleteFramebuffers(1, &mFBO);
    glDeleteTextures(2, mTexIds);
    
    // Clean up grid resources
    if (mGridVAO != 0) {
        glDeleteVertexArrays(1, &mGridVAO);
        mGridVAO = 0;
    }
    if (mGridVBO != 0) {
        glDeleteBuffers(1, &mGridVBO);
        mGridVBO = 0;
    }
    if (mGridShaderProgram != 0) {
        glDeleteProgram(mGridShaderProgram);
        mGridShaderProgram = 0;
    }
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

void UViewport::InitializePanels() {
    if (!mPropertiesPanel) {
        mPropertiesPanel = new CPropertiesPanel();
        // Set up callbacks for property changes
        mPropertiesPanel->SetOnPositionChanged([this](const glm::vec3& newPos) {
            if (mSelectedTrackPoint) {
                mSelectedTrackPoint->SetPosition(newPos);
            }
        });
    }

    if (!mHierarchyView) {
        mHierarchyView = new CHierarchyView();
        // Set up callback for selection changes
        mHierarchyView->SetOnSelectionChanged([this](std::shared_ptr<UTracks::UTrackPoint> node) {
            SetSelectedTrackPoint(node);
        });
    }
}

void UViewport::SetTrackPoints(const shared_vector<UTracks::UTrackPoint>& points) {
    if (!mHierarchyView) {
        InitializePanels();
    }
    mHierarchyView->SetTrack(points);
}

void UViewport::SetSelectedTrackPoint(std::shared_ptr<UTracks::UTrackPoint> point) {
    mSelectedTrackPoint = point;
    if (mPropertiesPanel) {
        mPropertiesPanel->SetSelectedNode(point);
    }
}

void UViewport::RenderUI(float deltaTime) {
    std::string name = mViewportName;

    if (name.empty()) {
        name = "Viewport##" + std::to_string(mTexIds[TEX_COLOR]);
    }

    // Initialize panels if not done yet
    if (!mPropertiesPanel || !mHierarchyView) {
        InitializePanels();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

    // Outer window begin
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
    
    // Setup Dockspace for multi-panel layout
    ImGuiID dockspaceId = ImGui::GetID("ViewportInternalDockspace");
    ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

    // Configure docking layout (only once)
    static bool dockingInitialized = false;
    if (!dockingInitialized) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, dockspaceFlags | ImGuiDockNodeFlags_DockSpace);
        
        ImGuiID leftNodeId, rightNodeId;
        ImGuiID rightTopId, rightBottomId;
        
        // Split: 15% left (hierarchy), 85% right
        leftNodeId = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.15f, nullptr, &rightNodeId);
        // Split right into: 80% top (3D viewport), 20% bottom (properties)
        rightTopId = ImGui::DockBuilderSplitNode(rightNodeId, ImGuiDir_Up, 0.80f, nullptr, &rightBottomId);
        
        // Dock windows
        ImGui::DockBuilderDockWindow("##HierarchyView", leftNodeId);
        ImGui::DockBuilderDockWindow("##Viewport3D", rightTopId);
        ImGui::DockBuilderDockWindow("##PropertiesPanel", rightBottomId);
        
        ImGui::DockBuilderFinish(dockspaceId);
        dockingInitialized = true;
    }

    // Draw the hierarchy view
    {
        ImGui::SetNextWindowClass(&window_class);
        ImGui::Begin("##HierarchyView", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("Track Points");
        ImGui::Separator();
        if (mHierarchyView) {
            mHierarchyView->Draw(dockspaceId);
        }
        ImGui::End();
    }

    // Draw the properties panel
    {
        ImGui::SetNextWindowClass(&window_class);
        ImGui::Begin("##PropertiesPanel", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("Properties");
        ImGui::Separator();
        if (mPropertiesPanel) {
            mPropertiesPanel->Draw(dockspaceId);
        }
        ImGui::End();
    }

    // Draw 3D viewport
    {
        ImGui::SetNextWindowClass(&window_class);
        ImGui::Begin("##Viewport3D", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        
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

        const bool acceptKeyboardInput = viewportFocused;
        const bool acceptMouseInput = viewportWindowHovered && mouseOverViewportImage;
        if (acceptKeyboardInput || acceptMouseInput) {
            mCamera.Update(deltaTime, mViewportSize.x, mViewportSize.y, acceptKeyboardInput, acceptMouseInput);
        }

        ImGui::Image((void*)size_t(mTexIds[TEX_COLOR]), { mViewportSize.x, mViewportSize.y }, { 0, 1 }, { 1, 0 });

        // Handle Gizmo manipulation for selected track point
        if (mSelectedTrackPoint && mViewportSize.x > 1.0f && mViewportSize.y > 1.0f) {
            ImGuizmo::SetRect(mViewportPos.x, mViewportPos.y, mViewportSize.x, mViewportSize.y);
            
            glm::mat4 view = mCamera.GetViewMatrix();
            glm::mat4 proj = mCamera.GetProjectionMatrix();
            
            // Create transform matrix from selected node position
            glm::vec3 nodePos = mSelectedTrackPoint->GetPosition();
            glm::mat4 transform = glm::translate(glm::identity<glm::mat4>(), nodePos);
            
            // Check keyboard for operation switching
            if (ImGui::IsKeyPressed(ImGuiKey_G)) {
                mGizmoOperation = ImGuizmo::TRANSLATE;
            } else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                mGizmoOperation = ImGuizmo::ROTATE;
            } else if (ImGui::IsKeyPressed(ImGuiKey_S)) {
                mGizmoOperation = ImGuizmo::SCALE;
            }
            
            // Check keyboard for mode switching (T for World/Local toggle)
            if (ImGui::IsKeyPressed(ImGuiKey_T)) {
                mGizmoMode = (mGizmoMode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
            }
            
            // Apply manipulate
            if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0], mGizmoOperation, mGizmoMode, &transform[0][0])) {
                // Update node position if gizmo was moved
                glm::vec3 newPos = glm::vec3(transform[3]);
                mSelectedTrackPoint->SetPosition(newPos);
                
                // Update properties panel to reflect changes
                if (mPropertiesPanel) {
                    mPropertiesPanel->SetSelectedNode(mSelectedTrackPoint);
                }
            }
        }

        // Grid rendering disabled temporarily - will be reimplemented with proper 3D rendering
        // ImGuizmo::DrawGrid() was rendering on the ImGui surface layer, occluding 3D content
        // TODO: Implement grid rendering within the framebuffer using VAO/VBO with proper depth testing
        /*
        if (bShowGroundGrid && mViewportSize.x > 1.0f && mViewportSize.y > 1.0f) {
            ImGuizmo::SetRect(mViewportPos.x, mViewportPos.y, mViewportSize.x, mViewportSize.y);
            
            // Render grid with ImGuizmo (simple version first)
            glm::mat4 view = mCamera.GetViewMatrix();
            glm::mat4 proj = mCamera.GetProjectionMatrix();
            glm::mat4 identity = glm::identity<glm::mat4>();
            
            // Draw base grid
            ImGuizmo::DrawGrid(&view[0][0], &proj[0][0], &identity[0][0], mGroundGridSize);
            
            // Draw larger grid in background (for "infinite" effect)
            float largeGridSize = mGroundGridSize * 10.0f;
            ImGuizmo::SetDrawlist(); // Ensure we're using the correct draw list
            ImGuizmo::DrawGrid(&view[0][0], &proj[0][0], &identity[0][0], largeGridSize);
        }
        */

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
        ImGui::PopStyleVar();
        ImGui::End();
    }

    // Outer window end
    ImGui::End();
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

void UViewport::RenderGridLines() {
    // TODO: Implement proper 3D grid rendering using VAO/VBO
    // This will render the grid within the framebuffer with proper depth testing
    // and support for Blender-style fade-out at distance
    if (!bShowGroundGrid) return;
    
    // Initialize grid on first call
    if (!bGridInitialized) {
        InitializeGrid();
    }
    
    if (!bGridInitialized || mGridShaderProgram == 0) return;
    
    // Enable blending for fade-out effect
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Use grid shader
    glUseProgram(mGridShaderProgram);
    
    // Set up matrices
    glm::mat4 view = mCamera.GetViewMatrix();
    glm::mat4 proj = mCamera.GetProjectionMatrix();
    glm::vec3 cameraPos = glm::inverse(view)[3];
    
    GLint viewLoc = glGetUniformLocation(mGridShaderProgram, "uView");
    GLint projLoc = glGetUniformLocation(mGridShaderProgram, "uProj");
    GLint gridSizeLoc = glGetUniformLocation(mGridShaderProgram, "uGridSize");
    GLint cameraPosLoc = glGetUniformLocation(mGridShaderProgram, "uCameraPos");
    GLint fadeDistanceLoc = glGetUniformLocation(mGridShaderProgram, "uFadeDistance");
    
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);
    glUniform3fv(cameraPosLoc, 1, &cameraPos[0]);
    
    // Bind VAO
    glBindVertexArray(mGridVAO);
    
    // Draw multiple grid layers with different scales (like Blender)
    float baseGridSize = mGroundGridSize;
    float fadeDistance = baseGridSize * 100.0f;
    
    // Layer 1: Base grid
    glUniform1f(gridSizeLoc, baseGridSize);
    glUniform1f(fadeDistanceLoc, fadeDistance);
    glDrawArrays(GL_LINES, 0, mGridLineCount);
    
    // Layer 2: 10x larger grid (for infinite effect)
    float largeGridSize = baseGridSize * 10.0f;
    glUniform1f(gridSizeLoc, largeGridSize);
    glUniform1f(fadeDistanceLoc, fadeDistance * 10.0f);
    glDrawArrays(GL_LINES, 0, mGridLineCount);
    
    // Layer 3: 100x larger grid (very far)
    float veryLargeGridSize = baseGridSize * 100.0f;
    glUniform1f(gridSizeLoc, veryLargeGridSize);
    glUniform1f(fadeDistanceLoc, fadeDistance * 100.0f);
    glDrawArrays(GL_LINES, 0, mGridLineCount);
    
    // Unbind
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void UViewport::InitializeGrid() {
    if (bGridInitialized) return;
    
    // Load grid shader
    const char* gridVertSrc = R"(
#version 460 core

layout(location = 0) in vec3 inPosition;

uniform mat4 uView;
uniform mat4 uProj;

out VS_OUT {
    vec3 worldPos;
} vs_out;

void main() {
    vs_out.worldPos = inPosition;
    gl_Position = uProj * uView * vec4(inPosition, 1.0);
}
)";

    const char* gridFragSrc = R"(
#version 460 core

in VS_OUT {
    vec3 worldPos;
} fs_in;

uniform float uGridSize;
uniform vec3 uCameraPos;
uniform float uFadeDistance;

out vec4 oPixelColor;

void main() {
    vec3 pos = fs_in.worldPos;
    vec3 grid = abs(mod(pos, uGridSize)) - uGridSize * 0.5;
    
    // Create grid line - check if we're near a grid line
    float distToLine = min(abs(grid.x), abs(grid.z));
    float lineWidth = 0.1;
    float alpha = smoothstep(lineWidth + 0.02, lineWidth - 0.02, distToLine) * 0.6;
    
    // Distance-based fade-out (Blender style)
    float distance = length(uCameraPos - pos);
    float fadeStart = uFadeDistance * 0.3;
    float fadeEnd = uFadeDistance;
    float fadeFactor = smoothstep(fadeEnd, fadeStart, distance);
    
    alpha *= fadeFactor;
    
    if (alpha < 0.01) discard;
    
    // Grid color: darker at distance, lighter up close
    vec3 gridColor = mix(vec3(0.3, 0.35, 0.4), vec3(0.5, 0.55, 0.6), fadeFactor);
    oPixelColor = vec4(gridColor, alpha);
}
)";
    
    // Compile vertex shader
    uint32_t vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &gridVertSrc, nullptr);
    glCompileShader(vertShader);
    
    // Check vertex shader compilation
    int success;
    char infoLog[512];
    glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertShader, 512, nullptr, infoLog);
        printf("Grid vertex shader compilation failed: %s\n", infoLog);
        glDeleteShader(vertShader);
        return;
    }
    
    // Compile fragment shader
    uint32_t fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &gridFragSrc, nullptr);
    glCompileShader(fragShader);
    
    // Check fragment shader compilation
    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragShader, 512, nullptr, infoLog);
        printf("Grid fragment shader compilation failed: %s\n", infoLog);
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return;
    }
    
    // Link shader program
    mGridShaderProgram = glCreateProgram();
    glAttachShader(mGridShaderProgram, vertShader);
    glAttachShader(mGridShaderProgram, fragShader);
    glLinkProgram(mGridShaderProgram);
    
    // Check linking
    glGetProgramiv(mGridShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(mGridShaderProgram, 512, nullptr, infoLog);
        printf("Grid shader program linking failed: %s\n", infoLog);
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return;
    }
    
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    
    // Generate grid line vertices
    std::vector<glm::vec3> gridLines;
    GenerateGridLines(mGroundGridSize, gridLines);
    mGridLineCount = gridLines.size();
    
    // Create VAO and VBO
    glCreateVertexArrays(1, &mGridVAO);
    glCreateBuffers(1, &mGridVBO);
    
    // Bind and fill VBO
    glBindBuffer(GL_ARRAY_BUFFER, mGridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(glm::vec3), gridLines.data(), GL_STATIC_DRAW);
    
    // Set up VAO
    glBindVertexArray(mGridVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    
    // Unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    bGridInitialized = true;
    printf("Grid rendering initialized successfully\n");
}

void UViewport::GenerateGridLines(float gridSize, std::vector<glm::vec3>& outLines) {
    int gridCount = 51;  // -25 to +25 grid cells
    float halfExtent = gridSize * (gridCount / 2);
    
    // Generate grid lines
    for (int i = -gridCount / 2; i <= gridCount / 2; ++i) {
        float pos = i * gridSize;
        
        // Lines parallel to X axis
        outLines.push_back(glm::vec3(pos, 0.0f, -halfExtent));
        outLines.push_back(glm::vec3(pos, 0.0f, halfExtent));
        
        // Lines parallel to Z axis
        outLines.push_back(glm::vec3(-halfExtent, 0.0f, pos));
        outLines.push_back(glm::vec3(halfExtent, 0.0f, pos));
    }
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
