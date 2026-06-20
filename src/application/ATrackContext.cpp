#include "application/ATrackContext.hpp"
#include "tracks/UTrack.hpp"
#include "tracks/UTrackPoint.hpp"
#include "ubo/common.hpp"
#include "util/fileutil.hpp"
#include "util/uiutil.hpp"
#include "ui/UViewportPicker.hpp"
#include "application/AInput.hpp"
#include "ui/UPathRenderer.hpp"

#include "primitives/USphere.hpp"

#include <pugixml.hpp>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "util/ImGuizmo.hpp"

#include <iostream>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

constexpr const char* TRACKS_CHILD_NAME = "train_tracks";
constexpr const char* TRACKS_FILE_NAME = "traintracks.xml";
constexpr const char* NEW_TRACK_DIALOG_LABEL = "New Track";

constexpr uint32_t VERTEX_ATTRIB_INDEX = 0;

constexpr glm::vec4 NORMAL_COLOR = { 0.00f, 0.25f, 0.75f, 1.0f };
constexpr glm::vec4 JUNCTION_COLOR = { 0.15f, 0.85f, 0.25f, 1.0f };
constexpr glm::vec4 STATION_COLOR = { 0.95f, 0.85f, 0.20f, 1.0f };
constexpr glm::vec4 HIGHLIGHT_COLOR = { 1.0f, 0.5f, 0.0f, 1.0f };
constexpr glm::vec4 SELECTED_COLOR = { 1.0f, 0.1f, 0.2f, 1.0f };
constexpr glm::vec4 HANDLE_COLOR = { 1.0f, 0.5f, 1.0f, 1.0f };

constexpr uint32_t HANDLE_A_MASK = 0x40000000;
constexpr uint32_t HANDLE_B_MASK = 0x80000000;
constexpr float CURVE_HANDLE_SCALE = 1.0f / 3.0f;

namespace {
    bool RenderMixedCheckbox(const char* label, bool* value, bool mixed) {
        if (mixed) {
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        }

        const bool changed = ImGui::Checkbox(label, value);

        if (mixed) {
            ImGui::PopItemFlag();
        }

        return changed;
    }

    glm::vec3 BuildHandleDirection(const glm::vec3& position, const glm::vec3* previousPosition, const glm::vec3* nextPosition) {
        if (previousPosition != nullptr && nextPosition != nullptr) {
            const glm::vec3 tangent = *nextPosition - *previousPosition;
            if (glm::length(tangent) > 0.0001f) {
                return glm::normalize(tangent);
            }
        }

        if (nextPosition != nullptr) {
            const glm::vec3 tangent = *nextPosition - position;
            if (glm::length(tangent) > 0.0001f) {
                return glm::normalize(tangent);
            }
        }

        if (previousPosition != nullptr) {
            const glm::vec3 tangent = position - *previousPosition;
            if (glm::length(tangent) > 0.0001f) {
                return glm::normalize(tangent);
            }
        }

        return glm::vec3(1.0f, 0.0f, 0.0f);
    }
}

ATrackContext::ATrackContext() : mPntVBO(0), mPntIBO(0), mPntVAO(0), mSimpleProgram(0), bGLInitialized(false), mBaseColorUniform(0),
    mSelectedTrack(), mSelectedPickType(ETrackNodePickType::Position), bSelectingJunctionPartner(false), mPendingNewTrackName(""),
    bTrackDialogOpen(false), bCanDuplicatePoint(true), bWasUsingGizmo(false), bBoxSelecting(false), bBoxSelectionActive(false),
    bBoxSelectionRequireFullContainment(false), mBoxSelectionNodePickRadius(10.0f),
    mBoxSelectionStartScreen(glm::zero<glm::vec2>()), mBoxSelectionEndScreen(glm::zero<glm::vec2>())
{

}

ATrackContext::~ATrackContext() {
    DestroyGLResources();
}

void ATrackContext::InitGLResources() {
    if (bGLInitialized) {
        return;
    }

    glCreateBuffers(1, &mPntVBO);
    glCreateBuffers(1, &mPntIBO);

    glNamedBufferStorage(mPntVBO, USphere::VertexCount * sizeof(glm::vec3), USphere::Vertices, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    glNamedBufferStorage(mPntIBO, USphere::IndexCount  * sizeof(uint32_t),  USphere::Indices, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);

    glCreateVertexArrays(1, &mPntVAO);
    glVertexArrayVertexBuffer(mPntVAO, 0, mPntVBO, 0, sizeof(glm::vec3));
    glVertexArrayElementBuffer(mPntVAO, mPntIBO);

    glEnableVertexArrayAttrib(mPntVAO, VERTEX_ATTRIB_INDEX);
    glVertexArrayAttribBinding(mPntVAO, VERTEX_ATTRIB_INDEX, 0);
    glVertexArrayAttribFormat(mPntVAO, VERTEX_ATTRIB_INDEX, glm::vec3::length(), GL_FLOAT, GL_FALSE, 0);

    InitSimpleShader();

    bGLInitialized = true;
}

void ATrackContext::InitSimpleShader() {
    // Compile vertex shader
    std::string vertTxt = UFileUtil::LoadShaderText("simple.vert");
    const char* vertTxtChars = vertTxt.data();

    uint32_t vertHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertHandle, 1, &vertTxtChars, NULL);
    glCompileShader(vertHandle);

    int32_t success = 0;
    glGetShaderiv(vertHandle, GL_COMPILE_STATUS, &success);
    if (!success) {
        int32_t logSize = 0;
        glGetShaderiv(vertHandle, GL_INFO_LOG_LENGTH, &logSize);

        std::vector<char> log(logSize);
        glGetShaderInfoLog(vertHandle, logSize, nullptr, &log[0]);

        std::cout << std::string(log.data()) << std::endl;

        glDeleteShader(vertHandle);
    }

    // Compile fragment shader
    std::string fragTxt = UFileUtil::LoadShaderText("simple.frag");
    const char* fragTxtChars = fragTxt.data();

    uint32_t fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragHandle, 1, &fragTxtChars, NULL);
    glCompileShader(fragHandle);

    success = 0;
    glGetShaderiv(fragHandle, GL_COMPILE_STATUS, &success);
    if (!success) {
        int32_t logSize = 0;
        glGetShaderiv(fragHandle, GL_INFO_LOG_LENGTH, &logSize);

        std::vector<char> log(logSize);
        glGetShaderInfoLog(fragHandle, logSize, nullptr, &log[0]);

        std::cout << std::string(log.data()) << std::endl;

        glDeleteShader(fragHandle);
    }

    // Generate shader program
    mSimpleProgram = glCreateProgram();
    glAttachShader(mSimpleProgram, vertHandle);
    glAttachShader(mSimpleProgram, fragHandle);
    glLinkProgram(mSimpleProgram);

    // Clean up
    glDetachShader(mSimpleProgram, vertHandle);
    glDetachShader(mSimpleProgram, fragHandle);
    glDeleteShader(vertHandle);
    glDeleteShader(fragHandle);

    UCommonUniformBuffer::LinkShaderToUBO(mSimpleProgram);
    mBaseColorUniform = glGetUniformLocation(mSimpleProgram, "uBaseColor");
}

void ATrackContext::DestroyGLResources() {
    uint32_t buffers[]{ mPntVBO, mPntIBO };

    glDeleteBuffers(2, buffers);
    glDeleteVertexArrays(1, &mPntVAO);
    glDeleteProgram(mSimpleProgram);

    mPntVBO = 0;
    mPntIBO = 0;
    mPntVAO = 0;
    mSimpleProgram = 0;

    bGLInitialized = false;
}

