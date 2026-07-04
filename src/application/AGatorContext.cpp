#include "application/AGatorContext.hpp"
#include "application/AInput.hpp"

#include "application/ANavContext.hpp"
#include "application/ATrackContext.hpp"
#include "application/ADrawableContext.hpp"
#include "application/AEntityContext.hpp"

#include "ui/UViewport.hpp"
#include "ui/UViewportPicker.hpp"

#include "util/rdr1util.hpp"

#include "application/AOptions.hpp"

#include <util/bstream.h>

#include <iostream>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuiFileDialog.h>
#include "util/ImGuizmo.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <limits>
#include <set>

namespace {
	bool HasSavedLayout() {
		return std::filesystem::exists(std::filesystem::current_path() / "imgui.ini");
	}

	ImVec2 GetResponsiveDialogSize(float widthRatio, float heightRatio, float maxWidth, float maxHeight) {
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		const ImVec2 workSize = mainViewport != nullptr ? mainViewport->WorkSize : ImVec2(800.0f, 600.0f);
		const float dialogMaxWidth = std::max(320.0f, std::min(maxWidth, workSize.x));
		const float dialogMaxHeight = std::max(240.0f, std::min(maxHeight, workSize.y));

		return {
			std::clamp(workSize.x * widthRatio, 320.0f, dialogMaxWidth),
			std::clamp(workSize.y * heightRatio, 240.0f, dialogMaxHeight)
		};
	}

	bool BuildWorldRayFromMouse(
		ASceneCamera& camera,
		const glm::vec2& mouseScreen,
		const glm::vec2& viewportPos,
		const glm::vec2& viewportSize,
		glm::vec3& outOrigin,
		glm::vec3& outDirection) {
		if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
			return false;
		}

		const float ndcX = ((mouseScreen.x - viewportPos.x) / viewportSize.x) * 2.0f - 1.0f;
		const float ndcY = 1.0f - ((mouseScreen.y - viewportPos.y) / viewportSize.y) * 2.0f;

		const glm::mat4 invViewProj = glm::inverse(camera.GetProjectionMatrix() * camera.GetViewMatrix());
		glm::vec4 nearClip(ndcX, ndcY, -1.0f, 1.0f);
		glm::vec4 farClip(ndcX, ndcY, 1.0f, 1.0f);

		glm::vec4 nearWorld = invViewProj * nearClip;
		glm::vec4 farWorld = invViewProj * farClip;
		if (nearWorld.w == 0.0f || farWorld.w == 0.0f) {
			return false;
		}

		nearWorld /= nearWorld.w;
		farWorld /= farWorld.w;

		outOrigin = glm::vec3(nearWorld);
		outDirection = glm::normalize(glm::vec3(farWorld - nearWorld));
		return glm::length(outDirection) > 0.0f;
	}

	bool IntersectRayTriangle(
		const glm::vec3& rayOrigin,
		const glm::vec3& rayDir,
		const glm::vec3& a,
		const glm::vec3& b,
		const glm::vec3& c,
		float& outT) {
		constexpr float EPS = 1e-6f;
		const glm::vec3 edge1 = b - a;
		const glm::vec3 edge2 = c - a;
		const glm::vec3 p = glm::cross(rayDir, edge2);
		const float det = glm::dot(edge1, p);
		if (std::abs(det) < EPS) {
			return false;
		}

		const float invDet = 1.0f / det;
		const glm::vec3 tVec = rayOrigin - a;
		const float u = glm::dot(tVec, p) * invDet;
		if (u < 0.0f || u > 1.0f) {
			return false;
		}

		const glm::vec3 q = glm::cross(tVec, edge1);
		const float v = glm::dot(rayDir, q) * invDet;
		if (v < 0.0f || (u + v) > 1.0f) {
			return false;
		}

		const float t = glm::dot(edge2, q) * invDet;
		if (t <= EPS) {
			return false;
		}

		outT = t;
		return true;
	}

	int PickPolygonAtRay(CNavmeshData& navmesh, const glm::vec3& rayOrigin, const glm::vec3& rayDirection) {
		float bestT = std::numeric_limits<float>::max();
		int bestPolygon = -1;

		std::vector<Vector3> vertices;
		const uint32_t polygonCount = navmesh.GetPolygonCount();
		for (uint32_t polygonIndex = 0; polygonIndex < polygonCount; polygonIndex++) {
			if (!navmesh.TryGetPolygonVerticesZUp(polygonIndex, vertices) || vertices.size() < 3) {
				continue;
			}

			const glm::vec3 anchor(vertices[0].x, vertices[0].y, vertices[0].z);
			for (size_t i = 1; i + 1 < vertices.size(); i++) {
				const glm::vec3 b(vertices[i].x, vertices[i].y, vertices[i].z);
				const glm::vec3 c(vertices[i + 1].x, vertices[i + 1].y, vertices[i + 1].z);

				float hitT = 0.0f;
				if (IntersectRayTriangle(rayOrigin, rayDirection, anchor, b, c, hitT) && hitT < bestT) {
					bestT = hitT;
					bestPolygon = int(polygonIndex);
				}
			}
		}

		return bestPolygon;
	}
}