void ATrackContext::LoadTracks(std::filesystem::path filePath) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filePath.c_str());

    if (!result) {
        return;
    }

    if (IsLoaded()) {
        ClearSelectedPoints();
        mTracks.clear();
        mTrackPoints.clear();
        mPathRenderers.clear();
    }

    std::filesystem::path configDir = filePath.parent_path();

    for (pugi::xml_node trackNode : doc.child(TRACKS_CHILD_NAME)) {
        std::shared_ptr<UTracks::UTrack> track = std::make_shared<UTracks::UTrack>();
        track->Deserialize(trackNode);
        mTracks.push_back(track);
    }

    std::sort(mTracks.begin(), mTracks.end(),
        [](std::shared_ptr<UTracks::UTrack> a, std::shared_ptr<UTracks::UTrack> b) {
            return a->GetConfigName() < b->GetConfigName();
        }
    );

    for (uint32_t i = 0; i < mTracks.size(); i++) {
        mTrackPoints.push_back(mTracks[i]->LoadNodePoints(configDir));

        std::shared_ptr<CPathRenderer> pathRenderer = std::make_shared<CPathRenderer>();
        pathRenderer->Init();
        for (std::shared_ptr<UTracks::UTrackPoint> pnt : mTrackPoints[i]) {
            pathRenderer->mPath.push_back({ pnt->GetPosition(), {1, 0, 0, 1,}, pnt->GetHandleA(), pnt->GetHandleB() });
        }
        pathRenderer->UpdateData();

        mPathRenderers.push_back(pathRenderer);
    }

    PostprocessNodes();

    mUndoStack.clear();
    mRedoStack.clear();
}

ATrackContext::AEditorSnapshot ATrackContext::CaptureSnapshot() const {
    AEditorSnapshot snapshot;
    snapshot.PickType = mSelectedPickType;
    snapshot.Selection = mSelectedPoints;
    snapshot.Tracks.reserve(mTracks.size());

    std::unordered_map<const UTracks::UTrackPoint*, std::pair<int, int>> pointToIndex;
    for (size_t trackIdx = 0; trackIdx < mTrackPoints.size(); trackIdx++) {
        for (size_t pointIdx = 0; pointIdx < mTrackPoints[trackIdx].size(); pointIdx++) {
            pointToIndex[mTrackPoints[trackIdx][pointIdx].get()] = { int(trackIdx), int(pointIdx) };
        }
    }

    for (size_t trackIdx = 0; trackIdx < mTracks.size(); trackIdx++) {
        ATrackState trackState;
        trackState.ConfigName = mTracks[trackIdx]->GetConfigName();
        trackState.GameFilename = mTracks[trackIdx]->GetGameFilename();
        trackState.StopsAtStations = *mTracks[trackIdx]->GetStopsAtStationsForEditor();
        trackState.Loops = *mTracks[trackIdx]->GetLoopsForEditor();
        trackState.Hidden = mTracks[trackIdx]->IsHidden();
        trackState.BrakingDist = *mTracks[trackIdx]->GetBrakingDistForEditor();

        trackState.Points.reserve(mTrackPoints[trackIdx].size());
        for (size_t pointIdx = 0; pointIdx < mTrackPoints[trackIdx].size(); pointIdx++) {
            const std::shared_ptr<UTracks::UTrackPoint>& point = mTrackPoints[trackIdx][pointIdx];

            APointState pointState;
            pointState.Position = point->GetPosition();
            pointState.HandleA = point->GetHandleA();
            pointState.HandleB = point->GetHandleB();
            pointState.Scalar = *point->GetScalarForEditor();
            pointState.StationType = point->GetStationType();
            pointState.IsTunnel = *point->GetIsTunnelForEditor();
            pointState.IsJunction = *point->GetIsJunctionForEditor();
            pointState.IsCurve = point->IsCurve();
            pointState.Argument = point->GetArgument();
            pointState.ParentTrackName = point->GetParentTrackName();
            pointState.JunctionTrackIdx = -1;
            pointState.JunctionPointIdx = -1;

            if (point->HasJunctionPartner()) {
                const std::shared_ptr<UTracks::UTrackPoint> partner = point->GetJunctionPartner().lock();
                const auto found = pointToIndex.find(partner.get());
                if (found != pointToIndex.end()) {
                    pointState.JunctionTrackIdx = found->second.first;
                    pointState.JunctionPointIdx = found->second.second;
                }
            }

            trackState.Points.push_back(std::move(pointState));
        }

        snapshot.Tracks.push_back(std::move(trackState));
    }

    return snapshot;
}

void ATrackContext::SyncPathRenderersFromTrackPoints() {
    for (size_t trackIdx = 0; trackIdx < mPathRenderers.size() && trackIdx < mTrackPoints.size(); trackIdx++) {
        mPathRenderers[trackIdx]->mPath.clear();
        mPathRenderers[trackIdx]->mPath.reserve(mTrackPoints[trackIdx].size());

        for (const std::shared_ptr<UTracks::UTrackPoint>& point : mTrackPoints[trackIdx]) {
            mPathRenderers[trackIdx]->mPath.push_back({ point->GetPosition(), { 1, 0, 0, 1 }, point->GetHandleA(), point->GetHandleB() });
        }

        mPathRenderers[trackIdx]->UpdateData();
    }
}

void ATrackContext::SetCurveState(uint16_t trackIdx, uint16_t pointIdx, bool isCurve) {
    if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
        return;
    }

    std::shared_ptr<UTracks::UTrackPoint> point = mTrackPoints[trackIdx][pointIdx];
    *point->GetIsCurveForEditor() = isCurve;

    if (isCurve) {
        RecalculateCurveHandles(trackIdx, pointIdx);
        return;
    }

    const glm::vec3& position = point->GetPosition();
    point->GetHandleAForEditor() = position;
    point->GetHandleBForEditor() = position;
}

void ATrackContext::RecalculateCurveHandles(uint16_t trackIdx, uint16_t pointIdx) {
    if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
        return;
    }

    shared_vector<UTracks::UTrackPoint>& trackPoints = mTrackPoints[trackIdx];
    std::shared_ptr<UTracks::UTrackPoint> point = trackPoints[pointIdx];
    const glm::vec3& position = point->GetPosition();

    if (trackPoints.size() < 2) {
        point->GetHandleAForEditor() = position;
        point->GetHandleBForEditor() = position;
        return;
    }

    const bool loops = trackIdx < mTracks.size() && *mTracks[trackIdx]->GetLoopsForEditor();
    const size_t currentIdx = pointIdx;
    const size_t pointCount = trackPoints.size();

    const bool hasPrevious = loops || currentIdx > 0;
    const bool hasNext = loops || currentIdx + 1 < pointCount;

    const glm::vec3* previousPosition = hasPrevious ? &trackPoints[(currentIdx + pointCount - 1) % pointCount]->GetPosition() : nullptr;
    const glm::vec3* nextPosition = hasNext ? &trackPoints[(currentIdx + 1) % pointCount]->GetPosition() : nullptr;

    const glm::vec3 handleDirection = BuildHandleDirection(position, previousPosition, nextPosition);

    const float previousDistance = previousPosition != nullptr ? glm::distance(position, *previousPosition) : 0.0f;
    const float nextDistance = nextPosition != nullptr ? glm::distance(position, *nextPosition) : 0.0f;

    if (previousPosition != nullptr) {
        point->GetHandleAForEditor() = position - handleDirection * (previousDistance * CURVE_HANDLE_SCALE);
    }
    else {
        point->GetHandleAForEditor() = position;
    }

    if (nextPosition != nullptr) {
        point->GetHandleBForEditor() = position + handleDirection * (nextDistance * CURVE_HANDLE_SCALE);
    }
    else {
        point->GetHandleBForEditor() = position;
    }
}

void ATrackContext::InsertNodeRelative(uint16_t trackIdx, uint16_t pointIdx, bool insertAfter) {
    if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
        return;
    }

    const std::shared_ptr<UTracks::UTrackPoint>& sourcePoint = mTrackPoints[trackIdx][pointIdx];
    std::shared_ptr<UTracks::UTrackPoint> newPoint = std::make_shared<UTracks::UTrackPoint>(sourcePoint->GetParentTrackName());

    newPoint->SetPosition(sourcePoint->GetPosition());
    newPoint->GetHandleAForEditor() = sourcePoint->GetHandleA();
    newPoint->GetHandleBForEditor() = sourcePoint->GetHandleB();
    *newPoint->GetScalarForEditor() = *sourcePoint->GetScalarForEditor();
    newPoint->GetStationTypeForEditor() = sourcePoint->GetStationType();
    *newPoint->GetIsTunnelForEditor() = *sourcePoint->GetIsTunnelForEditor();
    *newPoint->GetIsCurveForEditor() = *sourcePoint->GetIsCurveForEditor();
    *newPoint->GetIsJunctionForEditor() = false;
    *newPoint->GetArgumentForEditor() = (sourcePoint->GetStationType() != UTracks::ENodeStationType::None) ? sourcePoint->GetArgument() : "";

    const uint16_t insertIdx = insertAfter ? uint16_t(pointIdx + 1) : pointIdx;
    mTrackPoints[trackIdx].insert(mTrackPoints[trackIdx].begin() + insertIdx, newPoint);

    ClearSelectedPoints();
    mSelectedPoints.push_back({ trackIdx, insertIdx });
    newPoint->SetSelected(true);

    SyncPathRenderersFromTrackPoints();
}