AGatorContext::AGatorContext() : bIsDockingConfigured(false), mMainDockSpaceID(UINT32_MAX), mDockNodeTopID(UINT32_MAX),
	mDockNodeRightID(UINT32_MAX), mDockNodeDownID(UINT32_MAX), mPropertiesDockNodeID(UINT32_MAX), mAppPosition({ 0, 0 }),
	mNavContext(std::make_shared<ANavContext>()), mTrackContext(std::make_shared<ATrackContext>()), mDrawableContext(std::make_shared<ADrawableContext>()), mEntityContext(std::make_shared<AEntityContext>()),
	mPropertiesPanelTopID(UINT32_MAX), mPropertiesPanelBottomID(UINT32_MAX)
{
	OPTIONS.Load();
}

AGatorContext::~AGatorContext() {
	UViewportPicker::DestroyPicker();
	OPTIONS.Save();
}

void AGatorContext::SetUpDocking() {
	const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	const bool useTinyLayout = mainViewport->WorkSize.x < 1100.0f || mainViewport->WorkSize.y < 720.0f;
	const bool useCompactLayout = !useTinyLayout && (mainViewport->WorkSize.x < 1400.0f || mainViewport->WorkSize.y < 900.0f);
	const int layoutMode = useTinyLayout ? 2 : (useCompactLayout ? 1 : 0);
	static int lastLayoutMode = layoutMode;
	const bool hasSavedLayout = HasSavedLayout();

	ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	mMainDockSpaceID = ImGui::DockSpaceOverViewport(mainViewport, dockFlags);

	if (!bIsDockingConfigured && hasSavedLayout) {
		bIsDockingConfigured = true;
		lastLayoutMode = layoutMode;
		return;
	}

	if ((!bIsDockingConfigured || lastLayoutMode != layoutMode) && !hasSavedLayout) {
		ImGui::DockBuilderRemoveNode(mMainDockSpaceID); // clear any previous layout
		ImGui::DockBuilderAddNode(mMainDockSpaceID, dockFlags | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(mMainDockSpaceID, mainViewport->WorkSize);

		mDockNodeTopID = UINT32_MAX;
		mDockNodeRightID = UINT32_MAX;
		mDockNodeDownID = UINT32_MAX;

		if (useTinyLayout) {
			// For tiny layout: Properties panel on bottom (25% height), viewport on top
			mPropertiesDockNodeID = ImGui::DockBuilderSplitNode(mMainDockSpaceID, ImGuiDir_Down, 0.25f, nullptr, &mMainDockSpaceID);
			mPropertiesPanelTopID = ImGui::DockBuilderSplitNode(mPropertiesDockNodeID, ImGuiDir_Left, 0.38f, nullptr, &mPropertiesPanelBottomID);
		}
		else {
			// For larger layouts: Properties panel on left (20% width), viewport on right
			const float propertiesWidthRatio = useCompactLayout ? 0.20f : 0.18f;
			const float propertiesHeightRatio = useCompactLayout ? 0.45f : 0.50f;

			mPropertiesDockNodeID = ImGui::DockBuilderSplitNode(mMainDockSpaceID, ImGuiDir_Left, propertiesWidthRatio, nullptr, &mMainDockSpaceID);
			mPropertiesPanelTopID = ImGui::DockBuilderSplitNode(mPropertiesDockNodeID, ImGuiDir_Up, propertiesHeightRatio, nullptr, &mPropertiesPanelBottomID);
		}

		ImGui::DockBuilderDockWindow("Properties", mPropertiesPanelTopID);
		ImGui::DockBuilderDockWindow("Data Editor", mPropertiesPanelBottomID);
		ImGui::DockBuilderDockWindow("Main Viewport", mMainDockSpaceID);

		ImGui::DockBuilderFinish(mMainDockSpaceID);

		bIsDockingConfigured = true;
		lastLayoutMode = layoutMode;
	}
}

void AGatorContext::RenderMenuBar() {
	if (ImGui::BeginMainMenuBar()) {
		if (mTrackContext->IsLoaded()) {
			const ImGuiIO& io = ImGui::GetIO();
			if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
				SaveTracksCB();
			}
		}

		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open...")) {
				LoadFileCB();
			}
			if (ImGui::MenuItem("Load Game World...")) {
				std::string startingDir = OPTIONS.mLastOpenedDir.empty() ? "." : OPTIONS.mLastOpenedDir.u8string();
				ImGuiFileDialog::Instance()->OpenDialog("loadWorldDirDialog", "Select RDR2 Game Directory", nullptr, startingDir, 1, nullptr, ImGuiFileDialogFlags_Modal);
			}
			ImGui::Separator();
			ImGui::TextDisabled("Load Game World: View navmeshes & drawables");
			ImGui::TextDisabled("Then load your custom traintracks via Open");
			ImGui::Separator();
			
			if (ImGui::BeginMenu("Railroad Data")) {
				if (!mTrackContext->IsLoaded()) {
					ImGui::BeginDisabled();
				}

				if (ImGui::MenuItem("Save", "Ctrl+S")) {
					SaveTracksCB();
				}
				if (ImGui::MenuItem("Save as...")) {
					SaveTracksAsCB();
				}

				if (!mTrackContext->IsLoaded()) {
					ImGui::EndDisabled();
				}

				ImGui::EndMenu();
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Close")) {
				if (GLFWwindow* window = glfwGetCurrentContext()) {
					glfwSetWindowShouldClose(window, GLFW_TRUE);
				}
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("About")) {
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

void AGatorContext::SaveTracksCB() {
	if (!mTrackContext->IsLoaded()) {
		return;
	}

	if (OPTIONS.mLastSavedRailroadDir.empty()) {
		SaveTracksAsCB();
		return;
	}

	mTrackContext->SaveTracks(OPTIONS.mLastSavedRailroadDir);
}

void AGatorContext::Update(float deltaTime) {
	glm::vec2 viewportSize = mMainViewport->GetViewportSize();
	
	// Skip picker resize if viewport hasn't been initialized yet (ResizeViewport not called)
	if (viewportSize.x > 1 || viewportSize.y > 1) {
		UViewportPicker::ResizePicker(uint32_t(viewportSize.x), uint32_t(viewportSize.y));
	}

	glm::vec2 screenMousePos = mAppPosition + AInput::GetMousePosition();
	glm::vec2 bufferMousePos = screenMousePos - mMainViewport->GetViewportPosition();
	bufferMousePos.y = mMainViewport->GetViewportSize().y - bufferMousePos.y;

	if (bufferMousePos.x >= 0 && bufferMousePos.y >= 0) {
		mTrackContext->OnMouseHover(mMainViewport->GetCamera(), int32_t(bufferMousePos.x), int32_t(bufferMousePos.y));
	}
}

void AGatorContext::RenderPropertiesPanel() {
	ImGuiWindowClass window_class;
	window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;

	// Properties panel
	ImGui::SetNextWindowClass(&window_class);
	ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

	mTrackContext->RenderTreeView();

	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Navmesh", ImGuiTreeNodeFlags_DefaultOpen)) {
		static bool navFlagCompressed = false;
		static bool navFlagHasLinks = false;
		static bool navFlagDynamic = false;
		static bool navFlagHasWater = false;
		static bool navFlagIsDlc = false;
		static bool navFlagDlcSwappable = false;
		static bool polyFlagSmall = false;
		static bool polyFlagLarge = false;
		static bool polyFlagPaved = false;
		static bool polyFlagSheltered = false;
		static bool polyFlagSteep = false;
		static bool polyFlagWater = false;

		ImGui::Indent();
		ImGui::Checkbox("Pick Face On Viewport Click", &mEnableNavmeshFacePick);
		ImGui::TextDisabled("Click in viewport to select polygon face.");

		const char* highlightOptions[] = {
			"Show All Flag Colors",
			"Only SMALL",
			"Only LARGE",
			"Only PAVED",
			"Only SHELTERED",
			"Only STEEP",
			"Only WATER",
			"Only Network Spawn Candidate",
			"Only Road",
			"Only Interior",
			"Only Isolated",
			"Only Shallow Water"
		};
		int highlightMode = int(mNavContext->GetHighlightMode());
		if (ImGui::Combo("Highlight Filter", &highlightMode, highlightOptions, IM_ARRAYSIZE(highlightOptions))) {
			mNavContext->SetHighlightMode(uint32_t(highlightMode));
		}

		bool hideNonMatching = mNavContext->GetHideNonMatchingFaces();
		if (ImGui::Checkbox("Hide Non-Matching Faces", &hideNonMatching)) {
			mNavContext->SetHideNonMatchingFaces(hideNonMatching);
		}

		bool outlineHighlight = mNavContext->GetOutlineHighlight();
		if (ImGui::Checkbox("Outline Highlight", &outlineHighlight)) {
			mNavContext->SetOutlineHighlight(outlineHighlight);
		}
		ImGui::TextDisabled("Ohne Hide werden nicht passende Faces dunkel dargestellt.");

		if (ImGui::TreeNode("Flag Color Legend")) {
			ImGui::TextColored(ImVec4(0.35f, 0.45f, 1.0f, 1.0f), "SMALL");
			ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "LARGE");
			ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.35f, 1.0f), "PAVED");
			ImGui::TextColored(ImVec4(0.80f, 0.35f, 0.90f, 1.0f), "SHELTERED");
			ImGui::TextColored(ImVec4(1.00f, 0.30f, 0.30f, 1.0f), "STEEP");
			ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.0f, 1.0f), "WATER");
			ImGui::TextDisabled("Mehrere Flags werden farblich gemischt.");
			ImGui::TreePop();
		}
		ImGui::Text("Loaded navmeshes: %zu", mNavContext->GetLoadedNavmeshCount());

		if (mNavContext->GetLoadedNavmeshCount() == 0) {
			mSelectedNavmeshIndex = -1;
		}
		else if (mSelectedNavmeshIndex < 0 || size_t(mSelectedNavmeshIndex) >= mNavContext->GetLoadedNavmeshCount()) {
			mSelectedNavmeshIndex = 0;
		}

		if (ImGui::BeginListBox("##navmeshList", ImVec2(0.0f, 110.0f))) {
			for (size_t i = 0; i < mNavContext->GetLoadedNavmeshCount(); i++) {
				std::string label = mNavContext->GetLoadedNavmeshLabel(i);
				if (mNavContext->IsLoadedNavmeshDirty(i)) {
					label += " *";
				}

				const bool isSelected = (int(i) == mSelectedNavmeshIndex);
				if (ImGui::Selectable(label.c_str(), isSelected)) {
					mSelectedNavmeshIndex = int(i);
					mSelectedPolygonIndex = 0;
				}
			}
			ImGui::EndListBox();
		}

		if (mSelectedNavmeshIndex >= 0 && size_t(mSelectedNavmeshIndex) < mNavContext->GetLoadedNavmeshCount()) {
			if (ImGui::Button("Remove Selected Navmesh", { 220, 0 })) {
				if (mNavContext->RemoveLoadedNavmesh(size_t(mSelectedNavmeshIndex))) {
					mSelectedPolygonIndex = 0;
					if (mNavContext->GetLoadedNavmeshCount() == 0) {
						mSelectedNavmeshIndex = -1;
					}
					else if (size_t(mSelectedNavmeshIndex) >= mNavContext->GetLoadedNavmeshCount()) {
						mSelectedNavmeshIndex = int(mNavContext->GetLoadedNavmeshCount() - 1);
					}
				}
			}
		}

		if (ImGui::Button("Clear Loaded Navmeshes", { 220, 0 })) {
			mNavContext->ClearLoadedNavmeshes();
			mSelectedNavmeshIndex = -1;
			mSelectedPolygonIndex = 0;
		}

		if (mSelectedNavmeshIndex >= 0 && size_t(mSelectedNavmeshIndex) < mNavContext->GetLoadedNavmeshCount()) {
			std::shared_ptr<CNavmeshData> selectedNavmesh = mNavContext->GetLoadedNavmeshData(size_t(mSelectedNavmeshIndex));
			const ANavXmlMetadata* xmlMetadata = mNavContext->GetLoadedNavmeshXmlMetadata(size_t(mSelectedNavmeshIndex));
			if (selectedNavmesh != nullptr) {
				if (xmlMetadata != nullptr && xmlMetadata->available) {
					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Text("XML Insights");
					ImGui::Text("Polygons: %u", xmlMetadata->polygonCount);
					ImGui::Text("SpecialLinks: %u", xmlMetadata->specialLinkCount);
					ImGui::Text("NetworkSpawnCandidate faces: %u", xmlMetadata->networkSpawnCandidateCount);
					ImGui::Text("Road faces: %u", xmlMetadata->roadPolygonCount);
					ImGui::Text("Interior faces: %u", xmlMetadata->interiorPolygonCount);
					ImGui::Text("Isolated faces: %u", xmlMetadata->isolatedPolygonCount);
					ImGui::Text("ShallowWater faces: %u", xmlMetadata->shallowWaterPolygonCount);

					if (!xmlMetadata->specialLinkTypeCounts.empty() && ImGui::TreeNode("SpecialLink Types")) {
						for (const auto& [typeValue, count] : xmlMetadata->specialLinkTypeCounts) {
							ImGui::BulletText("Type %u: %u", typeValue, count);
						}
						ImGui::TreePop();
					}
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Text("Selected Navmesh Flags");

				const uint32_t navFlags = selectedNavmesh->GetNavmeshFlags();
				navFlagCompressed = (navFlags & ENavMeshFlags::VTX_DATA_COMPRESSED) != 0;
				navFlagHasLinks = (navFlags & ENavMeshFlags::HAS_LINKS) != 0;
				navFlagDynamic = (navFlags & ENavMeshFlags::IS_DYNAMIC) != 0;
				navFlagHasWater = (navFlags & ENavMeshFlags::HAS_WATER) != 0;
				navFlagIsDlc = (navFlags & ENavMeshFlags::IS_DLC) != 0;
				navFlagDlcSwappable = (navFlags & ENavMeshFlags::DLC_SWAPPABLE) != 0;

				bool navFlagsChanged = false;
				navFlagsChanged |= ImGui::Checkbox("VTX_DATA_COMPRESSED", &navFlagCompressed);
				navFlagsChanged |= ImGui::Checkbox("HAS_LINKS", &navFlagHasLinks);
				navFlagsChanged |= ImGui::Checkbox("IS_DYNAMIC", &navFlagDynamic);
				navFlagsChanged |= ImGui::Checkbox("HAS_WATER", &navFlagHasWater);
				navFlagsChanged |= ImGui::Checkbox("IS_DLC", &navFlagIsDlc);
				navFlagsChanged |= ImGui::Checkbox("DLC_SWAPPABLE", &navFlagDlcSwappable);

				if (navFlagsChanged) {
					uint32_t newFlags = 0;
					newFlags |= navFlagCompressed ? ENavMeshFlags::VTX_DATA_COMPRESSED : 0;
					newFlags |= navFlagHasLinks ? ENavMeshFlags::HAS_LINKS : 0;
					newFlags |= navFlagDynamic ? ENavMeshFlags::IS_DYNAMIC : 0;
					newFlags |= navFlagHasWater ? ENavMeshFlags::HAS_WATER : 0;
					newFlags |= navFlagIsDlc ? ENavMeshFlags::IS_DLC : 0;
					newFlags |= navFlagDlcSwappable ? ENavMeshFlags::DLC_SWAPPABLE : 0;
					selectedNavmesh->SetNavmeshFlags(newFlags);
					mNavContext->SetLoadedNavmeshDirty(size_t(mSelectedNavmeshIndex), true);
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Text("Polygon Flags");

				const int polygonCount = int(selectedNavmesh->GetPolygonCount());
				if (polygonCount > 0) {
					mSelectedPolygonIndex = std::clamp(mSelectedPolygonIndex, 0, polygonCount - 1);
					ImGui::InputInt("Polygon Index", &mSelectedPolygonIndex);
					mSelectedPolygonIndex = std::clamp(mSelectedPolygonIndex, 0, polygonCount - 1);

					uint8_t polygonFlags = 0;
					if (selectedNavmesh->TryGetPolygonFlags(uint32_t(mSelectedPolygonIndex), polygonFlags)) {
						polyFlagSmall = (polygonFlags & EPolygonFlags::SMALL) != 0;
						polyFlagLarge = (polygonFlags & EPolygonFlags::LARGE) != 0;
						polyFlagPaved = (polygonFlags & EPolygonFlags::PAVED) != 0;
						polyFlagSheltered = (polygonFlags & EPolygonFlags::SHELTERED) != 0;
						polyFlagSteep = (polygonFlags & EPolygonFlags::STEEP) != 0;
						polyFlagWater = (polygonFlags & EPolygonFlags::WATER) != 0;

						bool polygonFlagsChanged = false;
						polygonFlagsChanged |= ImGui::Checkbox("SMALL", &polyFlagSmall);
						polygonFlagsChanged |= ImGui::Checkbox("LARGE", &polyFlagLarge);
						polygonFlagsChanged |= ImGui::Checkbox("PAVED", &polyFlagPaved);
						polygonFlagsChanged |= ImGui::Checkbox("SHELTERED", &polyFlagSheltered);
						polygonFlagsChanged |= ImGui::Checkbox("STEEP", &polyFlagSteep);
						polygonFlagsChanged |= ImGui::Checkbox("WATER", &polyFlagWater);

						if (polygonFlagsChanged) {
							uint8_t newPolygonFlags = 0;
							newPolygonFlags |= polyFlagSmall ? EPolygonFlags::SMALL : 0;
							newPolygonFlags |= polyFlagLarge ? EPolygonFlags::LARGE : 0;
							newPolygonFlags |= polyFlagPaved ? EPolygonFlags::PAVED : 0;
							newPolygonFlags |= polyFlagSheltered ? EPolygonFlags::SHELTERED : 0;
							newPolygonFlags |= polyFlagSteep ? EPolygonFlags::STEEP : 0;
							newPolygonFlags |= polyFlagWater ? EPolygonFlags::WATER : 0;

							if (selectedNavmesh->TrySetPolygonFlags(uint32_t(mSelectedPolygonIndex), newPolygonFlags)) {
								mNavContext->SetLoadedNavmeshDirty(size_t(mSelectedNavmeshIndex), true);
							}
						}

						CNavPolygonInfo3 info3{};
						if (selectedNavmesh->TryGetPolygonInfo3(uint32_t(mSelectedPolygonIndex), info3)) {
							ImGui::Spacing();
							ImGui::Text("Selected Face Metadata");

							bool faceInfoChanged = false;
							faceInfoChanged |= ImGui::Checkbox("Network Spawn Candidate", &info3.bNetworkSpawnCandidate);
							faceInfoChanged |= ImGui::Checkbox("Is Road", &info3.bIsRoad);
							faceInfoChanged |= ImGui::Checkbox("Is Train Track", &info3.bIsTrainTrack);
							faceInfoChanged |= ImGui::Checkbox("Is Shallow Water", &info3.bIsShallowWater);
							faceInfoChanged |= ImGui::Checkbox("Interior", &info3.bInterior);
							faceInfoChanged |= ImGui::Checkbox("Isolated", &info3.bIsolated);
							faceInfoChanged |= ImGui::Checkbox("Near Vehicle", &info3.bNearVehicle);

							int pedDensity = int(info3.mPedDensity);
							if (ImGui::SliderInt("Ped Density", &pedDensity, 0, 255)) {
								info3.mPedDensity = uint8_t(std::clamp(pedDensity, 0, 255));
								faceInfoChanged = true;
							}

							int audioProps = int(info3.mAudioProperties);
							if (ImGui::InputInt("Audio Props", &audioProps)) {
								audioProps = std::clamp(audioProps, 0, 0xFFFF);
								info3.mAudioProperties = uint16_t(audioProps);
								faceInfoChanged = true;
							}

							if (faceInfoChanged && selectedNavmesh->TrySetPolygonInfo3(uint32_t(mSelectedPolygonIndex), info3)) {
								mNavContext->SetLoadedNavmeshDirty(size_t(mSelectedNavmeshIndex), true);
							}
						}

						uint8_t uiPolygonFlags = 0;
						uiPolygonFlags |= polyFlagSmall ? EPolygonFlags::SMALL : 0;
						uiPolygonFlags |= polyFlagLarge ? EPolygonFlags::LARGE : 0;
						uiPolygonFlags |= polyFlagPaved ? EPolygonFlags::PAVED : 0;
						uiPolygonFlags |= polyFlagSheltered ? EPolygonFlags::SHELTERED : 0;
						uiPolygonFlags |= polyFlagSteep ? EPolygonFlags::STEEP : 0;
						uiPolygonFlags |= polyFlagWater ? EPolygonFlags::WATER : 0;

						if (ImGui::Button("Apply Polygon Flags To All", { 220, 0 })) {
							const uint32_t changed = selectedNavmesh->SetAllPolygonFlagsMasked(0xFF, uiPolygonFlags);
							if (changed > 0) {
								mNavContext->SetLoadedNavmeshDirty(size_t(mSelectedNavmeshIndex), true);
							}
						}
					}
				}
				else {
					ImGui::TextDisabled("No polygons available in selected navmesh.");
				}

				if (mNavContext->IsLoadedNavmeshDirty(size_t(mSelectedNavmeshIndex))) {
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "Selected navmesh has unsaved flag changes.");
				}
			}
		}

		const std::string& navInfo = mNavContext->GetLastLoadInfo();
		if (!navInfo.empty()) {
			ImGui::Spacing();
			if (navInfo.find("XML fallback") != std::string::npos) {
				ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "Fallback loaded from XML");
				ImGui::TextWrapped("%s", navInfo.c_str());
			}
			else {
				ImGui::TextColored(ImVec4(0.45f, 0.90f, 0.60f, 1.0f), "%s", navInfo.c_str());
			}
		}

		const std::string& navError = mNavContext->GetLastLoadError();
		if (!navError.empty()) {
			ImGui::Spacing();
			ImGui::TextWrapped("%s", navError.c_str());
		}
		ImGui::Unindent();
	}
	
	ImGui::End();

	// Data editor panel
	ImGui::SetNextWindowClass(&window_class);
	ImGui::Begin("Data Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
	
	mTrackContext->RenderDataEditor();

	ImGui::End();
}

void AGatorContext::Render(float deltaTime) {
	ZoneScoped;

	SetUpDocking();

	RenderMenuBar();
	RenderPropertiesPanel();

	glm::vec2 viewportSize = mMainViewport->GetViewportSize();
	glm::vec2 viewportPos = mMainViewport->GetViewportPosition() + mAppPosition;
	ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

	mMainViewport->RenderUI(deltaTime);
	mTrackContext->RenderUI(mMainViewport->GetCamera(), mMainViewport->GetViewportPosition(), viewportSize);

	if (mEnableNavmeshFacePick &&
		mSelectedNavmeshIndex >= 0 &&
		size_t(mSelectedNavmeshIndex) < mNavContext->GetLoadedNavmeshCount() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!ImGui::GetIO().WantCaptureMouse) {
		const glm::vec2 mousePos = glm::vec2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
		const bool insideViewport =
			mousePos.x >= viewportPos.x &&
			mousePos.y >= viewportPos.y &&
			mousePos.x <= viewportPos.x + viewportSize.x &&
			mousePos.y <= viewportPos.y + viewportSize.y;

		if (insideViewport) {
			std::shared_ptr<CNavmeshData> navmesh = mNavContext->GetLoadedNavmeshData(size_t(mSelectedNavmeshIndex));
			if (navmesh != nullptr) {
				glm::vec3 rayOrigin(0.0f);
				glm::vec3 rayDir(0.0f);
				if (BuildWorldRayFromMouse(mMainViewport->GetCamera(), mousePos, viewportPos, viewportSize, rayOrigin, rayDir)) {
					const int pickedPolygon = PickPolygonAtRay(*navmesh, rayOrigin, rayDir);
					if (pickedPolygon >= 0) {
						mSelectedPolygonIndex = pickedPolygon;
					}
				}
			}
		}
	}

	const ImVec2 dialogSize = GetResponsiveDialogSize(0.85f, 0.85f, 960.0f, 720.0f);

	// Render open file dialog
	if (ImGuiFileDialog::Instance()->Display("loadFileDialog", 32, dialogSize)) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			OpenFile(ImGuiFileDialog::Instance()->GetFilePathName());
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("saveTracksAsDialog", 32, dialogSize)) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			OPTIONS.mLastSavedRailroadDir = ImGuiFileDialog::Instance()->GetFilePathName();
			SaveTracksCB();
		}

		ImGuiFileDialog::Instance()->Close();
	}

	// Handle world directory loader
	if (ImGuiFileDialog::Instance()->Display("loadWorldDirDialog", 32, dialogSize)) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string selectedPath = ImGuiFileDialog::Instance()->GetCurrentPath();
			LoadWorldDirectory(selectedPath);
		}

		ImGuiFileDialog::Instance()->Close();
	}
}