void ATrackContext::DeleteSelectedNode(uint16_t trackIdx, uint16_t pointIdx) {
    if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
        return;
    }

    if (mTrackPoints[trackIdx].size() <= 1) {
        return;
    }

    std::shared_ptr<UTracks::UTrackPoint> point = mTrackPoints[trackIdx][pointIdx];
    if (point->HasJunctionPartner()) {
        point->SetJunctionPartner(nullptr);
    }

    mTrackPoints[trackIdx].erase(mTrackPoints[trackIdx].begin() + pointIdx);

    ClearSelectedPoints();
    const uint16_t nextIdx = std::min<uint16_t>(pointIdx, uint16_t(mTrackPoints[trackIdx].size() - 1));
    mSelectedPoints.push_back({ trackIdx, nextIdx });
    mTrackPoints[trackIdx][nextIdx]->SetSelected(true);

    SyncPathRenderersFromTrackPoints();
}

void ATrackContext::RestoreSnapshot(const AEditorSnapshot& snapshot) {
    ClearSelectedPoints();
    mTracks.clear();
    mTrackPoints.clear();
    mPathRenderers.clear();
    mSelectedTrack.reset();

    mTracks.reserve(snapshot.Tracks.size());
    mTrackPoints.reserve(snapshot.Tracks.size());
    mPathRenderers.reserve(snapshot.Tracks.size());

    for (const ATrackState& trackState : snapshot.Tracks) {
        std::shared_ptr<UTracks::UTrack> track = std::make_shared<UTracks::UTrack>();
        *track->GetConfigNameForEditor() = trackState.ConfigName;
        *track->GetGameFilenameForEditor() = trackState.GameFilename;
        *track->GetStopsAtStationsForEditor() = trackState.StopsAtStations;
        *track->GetLoopsForEditor() = trackState.Loops;
        *track->GetBrakingDistForEditor() = trackState.BrakingDist;
        track->SetHidden(trackState.Hidden);
        mTracks.push_back(track);

        shared_vector<UTracks::UTrackPoint> points;
        points.reserve(trackState.Points.size());

        for (const APointState& pointState : trackState.Points) {
            std::shared_ptr<UTracks::UTrackPoint> point = std::make_shared<UTracks::UTrackPoint>(pointState.ParentTrackName);
            point->SetPosition(pointState.Position);
            point->GetHandleAForEditor() = pointState.HandleA;
            point->GetHandleBForEditor() = pointState.HandleB;
            *point->GetScalarForEditor() = pointState.Scalar;
            point->GetStationTypeForEditor() = pointState.StationType;
            *point->GetIsTunnelForEditor() = pointState.IsTunnel;
            *point->GetIsJunctionForEditor() = pointState.IsJunction;
            *point->GetIsCurveForEditor() = pointState.IsCurve;
            *point->GetArgumentForEditor() = pointState.Argument;
            point->SetSelected(false);

            points.push_back(point);
        }

        mTrackPoints.push_back(std::move(points));

        std::shared_ptr<CPathRenderer> pathRenderer = std::make_shared<CPathRenderer>();
        pathRenderer->Init();
        mPathRenderers.push_back(pathRenderer);
    }

    for (size_t trackIdx = 0; trackIdx < snapshot.Tracks.size(); trackIdx++) {
        for (size_t pointIdx = 0; pointIdx < snapshot.Tracks[trackIdx].Points.size(); pointIdx++) {
            const APointState& pointState = snapshot.Tracks[trackIdx].Points[pointIdx];

            if (pointState.JunctionTrackIdx < 0 || pointState.JunctionPointIdx < 0) {
                continue;
            }

            const size_t partnerTrackIdx = size_t(pointState.JunctionTrackIdx);
            const size_t partnerPointIdx = size_t(pointState.JunctionPointIdx);

            if (partnerTrackIdx >= mTrackPoints.size() || partnerPointIdx >= mTrackPoints[partnerTrackIdx].size()) {
                continue;
            }

            mTrackPoints[trackIdx][pointIdx]->SetJunctionPartner(mTrackPoints[partnerTrackIdx][partnerPointIdx]);
        }
    }

    mSelectedPickType = snapshot.PickType;
    for (const APointSelection& sel : snapshot.Selection) {
        if (sel.TrackIdx < mTrackPoints.size() && sel.PointIdx < mTrackPoints[sel.TrackIdx].size()) {
            mSelectedPoints.push_back(sel);
            mTrackPoints[sel.TrackIdx][sel.PointIdx]->SetSelected(true);
        }
    }

    SyncPathRenderersFromTrackPoints();
}

void ATrackContext::PushUndoSnapshot() {
    if (!IsLoaded()) {
        return;
    }

    mUndoStack.push_back(CaptureSnapshot());
    if (mUndoStack.size() > MAX_HISTORY_STATES) {
        mUndoStack.erase(mUndoStack.begin());
    }

    mRedoStack.clear();
}

bool ATrackContext::Undo() {
    if (mUndoStack.empty()) {
        return false;
    }

    mRedoStack.push_back(CaptureSnapshot());
    RestoreSnapshot(mUndoStack.back());
    mUndoStack.pop_back();
    return true;
}

bool ATrackContext::Redo() {
    if (mRedoStack.empty()) {
        return false;
    }

    mUndoStack.push_back(CaptureSnapshot());
    RestoreSnapshot(mRedoStack.back());
    mRedoStack.pop_back();
    return true;
}

void ATrackContext::HandleUndoRedoShortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }

    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift) {
            Undo();
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) {
            Redo();
        }
    }

    if (mSelectedPoints.size() != 1) {
        return;
    }

    uint16_t trackIdx = 0;
    uint16_t pointIdx = 0;
    mSelectedPoints[0].Get(trackIdx, pointIdx);

    if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Insert, false)) {
        PushUndoSnapshot();
        InsertNodeRelative(trackIdx, pointIdx, !io.KeyShift);
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && mTrackPoints[trackIdx].size() > 1) {
        PushUndoSnapshot();
        DeleteSelectedNode(trackIdx, pointIdx);
    }
}

void ATrackContext::PostprocessNodes() {
    for (shared_vector<UTracks::UTrackPoint> trackPoints : mTrackPoints) {
        for (const std::shared_ptr<UTracks::UTrackPoint> pnt : trackPoints) {
            if (!pnt->IsJunction() || !pnt->GetJunctionPartner().expired()) {
                continue;
            }

            for (uint32_t trackIdx = 0; trackIdx < mTracks.size(); trackIdx++) {
                if (mTracks[trackIdx]->GetConfigName() != pnt->GetArgument()) {
                    continue;
                }

                glm::vec3 curPntPos = pnt->GetPosition();

                for (std::shared_ptr<UTracks::UTrackPoint> otherPnt : mTrackPoints[trackIdx]) {
                    float dist = glm::distance(curPntPos, otherPnt->GetPosition());

                    if (dist <= 0.0001f) {
                        pnt->SetJunctionPartner(otherPnt);
                        otherPnt->SetJunctionPartner(pnt);
                    }
                }

                break;
            }
        }
    }
}