void AGatorContext::PostRender(float deltaTime) {
	mMainViewport->BindViewport();

	mNavContext->Render(mMainViewport->GetCamera());
	mDrawableContext->Render(mMainViewport->GetCamera());
	mTrackContext->Render(mMainViewport->GetCamera());
	
	// Render grid lines with Blender-style fade-out
	mMainViewport->RenderGridLines();

	mMainViewport->UnbindViewport();
}

void AGatorContext::SetAppPosition(const int xPos, const int yPos) {
	mAppPosition = { xPos, yPos };
}

void AGatorContext::OpenFile(std::filesystem::path filePath) {
	if (!std::filesystem::exists(filePath) || !filePath.has_extension()) {
		return;
	}

	const bool isYnvXml = filePath.extension() == ".xml" && filePath.stem().extension() == ".ynv";

	if (filePath.extension() == ".ynv" || isYnvXml) {
		if (mNavContext->LoadNavmesh(filePath)) {
			OPTIONS.mLastOpenedDir = filePath;
		}
	}
	else if (filePath.extension() == ".ydr") {
		mDrawableContext->LoadDrawable(filePath);
	}
	else if (filePath.extension() == ".xml") {
		if (filePath.stem() == "traintracks") {
			mTrackContext->InitGLResources();
			mTrackContext->LoadTracks(filePath);

			// Set track points in viewport for new panels (Hierarchy & Properties)
			if (mTrackContext->IsLoaded() && mMainViewport) {
				auto trackPoints = mTrackContext->GetAllTrackPoints();
				if (!trackPoints.empty()) {
					mMainViewport->SetTrackPoints(trackPoints);
				}
			}

			OPTIONS.mLastOpenedDir = filePath;
			OPTIONS.mLastOpenedRailroadDir = filePath.parent_path();
		}
	}
	else if (filePath.filename() == "swrailroad.wsi") {
		RDR1Util::ExtractTrainPoints(filePath);
	}
}