void ATrackContext::SaveTracks(std::filesystem::path dirPath) {
    std::filesystem::path fullConfigPath = dirPath / TRACKS_FILE_NAME;
    pugi::xml_document doc;
    doc.document_element().append_attribute("encoding").set_value("UTF-8");

    pugi::xml_node rootNode = doc.append_child(TRACKS_CHILD_NAME);
    rootNode.append_attribute("version").set_value("1");

    for (std::shared_ptr<UTracks::UTrack> track : mTracks) {
        pugi::xml_node trackNode = rootNode.append_child("train_track");
        track->Serialize(trackNode);
    }

    doc.save_file(fullConfigPath.c_str(), PUGIXML_TEXT("\t"), pugi::format_indent | pugi::format_indent_attributes | pugi::format_save_file_text, pugi::encoding_utf8);

    for (uint32_t trackIdx = 0; trackIdx < mTrackPoints.size(); trackIdx++) {
        mTracks[trackIdx]->SaveNodePoints(dirPath, mTrackPoints[trackIdx]);
    }
}

void ATrackContext::RenderTreeView() {
    if (mTracks.size() == 0) {
        ImGui::Text("Please load traintracks.xml.");
        return;
    }

    bool treeNodeOpen = ImGui::TreeNodeEx("Track Configs", ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PushID("Track Configs");
    
    if (ImGui::BeginPopupContextItem("Context Menu"))
    {
        if (ImGui::MenuItem("Add track...")) {
            bTrackDialogOpen = true;
        }

        ImGui::EndPopup();
    }
    
    ImGui::PopID();

    if (bTrackDialogOpen) {
        ImGui::OpenPopup(NEW_TRACK_DIALOG_LABEL);
        bTrackDialogOpen = false;
    }

    RenderNewTrackDialog();

    if (treeNodeOpen) {
        ImGui::Indent();

        for (uint32_t i = 0; i < mTracks.size(); i++) {
            std::shared_ptr<UTracks::UTrack> track = mTracks[i];
            ImGui::PushID(i);

            if (track->IsHidden()) {
                if (ImGui::Button("Show", { 40, 0 })) {
                    track->SetHidden(false);
                }
            }
            else {
                if (ImGui::Button("Hide", { 40, 0 })) {
                    track->SetHidden(true);
                }
            }

            ImGui::SameLine();

            bool isSelected = !mSelectedTrack.expired() && mSelectedTrack.lock() == track;
            if (ImGui::Selectable(track->GetConfigName().c_str(), isSelected)) {
                mSelectedTrack = track;
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }
}

void ATrackContext::RenderDataEditor() {
    if (ImGui::CollapsingHeader("Selection Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::Checkbox("Box select: fully enclosed only", &bBoxSelectionRequireFullContainment);
        ImGui::SetNextItemWidth(220.0f);
        ImGui::SliderFloat("Box select touch radius (px)", &mBoxSelectionNodePickRadius, 2.0f, 30.0f, "%.1f");
        ImGui::TextDisabled("Selection modifiers: Shift = add, Ctrl = toggle, no modifier = replace");
        ImGui::Unindent();
        ImGui::Spacing();
    }

    if (!mSelectedTrack.expired()) {
        RenderTrackDataEditor(mSelectedTrack.lock());
        ImGui::Spacing();
    }

    if (mSelectedPoints.size() == 1) {
        uint16_t trackIdx, pointIdx;
        mSelectedPoints[0].Get(trackIdx, pointIdx);

        if (trackIdx < mTrackPoints.size() && pointIdx < mTrackPoints[trackIdx].size()) {
            RenderPointDataEditorSingle(mTrackPoints[trackIdx][pointIdx], trackIdx, pointIdx);
        }
        else {
            ClearSelectedPoints();
        }
    }
    else if (mSelectedPoints.size() > 1) {
        RenderPointDataEditorMulti();
    }
}

void ATrackContext::RenderTrackDataEditor(std::shared_ptr<UTracks::UTrack> track) {
    if (ImGui::CollapsingHeader("Selected Track Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Spacing();
        UIUtil::RenderTextInput("Name", track->GetConfigNameForEditor(), 0);

        ImGui::Spacing();
        UIUtil::RenderTextInput("DAT file", track->GetGameFilenameForEditor(), 0);
        if (!track->GetGameFilenameForEditor()->empty()) {
            std::filesystem::path datPath(*track->GetGameFilenameForEditor());
            if (datPath.extension() != ".dat") {
                datPath.replace_extension(".dat");
                *track->GetGameFilenameForEditor() = datPath.generic_string();
            }
        }

        ImGui::Spacing();
        ImGui::InputScalar("Braking Distance", ImGuiDataType_U32, track->GetBrakingDistForEditor());

        ImGui::Spacing();
        ImGui::Checkbox("Loops?", track->GetLoopsForEditor());

        ImGui::Spacing();
        ImGui::Checkbox("Stops at stations?", track->GetStopsAtStationsForEditor());

        ImGui::Spacing();
        ImGui::TextDisabled("Tip: Existing DAT bearbeiten oder neuen DAT-Namen eintragen (wird beim Speichern erzeugt).\nShortcuts: Insert=Add After, Shift+Insert=Add Before, Delete=Remove Node");

        ImGui::Unindent();
    }
}

void ATrackContext::RenderPointDataEditorSingle(std::shared_ptr<UTracks::UTrackPoint> point, uint16_t trackIdx, uint16_t pointIdx) {
    if (ImGui::CollapsingHeader("Selected Node Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        if (trackIdx < mTracks.size()) {
            const std::string& trackConfigName = mTracks[trackIdx]->GetConfigName();
            const std::string& datPath = mTracks[trackIdx]->GetGameFilename();

            std::string datFileName = datPath;
            const size_t slashPos = datFileName.find_last_of("/\\");
            if (slashPos != std::string::npos && slashPos + 1 < datFileName.size()) {
                datFileName = datFileName.substr(slashPos + 1);
            }

            ImGui::Text("Track Config:");
            ImGui::SameLine();
            ImGui::TextUnformatted(trackConfigName.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy Config")) {
                ImGui::SetClipboardText(trackConfigName.c_str());
            }

            ImGui::Text("Track DAT File:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "%s", datFileName.c_str());

            ImGui::Text("Track DAT Path:");
            ImGui::SameLine();
            ImGui::TextUnformatted(datPath.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy DAT Path")) {
                ImGui::SetClipboardText(datPath.c_str());
            }
            ImGui::Spacing();
        }

        ImGui::Spacing();
        UIUtil::RenderComboEnum<UTracks::ENodeStationType>("Station Type", point->GetStationTypeForEditor());

        ImGui::Spacing();

        // Only nodes with station type None can be junctions, so show UI for one or the other.
        if (point->GetStationType() == UTracks::ENodeStationType::None) {
            if (!point->HasJunctionPartner()) {
                ImGui::Text("No Junction");
                ImGui::SameLine();
                if (ImGui::Button("Choose Junction")) {
                    bSelectingJunctionPartner = true;
                }
            }
            else {
                ImGui::Text("Junction between:");
                ImGui::Text("%s", point->GetParentTrackName().data());
                ImGui::Text("and");
                ImGui::Text("%s", point->GetJunctionPartner().lock()->GetParentTrackName().data());

                if (ImGui::Button("Clear Junction")) {
                    PushUndoSnapshot();
                    point->SetJunctionPartner(nullptr);
                    SyncPathRenderersFromTrackPoints();
                }
            }
        }
        else {
            UIUtil::RenderTextInput("Station Name", point->GetArgumentForEditor(), 0);
        }

        ImGui::Spacing();
        ImGui::Checkbox("Is in a tunnel?", point->GetIsTunnelForEditor());

        ImGui::Spacing();
        bool isCurve = point->IsCurve();
        if (ImGui::Checkbox("Is curve?", &isCurve)) {
            PushUndoSnapshot();
            SetCurveState(trackIdx, pointIdx, isCurve);
            SyncPathRenderersFromTrackPoints();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("History");

        if (mUndoStack.empty()) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Undo", { 100, 0 })) {
            Undo();
        }
        if (mUndoStack.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (mRedoStack.empty()) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Redo", { 100, 0 })) {
            Redo();
        }
        if (mRedoStack.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Node Actions");

        uint16_t actionPointIdx = pointIdx;
        bool hasSelectedPoint = false;
        if (!mSelectedPoints.empty()) {
            uint16_t selectedTrackIdx = 0;
            mSelectedPoints[0].Get(selectedTrackIdx, actionPointIdx);
            hasSelectedPoint = (selectedTrackIdx == trackIdx && actionPointIdx < mTrackPoints[trackIdx].size());
        }

        if (!hasSelectedPoint) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Add Node Before", { 140, 0 }) && hasSelectedPoint) {
            PushUndoSnapshot();
            InsertNodeRelative(trackIdx, actionPointIdx, false);
        }

        ImGui::SameLine();
        if (ImGui::Button("Add Node After", { 140, 0 }) && hasSelectedPoint) {
            PushUndoSnapshot();
            InsertNodeRelative(trackIdx, actionPointIdx, true);
        }

        const bool canDelete = hasSelectedPoint && mTrackPoints[trackIdx].size() > 1;
        if (!canDelete) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Delete Node", { 140, 0 }) && canDelete) {
            PushUndoSnapshot();
            DeleteSelectedNode(trackIdx, actionPointIdx);
        }

        if (!canDelete) {
            ImGui::EndDisabled();
        }

        if (!hasSelectedPoint) {
            ImGui::EndDisabled();
        }

        ImGui::Unindent();
    }
}

void ATrackContext::RenderPointDataEditorMulti() {
    if (ImGui::CollapsingHeader("Selected Node Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        size_t validSelectionCount = 0;
        size_t selectedCurveCount = 0;
        bool anyTunnel = false;
        bool allTunnel = true;
        bool anyCurve = false;
        bool allCurve = true;

        for (const APointSelection& selection : mSelectedPoints) {
            if (selection.TrackIdx >= mTrackPoints.size() || selection.PointIdx >= mTrackPoints[selection.TrackIdx].size()) {
                continue;
            }

            const std::shared_ptr<UTracks::UTrackPoint>& point = mTrackPoints[selection.TrackIdx][selection.PointIdx];
            const bool isTunnel = *point->GetIsTunnelForEditor();
            const bool isCurve = point->IsCurve();

            validSelectionCount++;
            selectedCurveCount += isCurve ? 1 : 0;
            anyTunnel = anyTunnel || isTunnel;
            allTunnel = allTunnel && isTunnel;
            anyCurve = anyCurve || isCurve;
            allCurve = allCurve && isCurve;
        }

        if (validSelectionCount == 0) {
            ClearSelectedPoints();
            ImGui::Unindent();
            return;
        }

        ImGui::Text("%zu nodes selected", validSelectionCount);

        ImGui::Spacing();
        bool tunnelValue = allTunnel;
        if (RenderMixedCheckbox("Is in a tunnel?", &tunnelValue, anyTunnel != allTunnel)) {
            PushUndoSnapshot();

            for (const APointSelection& selection : mSelectedPoints) {
                if (selection.TrackIdx >= mTrackPoints.size() || selection.PointIdx >= mTrackPoints[selection.TrackIdx].size()) {
                    continue;
                }

                *mTrackPoints[selection.TrackIdx][selection.PointIdx]->GetIsTunnelForEditor() = tunnelValue;
            }
        }

        ImGui::Spacing();
        bool curveValue = allCurve;
        if (RenderMixedCheckbox("Is curve?", &curveValue, anyCurve != allCurve)) {
            PushUndoSnapshot();

            for (const APointSelection& selection : mSelectedPoints) {
                SetCurveState(selection.TrackIdx, selection.PointIdx, curveValue);
            }

            SyncPathRenderersFromTrackPoints();
        }

        ImGui::Spacing();
        if (selectedCurveCount == 0) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Recalculate Curve Handles", { 220, 0 }) && selectedCurveCount > 0) {
            PushUndoSnapshot();

            for (const APointSelection& selection : mSelectedPoints) {
                if (selection.TrackIdx >= mTrackPoints.size() || selection.PointIdx >= mTrackPoints[selection.TrackIdx].size()) {
                    continue;
                }

                if (mTrackPoints[selection.TrackIdx][selection.PointIdx]->IsCurve()) {
                    RecalculateCurveHandles(selection.TrackIdx, selection.PointIdx);
                }
            }

            SyncPathRenderersFromTrackPoints();
        }

        if (selectedCurveCount == 0) {
            ImGui::EndDisabled();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("History");

        if (mUndoStack.empty()) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Undo", { 100, 0 })) {
            Undo();
        }
        if (mUndoStack.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (mRedoStack.empty()) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Redo", { 100, 0 })) {
            Redo();
        }
        if (mRedoStack.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::Unindent();
    }
}

void ATrackContext::RenderNewTrackDialog() {
    bool open = true;
    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    const ImVec2 workSize = mainViewport != nullptr ? mainViewport->WorkSize : ImVec2(800.0f, 600.0f);
    ImGui::SetNextWindowSize({ std::clamp(workSize.x * 0.35f, 280.0f, 420.0f), 0.0f });
    if (ImGui::BeginPopupModal(NEW_TRACK_DIALOG_LABEL, &open)) {
        ImGui::Text("New track name:");
        UIUtil::RenderTextInput("##pendingTrackName", &mPendingNewTrackName, 0);

        if (mPendingNewTrackName.empty()) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("OK", { 100, 0 })) {
            // Create new track
            std::shared_ptr<UTracks::UTrack> newTrack = std::make_shared<UTracks::UTrack>(mPendingNewTrackName);
            mTracks.push_back(newTrack);

            // Create track points vector
            shared_vector<UTracks::UTrackPoint> newTrackPoints;
            std::shared_ptr<UTracks::UTrackPoint> newPoint = std::make_shared<UTracks::UTrackPoint>(newTrack->GetConfigName());
            newTrackPoints.push_back(newPoint);
            mTrackPoints.push_back(newTrackPoints);

            // Create path renderer
            std::shared_ptr<CPathRenderer> pathRenderer = std::make_shared<CPathRenderer>();
            pathRenderer->Init();
            for (std::shared_ptr<UTracks::UTrackPoint> pnt : newTrackPoints) {
                pathRenderer->mPath.push_back({ pnt->GetPosition(), {1, 0, 0, 1,}, pnt->GetHandleA(), pnt->GetHandleB() });
            }
            pathRenderer->UpdateData();
            mPathRenderers.push_back(pathRenderer);

            ImGui::CloseCurrentPopup();
        }
        if (mPendingNewTrackName.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", { 100, 0 })) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ATrackContext::RenderUI(ASceneCamera& camera, const glm::vec2& viewportPos, const glm::vec2& viewportSize) {
    HandleViewportMouse(camera, viewportPos, viewportSize);
    DrawBoxSelectionOverlay();

    if (mSelectedPoints.size() == 0) {
        bWasUsingGizmo = false;
        return;
    }

    const bool isUsingGizmo = ImGuizmo::IsUsing();
    if (isUsingGizmo && !bWasUsingGizmo) {
        PushUndoSnapshot();
    }
    bWasUsingGizmo = isUsingGizmo;

    if (!isUsingGizmo) {
        bCanDuplicatePoint = true;
    }

    glm::mat4 projMtx = camera.GetProjectionMatrix();
    glm::mat4 viewMtx = camera.GetViewMatrix();

    uint16_t trackIdx, pointIdx;
    mSelectedPoints[0].Get(trackIdx, pointIdx);

    if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
        ClearSelectedPoints();
        return;
    }

    std::shared_ptr<UTracks::UTrackPoint> selPoint = mTrackPoints[trackIdx][pointIdx];

    bool bUpdated = false;
    bool bRequireFullPathSync = false;
    switch (mSelectedPickType) {
        case ETrackNodePickType::Position:
        {
            glm::vec3 avgPosition = glm::zero<glm::vec3>();

            if (mSelectedPoints.size() <= 100) {
                for (const APointSelection& s : mSelectedPoints) {
                    if (s.TrackIdx >= mTrackPoints.size() || s.PointIdx >= mTrackPoints[s.TrackIdx].size()) {
                        continue;
                    }
                    avgPosition += mTrackPoints[s.TrackIdx][s.PointIdx]->GetPosition();
                }

                avgPosition /= mSelectedPoints.size();
            }

            glm::mat4 modelMtx = glm::translate(glm::identity<glm::mat4>(), avgPosition);

            if (ImGuizmo::Manipulate(&viewMtx[0][0], &projMtx[0][0], ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::WORLD, &modelMtx[0][0])) {
                if (mSelectedPoints.size() == 1 && ImGui::GetIO().KeyCtrl && bCanDuplicatePoint) {
                    bCanDuplicatePoint = false;
                    uint16_t insertIdx = pointIdx + 1;

                    std::shared_ptr<UTracks::UTrackPoint> newPt = std::make_shared<UTracks::UTrackPoint>(*selPoint);
                    for (uint32_t i = 0; i < mTracks.size(); i++) {
                        if (mTracks[i]->GetConfigName() == newPt->GetParentTrackName()) {
                            mTrackPoints[i].insert(mTrackPoints[i].begin() + insertIdx, newPt);

                            auto f = mPathRenderers[i]->mPath.begin() + insertIdx;
                            mPathRenderers[i]->mPath.insert(f, { newPt->GetPosition(), {1, 0, 0, 1}, newPt->GetHandleA(), newPt->GetHandleB() });
                            mPathRenderers[i]->UpdateData();

                            break;
                        }
                    }

                    ClearSelectedPoints();
                    mSelectedPoints.push_back({ trackIdx, insertIdx });

                    newPt->SetSelected(true);
                    selPoint = newPt;
                }

                glm::vec3 diff = glm::vec3(modelMtx[3]) - avgPosition;
                for (const APointSelection& s : mSelectedPoints) {
                    if (s.TrackIdx >= mTrackPoints.size() || s.PointIdx >= mTrackPoints[s.TrackIdx].size()) {
                        continue;
                    }

                    std::shared_ptr<UTracks::UTrackPoint> pnt = mTrackPoints[s.TrackIdx][s.PointIdx];

                    pnt->GetPositionForEditor() += diff;

                    pnt->GetHandleAForEditor() += diff;
                    pnt->GetHandleBForEditor() += diff;

                    if (pnt->HasJunctionPartner()) {
                        std::shared_ptr<UTracks::UTrackPoint> partner = pnt->GetJunctionPartner().lock();
                        partner->GetPositionForEditor() += diff;

                        partner->GetHandleAForEditor() += diff;
                        partner->GetHandleBForEditor() += diff;
                    }
                }

                if (mSelectedPoints.size() > 1) {
                    RecalculateCurveNeighborhoodForSelection();
                    bRequireFullPathSync = true;
                }

                bUpdated = true;
            }

            break;
        }
        case ETrackNodePickType::Handle_A:
        {
            glm::mat4 modelMtx = glm::translate(glm::identity<glm::mat4>(), selPoint->GetHandleA());

            if (ImGuizmo::Manipulate(&viewMtx[0][0], &projMtx[0][0], ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::WORLD, &modelMtx[0][0])) {
                selPoint->GetHandleAForEditor() = glm::vec3(modelMtx[3]);

                if (ImGui::GetIO().KeyShift) {
                    glm::vec3 pointSpaceDelta = selPoint->GetHandleA() - selPoint->GetPosition();
                    selPoint->GetHandleBForEditor() = -pointSpaceDelta + selPoint->GetPosition();
                }

                if (selPoint->HasJunctionPartner()) {
                    std::shared_ptr<UTracks::UTrackPoint> partner = selPoint->GetJunctionPartner().lock();
                    partner->GetHandleAForEditor() = glm::vec3(modelMtx[3]);
                }
            }

            bUpdated = true;
            break;
        }
        case ETrackNodePickType::Handle_B:
        {
            glm::mat4 modelMtx = glm::translate(glm::identity<glm::mat4>(), selPoint->GetHandleB());

            if (ImGuizmo::Manipulate(&viewMtx[0][0], &projMtx[0][0], ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::WORLD, &modelMtx[0][0])) {
                selPoint->GetHandleBForEditor() = glm::vec3(modelMtx[3]);

                if (ImGui::GetIO().KeyShift) {
                    glm::vec3 pointSpaceDelta = selPoint->GetHandleB() - selPoint->GetPosition();
                    selPoint->GetHandleAForEditor() = -pointSpaceDelta + selPoint->GetPosition();
                }

                if (selPoint->HasJunctionPartner()) {
                    std::shared_ptr<UTracks::UTrackPoint> partner = selPoint->GetJunctionPartner().lock();
                    partner->GetHandleBForEditor() = glm::vec3(modelMtx[3]);
                }
            }

            bUpdated = true;
            break;
        }
    }

    if (bUpdated && bRequireFullPathSync) {
        SyncPathRenderersFromTrackPoints();
    }
    else if (bUpdated) {
        for (const APointSelection& s : mSelectedPoints) {
            if (s.TrackIdx >= mTrackPoints.size() || s.TrackIdx >= mPathRenderers.size() || s.PointIdx >= mTrackPoints[s.TrackIdx].size() || s.PointIdx >= mPathRenderers[s.TrackIdx]->mPath.size()) {
                continue;
            }

            CPathPoint& p = mPathRenderers[s.TrackIdx]->mPath[s.PointIdx];
            p.Position = mTrackPoints[s.TrackIdx][s.PointIdx]->GetPosition();
            p.LeftHandle = mTrackPoints[s.TrackIdx][s.PointIdx]->GetHandleA();
            p.RightHandle = mTrackPoints[s.TrackIdx][s.PointIdx]->GetHandleB();

            mPathRenderers[s.TrackIdx]->UpdateData();
        }
    }
}

void ATrackContext::HandleViewportMouse(ASceneCamera& camera, const glm::vec2& viewportPos, const glm::vec2& viewportSize) {
    if (mTracks.empty() || viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
        bBoxSelecting = false;
        bBoxSelectionActive = false;
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const ImRect viewportRect(
        ImVec2(viewportPos.x, viewportPos.y),
        ImVec2(viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y)
    );

    const bool isMouseInViewport = viewportRect.Contains(io.MousePos);
    if (!isMouseInViewport && !bBoxSelecting) {
        return;
    }

    const auto queryBufferPos = [&]() {
        const int32_t bufferX = int32_t(std::clamp(io.MousePos.x - viewportPos.x, 0.0f, viewportSize.x - 1.0f));
        const int32_t bufferY = int32_t(std::clamp(viewportSize.y - (io.MousePos.y - viewportPos.y), 0.0f, viewportSize.y - 1.0f));
        return std::pair<int32_t, int32_t>(bufferX, bufferY);
    };

    if (isMouseInViewport && !ImGuizmo::IsUsing() && !bBoxSelecting) {
        const auto [bufferX, bufferY] = queryBufferPos();
        OnMouseHover(camera, bufferX, bufferY);
    }

    if (isMouseInViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
        bBoxSelecting = true;
        bBoxSelectionActive = false;
        mBoxSelectionStartScreen = { io.MousePos.x, io.MousePos.y };
        mBoxSelectionEndScreen = mBoxSelectionStartScreen;
    }

    if (bBoxSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        mBoxSelectionEndScreen = { io.MousePos.x, io.MousePos.y };
        const glm::vec2 delta = mBoxSelectionEndScreen - mBoxSelectionStartScreen;
        bBoxSelectionActive = glm::dot(delta, delta) > 16.0f;
    }

    if (bBoxSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (bBoxSelectionActive) {
            ApplyBoxSelection(camera, viewportPos, viewportSize, io.KeyShift, io.KeyCtrl);
        }
        else if (isMouseInViewport) {
            const auto [bufferX, bufferY] = queryBufferPos();
            OnMouseClick(camera, bufferX, bufferY);
        }

        bBoxSelecting = false;
        bBoxSelectionActive = false;
    }
}

void ATrackContext::ApplyBoxSelection(ASceneCamera& camera, const glm::vec2& viewportPos, const glm::vec2& viewportSize, bool additive, bool toggle) {
    const float minX = std::min(mBoxSelectionStartScreen.x, mBoxSelectionEndScreen.x);
    const float maxX = std::max(mBoxSelectionStartScreen.x, mBoxSelectionEndScreen.x);
    const float minY = std::min(mBoxSelectionStartScreen.y, mBoxSelectionEndScreen.y);
    const float maxY = std::max(mBoxSelectionStartScreen.y, mBoxSelectionEndScreen.y);

    if (!additive && !toggle) {
        ClearSelectedPoints();
    }

    mSelectedPickType = ETrackNodePickType::Position;

    for (uint16_t trackIdx = 0; trackIdx < mTrackPoints.size(); trackIdx++) {
        if (trackIdx < mTracks.size() && mTracks[trackIdx]->IsHidden()) {
            continue;
        }

        for (uint16_t pointIdx = 0; pointIdx < mTrackPoints[trackIdx].size(); pointIdx++) {
            glm::vec2 screenPos = glm::zero<glm::vec2>();
            if (!ProjectPointToViewport(camera, mTrackPoints[trackIdx][pointIdx]->GetPosition(), viewportPos, viewportSize, screenPos)) {
                continue;
            }

            const bool fullyContained = !(screenPos.x < minX || screenPos.x > maxX || screenPos.y < minY || screenPos.y > maxY);
            bool intersects = fullyContained;

            if (!fullyContained && !bBoxSelectionRequireFullContainment) {
                const float clampedX = std::clamp(screenPos.x, minX, maxX);
                const float clampedY = std::clamp(screenPos.y, minY, maxY);
                const float dx = screenPos.x - clampedX;
                const float dy = screenPos.y - clampedY;
                intersects = (dx * dx + dy * dy) <= (mBoxSelectionNodePickRadius * mBoxSelectionNodePickRadius);
            }

            if (!intersects) {
                continue;
            }

            const bool wasSelected = IsPointSelected(trackIdx, pointIdx);

            if (toggle) {
                if (wasSelected) {
                    mTrackPoints[trackIdx][pointIdx]->SetSelected(false);
                    mSelectedPoints.erase(
                        std::remove_if(mSelectedPoints.begin(), mSelectedPoints.end(), [trackIdx, pointIdx](const APointSelection& s) {
                            return s.TrackIdx == trackIdx && s.PointIdx == pointIdx;
                        }),
                        mSelectedPoints.end()
                    );
                }
                else {
                    mSelectedPoints.push_back({ trackIdx, pointIdx });
                    mTrackPoints[trackIdx][pointIdx]->SetSelected(true);
                }
                continue;
            }

            if (!wasSelected) {
                mSelectedPoints.push_back({ trackIdx, pointIdx });
                mTrackPoints[trackIdx][pointIdx]->SetSelected(true);
            }
        }
    }
}

void ATrackContext::DrawBoxSelectionOverlay() const {
    if (!bBoxSelecting) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 minCorner(
        std::min(mBoxSelectionStartScreen.x, mBoxSelectionEndScreen.x),
        std::min(mBoxSelectionStartScreen.y, mBoxSelectionEndScreen.y)
    );
    const ImVec2 maxCorner(
        std::max(mBoxSelectionStartScreen.x, mBoxSelectionEndScreen.x),
        std::max(mBoxSelectionStartScreen.y, mBoxSelectionEndScreen.y)
    );

    drawList->AddRectFilled(minCorner, maxCorner, IM_COL32(255, 120, 40, 45));
    drawList->AddRect(minCorner, maxCorner, IM_COL32(255, 120, 40, 220), 0.0f, 0, 1.5f);
}

bool ATrackContext::ProjectPointToViewport(ASceneCamera& camera, const glm::vec3& position, const glm::vec2& viewportPos, const glm::vec2& viewportSize, glm::vec2& outScreenPos) const {
    const glm::vec4 clipPos = camera.GetProjectionMatrix() * camera.GetViewMatrix() * glm::vec4(position, 1.0f);
    if (std::abs(clipPos.w) < 0.0001f) {
        return false;
    }

    const glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
    if (ndc.z < -1.0f || ndc.z > 1.0f) {
        return false;
    }

    outScreenPos.x = viewportPos.x + ((ndc.x * 0.5f) + 0.5f) * viewportSize.x;
    outScreenPos.y = viewportPos.y + (1.0f - ((ndc.y * 0.5f) + 0.5f)) * viewportSize.y;
    return true;
}

bool ATrackContext::IsPointSelected(uint16_t trackIdx, uint16_t pointIdx) const {
    for (const APointSelection& selection : mSelectedPoints) {
        if (selection.TrackIdx == trackIdx && selection.PointIdx == pointIdx) {
            return true;
        }
    }

    return false;
}

void ATrackContext::RecalculateCurveNeighborhoodForSelection() {
    std::unordered_set<uint32_t> recalculationSet;

    for (const APointSelection& selection : mSelectedPoints) {
        if (selection.TrackIdx >= mTrackPoints.size() || selection.PointIdx >= mTrackPoints[selection.TrackIdx].size()) {
            continue;
        }

        const uint16_t trackIdx = selection.TrackIdx;
        const uint16_t pointIdx = selection.PointIdx;
        const size_t pointCount = mTrackPoints[trackIdx].size();
        const bool loops = trackIdx < mTracks.size() && *mTracks[trackIdx]->GetLoopsForEditor();

        auto encode = [](uint16_t t, uint16_t p) {
            return (uint32_t(t) << 16) | uint32_t(p);
        };

        recalculationSet.insert(encode(trackIdx, pointIdx));

        if (pointCount < 2) {
            continue;
        }

        if (loops || pointIdx > 0) {
            const uint16_t prevIdx = loops ? uint16_t((pointIdx + pointCount - 1) % pointCount) : uint16_t(pointIdx - 1);
            recalculationSet.insert(encode(trackIdx, prevIdx));
        }

        if (loops || pointIdx + 1 < pointCount) {
            const uint16_t nextIdx = loops ? uint16_t((pointIdx + 1) % pointCount) : uint16_t(pointIdx + 1);
            recalculationSet.insert(encode(trackIdx, nextIdx));
        }
    }

    for (uint32_t encoded : recalculationSet) {
        const uint16_t trackIdx = uint16_t((encoded >> 16) & 0xFFFF);
        const uint16_t pointIdx = uint16_t(encoded & 0xFFFF);
        if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
            continue;
        }

        if (mTrackPoints[trackIdx][pointIdx]->IsCurve()) {
            RecalculateCurveHandles(trackIdx, pointIdx);
        }
    }
}

void ATrackContext::Render(ASceneCamera& camera) {
    if (mTracks.size() == 0) {
        return;
    }

    HandleUndoRedoShortcuts();

    UCommonUniformBuffer::SetProjAndViewMatrices(camera.GetProjectionMatrix(), camera.GetViewMatrix());
    glBindVertexArray(mPntVAO);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glUseProgram(mSimpleProgram);

    for (uint32_t trackIdx = 0; trackIdx < mTracks.size(); trackIdx++) {
        if (mTracks[trackIdx]->IsHidden()) {
            continue;
        }

        for (const std::shared_ptr<UTracks::UTrackPoint> pnt : mTrackPoints[trackIdx]) {
            if (pnt->IsSelected()) {
                glUniform4fv(mBaseColorUniform, 1, &SELECTED_COLOR.x);
            }
            else if (pnt->IsHighlighted()) {
                glUniform4fv(mBaseColorUniform, 1, &HIGHLIGHT_COLOR.x);
            }
            else if (pnt->GetStationType() != UTracks::ENodeStationType::None) {
                glUniform4fv(mBaseColorUniform, 1, &STATION_COLOR.x);
            }
            else if (pnt->IsJunction()) {
                glUniform4fv(mBaseColorUniform, 1, &JUNCTION_COLOR.x);
            }
            else {
                glUniform4fv(mBaseColorUniform, 1, &NORMAL_COLOR.x);
            }

            UCommonUniformBuffer::SetModelMatrix(glm::translate(glm::identity<glm::mat4>(), pnt->GetPosition()));
            UCommonUniformBuffer::SubmitUBO();

            glDrawElements(GL_TRIANGLES, USphere::IndexCount, GL_UNSIGNED_INT, 0);

            pnt->SetHighlighted(false);

            // Draw handles
            if (pnt->IsCurve() && pnt->IsSelected()) {
                glUniform4fv(mBaseColorUniform, 1, &HANDLE_COLOR.r);
                UCommonUniformBuffer::SetModelMatrix(glm::translate(glm::identity<glm::mat4>(), pnt->GetHandleA()));
                UCommonUniformBuffer::SubmitUBO();

                glDrawElements(GL_TRIANGLES, USphere::IndexCount, GL_UNSIGNED_INT, 0);

                UCommonUniformBuffer::SetModelMatrix(glm::translate(glm::identity<glm::mat4>(), pnt->GetHandleB()));
                UCommonUniformBuffer::SubmitUBO();

                glDrawElements(GL_TRIANGLES, USphere::IndexCount, GL_UNSIGNED_INT, 0);
            }
        }
    }

    glUseProgram(0);
    glBindVertexArray(0);

    for (uint32_t trackIdx = 0; trackIdx < mTracks.size(); trackIdx++) {
        if (mTracks[trackIdx]->IsHidden()) {
            continue;
        }

        mPathRenderers[trackIdx]->Draw(camera, glm::identity<glm::mat4>());
    }
}

void ATrackContext::RenderPickingBuffer(ASceneCamera& camera) {
    UViewportPicker::BindBuffer();

    UCommonUniformBuffer::SetProjAndViewMatrices(camera.GetProjectionMatrix(), camera.GetViewMatrix());
    glBindVertexArray(mPntVAO);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (uint32_t trackIdx = 0; trackIdx < mTracks.size(); trackIdx++) {
        if (mTracks[trackIdx]->IsHidden()) {
            continue;
        }

        for (uint32_t pointIdx = 0; pointIdx < mTrackPoints[trackIdx].size(); pointIdx++) {
            UCommonUniformBuffer::SetModelMatrix(glm::translate(glm::identity<glm::mat4>(), mTrackPoints[trackIdx][pointIdx]->GetPosition()));
            UCommonUniformBuffer::SubmitUBO();

            uint32_t trackId = ((trackIdx + 1) << 16) & 0x3FFF0000;
            uint32_t pointId = pointIdx + 1;
            UViewportPicker::SetIdUniform(trackId | pointId);

            glDrawElements(GL_TRIANGLES, USphere::IndexCount, GL_UNSIGNED_INT, 0);

            if (mTrackPoints[trackIdx][pointIdx]->IsSelected() && mTrackPoints[trackIdx][pointIdx]->IsCurve()) {
                UCommonUniformBuffer::SetModelMatrix(glm::translate(glm::identity<glm::mat4>(), mTrackPoints[trackIdx][pointIdx]->GetHandleA()));
                UCommonUniformBuffer::SubmitUBO();

                UViewportPicker::SetIdUniform(HANDLE_A_MASK | trackId | pointId);
                glDrawElements(GL_TRIANGLES, USphere::IndexCount, GL_UNSIGNED_INT, 0);

                UCommonUniformBuffer::SetModelMatrix(glm::translate(glm::identity<glm::mat4>(), mTrackPoints[trackIdx][pointIdx]->GetHandleB()));
                UCommonUniformBuffer::SubmitUBO();

                UViewportPicker::SetIdUniform(HANDLE_B_MASK | trackId | pointId);
                glDrawElements(GL_TRIANGLES, USphere::IndexCount, GL_UNSIGNED_INT, 0);
            }
        }
    }

    UViewportPicker::UnbindBuffer();
}

void ATrackContext::OnMouseHover(ASceneCamera& camera, int32_t pX, int32_t pY) {
    if (mTracks.size() == 0 || ImGuizmo::IsUsing()) {
        return;
    }

    RenderPickingBuffer(camera);

    uint32_t result = UViewportPicker::Query(pX, pY);
    if (result == 0) {
        return;
    }

    //mSelectedPickType = ETrackNodePickType((result & 0xC0000000) >> 30);
    uint16_t trackIdx = ((result & 0x3FFF0000) >> 16) - 1;
    uint16_t pointIdx = (result & 0xFFFF) - 1;

    if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
        return;
    }

    mTrackPoints[trackIdx][pointIdx]->SetHighlighted(true);
}

void ATrackContext::OnMouseClick(ASceneCamera& camera, int32_t pX, int32_t pY) {
    if (mTracks.size() == 0 || ImGuizmo::IsUsing()) {
        return;
    }

    RenderPickingBuffer(camera);
    uint32_t result = UViewportPicker::Query(pX, pY);

    ETrackNodePickType pickType = ETrackNodePickType((result & 0xC0000000) >> 30);
    uint16_t trackIdx = ((result & 0x3FFF0000) >> 16) - 1;
    uint16_t pointIdx = (result & 0xFFFF) - 1;

    const bool pickInRange = (trackIdx < mTrackPoints.size() && pointIdx < mTrackPoints[trackIdx].size());

    if (bSelectingJunctionPartner) {
        if (pickType != ETrackNodePickType::Position || trackIdx == UINT16_MAX || !pickInRange) {
            bSelectingJunctionPartner = false;
            return;
        }

        PushUndoSnapshot();

        std::shared_ptr<UTracks::UTrackPoint> junctionPartner = mTrackPoints[trackIdx][pointIdx];
        if (junctionPartner->HasJunctionPartner()) {
            junctionPartner->SetJunctionPartner(nullptr);
        }

        uint16_t trackIdx, pointIdx;
        mSelectedPoints[0].Get(trackIdx, pointIdx);

        if (trackIdx >= mTrackPoints.size() || pointIdx >= mTrackPoints[trackIdx].size()) {
            bSelectingJunctionPartner = false;
            ClearSelectedPoints();
            return;
        }

        std::shared_ptr<UTracks::UTrackPoint> selPoint = mTrackPoints[trackIdx][pointIdx];
        selPoint->SetJunctionPartner(junctionPartner);
        junctionPartner->SetJunctionPartner(selPoint);
        
        glm::vec3 middlePos = (junctionPartner->GetPosition() + selPoint->GetPosition()) / 2.0f;
        selPoint->GetPositionForEditor() = middlePos;
        junctionPartner->GetPositionForEditor() = middlePos;

        glm::vec3 middleHandleA = (junctionPartner->GetHandleA() + selPoint->GetHandleA()) / 2.0f;
        selPoint->GetHandleAForEditor() = middleHandleA;
        junctionPartner->GetHandleAForEditor() = middleHandleA;

        glm::vec3 middleHandleB = (junctionPartner->GetHandleB() + selPoint->GetHandleB()) / 2.0f;
        selPoint->GetHandleBForEditor() = middleHandleB;
        junctionPartner->GetHandleBForEditor() = middleHandleB;

        bSelectingJunctionPartner = false;
        SyncPathRenderersFromTrackPoints();
    }
    else {
        const ImGuiIO& io = ImGui::GetIO();
        const bool additive = io.KeyShift;
        const bool toggle = io.KeyCtrl;

        if ((result == 0 || !pickInRange) && !additive && !toggle) {
            ClearSelectedPoints();
        }

        if (result == 0 || !pickInRange) {
            mSelectedPickType = ETrackNodePickType::Position;
            return;
        }

        if (!additive && !toggle) {
            ClearSelectedPoints();
        }

        if (toggle) {
            if (IsPointSelected(trackIdx, pointIdx)) {
                mTrackPoints[trackIdx][pointIdx]->SetSelected(false);
                mSelectedPoints.erase(
                    std::remove_if(mSelectedPoints.begin(), mSelectedPoints.end(), [trackIdx, pointIdx](const APointSelection& s) {
                        return s.TrackIdx == trackIdx && s.PointIdx == pointIdx;
                    }),
                    mSelectedPoints.end()
                );

                if (mSelectedPoints.empty()) {
                    mSelectedPickType = ETrackNodePickType::Position;
                }
                return;
            }
        }

        mSelectedPickType = pickType;

        if (!IsPointSelected(trackIdx, pointIdx)) {
            mSelectedPoints.push_back({ trackIdx, pointIdx });
            mTrackPoints[trackIdx][pointIdx]->SetSelected(true);
        }
    }
}

void ATrackContext::ClearSelectedPoints() {
    for (APointSelection pnt : mSelectedPoints) {
        uint16_t trackIdx, pointIdx;
        pnt.Get(trackIdx, pointIdx);

        if (trackIdx < mTrackPoints.size() && pointIdx < mTrackPoints[trackIdx].size()) {
            mTrackPoints[trackIdx][pointIdx]->SetSelected(false);
        }
    }

    mSelectedPoints.clear();
}