void AGatorContext::LoadFileCB() {
	std::string startingDir = OPTIONS.mLastOpenedDir.empty() ? "." : OPTIONS.mLastOpenedDir.u8string();
	ImGuiFileDialog::Instance()->OpenDialog("loadFileDialog", "Open File", "traintracks.xml{.xml},Navmeshes (*.ynv, *.ynv.xml){.ynv,.xml}", startingDir, 1, nullptr, ImGuiFileDialogFlags_Modal);
}

void AGatorContext::SaveTracksAsCB() {
	std::string startingDir = OPTIONS.mLastSavedRailroadDir.empty() ? "." : OPTIONS.mLastSavedRailroadDir.u8string();
	ImGuiFileDialog::Instance()->OpenDialog("saveTracksAsDialog", "Choose Directory", nullptr, startingDir, 1, nullptr, ImGuiFileDialogFlags_Modal);
}

void AGatorContext::OnFileDropped(std::filesystem::path filePath) {
	OpenFile(filePath);
}

void AGatorContext::LoadWorldDirectory(std::filesystem::path directoryPath) {
	if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath)) {
		std::cerr << "Invalid directory: " << directoryPath << std::endl;
		return;
	}

	std::cout << "\n=== Loading RDR2 World from: " << directoryPath << " ===" << std::endl;
	
	// 1. Recursively load all .ynv (navmesh) files
	// Strategy: Look for .ynv.xml first (works with RSC8 native files), fall back to .ynv
	std::cout << "\n[1/3] Searching for navmeshes (recursive)..." << std::endl;
	int navmeshCount = 0;
	std::set<std::string> loadedBasenames;  // Track which navmeshes we've already loaded
	
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
		if (entry.is_regular_file()) {
			const auto& path = entry.path();
			
			// Prefer .ynv.xml files (these always work, even for RSC8 native files)
			if (path.extension() == ".xml" && path.stem().extension() == ".ynv") {
				std::string basename = path.stem().stem().string();  // e.g., "navmesh[123][456]"
				if (loadedBasenames.find(basename) == loadedBasenames.end()) {
					if (mNavContext->LoadNavmesh(path)) {
						loadedBasenames.insert(basename);
						navmeshCount++;
						// Progress indicator every 50 files
						if (navmeshCount % 50 == 0) {
							std::cout << "  Progress: " << navmeshCount << " navmeshes loaded..." << std::endl;
						}
					}
				}
			}
		}
	}
	
	// Fall back to native .ynv files only if no corresponding .ynv.xml exists
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
		if (entry.is_regular_file()) {
			const auto& path = entry.path();
			
			if (path.extension() == ".ynv") {
				std::string basename = path.stem().string();  // e.g., "navmesh[123][456]"
				if (loadedBasenames.find(basename) == loadedBasenames.end()) {
					// Try native first, but silently skip if it fails
					if (mNavContext->LoadNavmesh(path)) {
						loadedBasenames.insert(basename);
						navmeshCount++;
					}
				}
			}
		}
	}
	std::cout << "  Loaded " << navmeshCount << " navmesh(es)" << std::endl;
	
	// 2. Recursively load all .ydr (drawable/model) files with XML fallback
	std::cout << "\n[2/3] Searching for drawables (recursive)..." << std::endl;
	int drawableCount = 0;
	std::set<std::string> loadedDrawables;  // Track loaded drawable basenames
	
	// First pass: collect all files to understand what we have
	std::vector<std::filesystem::path> ydrXmlFiles;
	std::vector<std::filesystem::path> ydrFiles;
	
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
		if (entry.is_regular_file()) {
			const auto& path = entry.path();
			
			if (path.extension() == ".xml" && path.stem().extension() == ".ydr") {
				ydrXmlFiles.push_back(path);
			} else if (path.extension() == ".ydr") {
				ydrFiles.push_back(path);
			}
		}
	}
	
	std::cout << "  Found " << ydrXmlFiles.size() << " .ydr.xml files and " << ydrFiles.size() << " .ydr files" << std::endl;
	
	// Prefer .ydr.xml files
	for (const auto& path : ydrXmlFiles) {
		std::string basename = path.stem().stem().string();
		if (loadedDrawables.find(basename) == loadedDrawables.end()) {
			if (mDrawableContext->LoadDrawable(path)) {
				loadedDrawables.insert(basename);
				drawableCount++;
				// Progress indicator every 20 files
				if (drawableCount % 20 == 0) {
					std::cout << "  Progress: " << drawableCount << " drawables loaded..." << std::endl;
				}
			} else {
				// Log failures for debugging
				if (drawableCount % 100 == 0) {
					std::cerr << "  Failed to load: " << path.filename() << " - " << mDrawableContext->GetLastLoadError() << std::endl;
				}
			}
		}
	}
	
	// Fall back to native .ydr files
	for (const auto& path : ydrFiles) {
		std::string basename = path.stem().string();
		if (loadedDrawables.find(basename) == loadedDrawables.end()) {
			if (mDrawableContext->LoadDrawable(path)) {
				loadedDrawables.insert(basename);
				drawableCount++;
				// Progress indicator every 20 files
				if (drawableCount % 20 == 0) {
					std::cout << "  Progress: " << drawableCount << " drawables loaded..." << std::endl;
				}
			}
		}
	}
	std::cout << "  Loaded " << drawableCount << " drawable(s)" << std::endl;
	
	// 3. Recursively load all .ymap.xml (entity map) files
	std::cout << "\n[3/4] Searching for entity maps (recursive)..." << std::endl;
	int entityMapCount = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
		if (entry.is_regular_file()) {
			const auto& path = entry.path();
			
			if (path.extension() == ".xml" && path.stem().extension() == ".ymap") {
				if (mEntityContext->LoadYmap(path)) {
					entityMapCount++;
					std::cout << "  Loaded: " << path.filename() << " (" << mEntityContext->GetLoadedEntityCount() << " entities)" << std::endl;
				}
			}
		}
	}
	std::cout << "  Loaded " << entityMapCount << " entity map(s)" << std::endl;
	
	// 4. NOTE: Collision files (.ybn) are not yet supported
	std::cout << "\n[4/4] Asset loading completed" << std::endl;
	// Users load their own traintracks separately via File -> Open
	std::cout << "NOTE: Custom traintracks not loaded (load separately via File -> Open)" << std::endl;
	
	// Save the directory path for future use
	OPTIONS.mLastOpenedDir = directoryPath;
	
	// Auto-position camera to view loaded world assets
	// Center of RDR2 region based on loaded navmesh tiles
	glm::vec3 worldCenter(245.0f, 235.0f, 100.0f);  // Approximate center of navmesh tiles (192-300 X, 210-260 Y)
	glm::vec3 cameraPos = worldCenter + glm::vec3(150.0f, 150.0f, 150.0f);  // ~212 units away at 45° angle
	mMainViewport->GetCamera().SetView(cameraPos, worldCenter, glm::vec3(0.0f, 1.0f, 0.0f));
	
	std::cout << "\n=== World assets loaded successfully! ===" << std::endl;
	std::cout << "Total: " << navmeshCount << " navmeshes + " << drawableCount << " drawables + " << mEntityContext->GetLoadedEntityCount() << " entities" << std::endl;
	std::cout << "Ready to load your custom traintracks via File -> Open\n" << std::endl;
}

void AGatorContext::OnGLInitialized() {
	try {
		std::cout << "Creating Main Viewport..." << std::endl;
		mMainViewport = std::make_shared<UViewport>("Main Viewport");
		std::cout << "Main Viewport created successfully" << std::endl;

		std::cout << "Initializing NavContext..." << std::endl;
		mNavContext->OnGLInitialized();
		std::cout << "NavContext initialized successfully" << std::endl;

		// Don't create picker yet - wait until first frame when viewport has real size
		std::cout << "AGatorContext OnGLInitialized() completed successfully" << std::endl;
	}
	catch (const std::exception& ex) {
		std::cerr << "Exception in OnGLInitialized: " << ex.what() << std::endl;
		throw;
	}
}
