#include "application/AGatorApplication.hpp"
#include "application/AInput.hpp"
#include "application/AGatorContext.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>

#include "util/ImGuizmo.hpp"

#include <algorithm>
#include <string>
#include <filesystem>
#include <iostream>
#include <iostream>
#include <vector>


AGatorContext* GatorContext = nullptr;

namespace {
	int ClampInitialWindowSize(int targetSize, int preferredMinSize, int maxSize) {
		const int minSize = std::min(preferredMinSize, maxSize);
		return std::clamp(targetSize, minSize, maxSize);
	}

	float GetWindowUiScale(GLFWwindow* window) {
		float xScale = 1.0f;
		float yScale = 1.0f;
		glfwGetWindowContentScale(window, &xScale, &yScale);
		return std::max(1.0f, std::min(xScale, yScale));
	}

	void GLFWErrorCallback(int errorCode, const char* description) {
		std::cerr << "GLFW error [" << errorCode << "]: " << description << std::endl;
	}
}

void DealWithGLErrors(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, const void* userParam) {
	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
		return;
	}

	std::cout << "GL CALLBACK: " << message << std::endl;
}

AGatorApplication::AGatorApplication() {
	mWindow = nullptr;
	mContext = nullptr;
}

bool AGatorApplication::Setup() {
	// Initialize GLFW
	glfwSetErrorCallback(GLFWErrorCallback);
	if (!glfwInit())
		return false;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
	glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

	GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
	int monitorX = 0;
	int monitorY = 0;
	int workWidth = 1280;
	int workHeight = 800;
	if (primaryMonitor != nullptr) {
		glfwGetMonitorWorkarea(primaryMonitor, &monitorX, &monitorY, &workWidth, &workHeight);
	}

	const int maxWindowWidth = std::max(640, workWidth - 80);
	const int maxWindowHeight = std::max(480, workHeight - 80);
	const int windowWidth = ClampInitialWindowSize(int(workWidth * 0.80f), 960, maxWindowWidth);
	const int windowHeight = ClampInitialWindowSize(int(workHeight * 0.80f), 640, maxWindowHeight);

	mWindow = glfwCreateWindow(windowWidth, windowHeight, "NaviGator", nullptr, nullptr);
	if (mWindow == nullptr) {
		glfwTerminate();
		return false;
	}

	glfwSetWindowPos(mWindow, monitorX + (workWidth - windowWidth) / 2, monitorY + (workHeight - windowHeight) / 2);
	glfwSetWindowSizeLimits(mWindow, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

	glfwSetKeyCallback(mWindow, AInput::GLFWKeyCallback);
	glfwSetCursorPosCallback(mWindow, AInput::GLFWMousePositionCallback);
	glfwSetMouseButtonCallback(mWindow, AInput::GLFWMouseButtonCallback);
	glfwSetScrollCallback(mWindow, AInput::GLFWMouseScrollCallback);
	glfwSetDropCallback(mWindow, GLFWDropCallback);

	glfwMakeContextCurrent(mWindow);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cerr << "Failed to initialize GLAD" << std::endl;
		glfwDestroyWindow(mWindow);
		glfwTerminate();
		mWindow = nullptr;
		return false;
	}
	glClearColor(0.5f, 1.0f, 0.5f, 1.0f);
	glfwSwapInterval(1);

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(DealWithGLErrors, nullptr);

	// Initialize imgui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
	io.IniFilename = "imgui.ini";

	const float uiScale = GetWindowUiScale(mWindow);
	io.FontGlobalScale = uiScale;

	ImGui::StyleColorsDark();
	ImGui::GetStyle().ScaleAllSizes(uiScale);

	io.Fonts->AddFontDefault();

	ImFontConfig config;
	config.MergeMode = true;
	config.GlyphMinAdvanceX = 13.0f * uiScale;

	// Try to load font from multiple possible locations
	static const ImWchar icon_ranges[] = { 0xE000, 0xF8FF, 0 };
	ImFont* iconFont = nullptr;
	
	std::vector<std::filesystem::path> fontSearchPaths = {
		"asset/font/MaterialSymbolsRounded.ttf",  // From project root
		"../../asset/font/MaterialSymbolsRounded.ttf",  // From build/Debug
		std::filesystem::current_path() / "asset/font/MaterialSymbolsRounded.ttf",  // From cwd
	};
	
	for (const auto& fontPath : fontSearchPaths) {
		if (std::filesystem::exists(fontPath)) {
			iconFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 13.0f * uiScale, &config, icon_ranges);
			if (iconFont) {
				std::cout << "Loaded font from: " << fontPath << std::endl;
				break;
			}
		}
	}
	
	// If still not found, just skip icon font
	if (!iconFont) {
		std::string searchPaths;
		for (const auto& p : fontSearchPaths) {
			searchPaths += p.string() + "\n  ";
		}
		std::cerr << "Warning: Could not load font. Searched:\n  " << searchPaths << std::endl;
	}

	ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
	ImGui_ImplOpenGL3_Init("#version 150");

	// Create viewer context
	try {
		mContext = new AGatorContext();
		std::cout << "AGatorContext created successfully" << std::endl;
		
		mContext->OnGLInitialized();
		std::cout << "AGatorContext OnGLInitialized() completed successfully" << std::endl;
		
		// Auto-load test drawable directory if it exists (for development testing)
		std::filesystem::path testDrawableDir = "G:\\dist-debug\\ydr";
		if (std::filesystem::exists(testDrawableDir) && std::filesystem::is_directory(testDrawableDir)) {
			std::cout << "\n=== AUTO-LOADING TEST DRAWABLE DIRECTORY ===" << std::endl;
			mContext->LoadWorldDir(testDrawableDir);
			std::cout << "=== TEST DRAWABLE LOADING COMPLETE ===" << std::endl;
		}
	}
	catch (const std::exception& ex) {
		std::cerr << "Exception during context initialization: " << ex.what() << std::endl;
		return false;
	}
	catch (...) {
		std::cerr << "Unknown exception during context initialization" << std::endl;
		return false;
	}

	GatorContext = mContext;
	return true;
}

bool AGatorApplication::Teardown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	
	glfwDestroyWindow(mWindow);
	glfwTerminate();

	delete mContext;

	return true;
}

bool AGatorApplication::Execute(float deltaTime) {
	// Try to make sure we return an error if anything's fucky
	if (mContext == nullptr || mWindow == nullptr || glfwWindowShouldClose(mWindow))
		return false;

	// Update viewer context
	mContext->Update(deltaTime);

	// Begin actual rendering
	glfwMakeContextCurrent(mWindow);
	glfwPollEvents();

	AInput::UpdateInputState();

	int xPos, yPos;
	glfwGetWindowPos(mWindow, &xPos, &yPos);
	mContext->SetAppPosition(xPos, yPos);

	const float dynamicUiScale = GetWindowUiScale(mWindow);
	ImGui::GetIO().FontGlobalScale = dynamicUiScale;

	// The context renders both the ImGui elements and the background elements.
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	// Update buffer size
	int width, height;
	glfwGetFramebufferSize(mWindow, &width, &height);
	glViewport(0, 0, width, height);

	// Clear buffers
	glClearColor(0.353f, 0.294f, 0.647f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Render viewer context
	mContext->Render(deltaTime);

	// Render imgui
	ImGui::Render();

	mContext->PostRender(deltaTime);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}

	// Swap buffers
	glfwSwapBuffers(mWindow);

	return true;
}

void GLFWDropCallback(GLFWwindow* window, int count, const char* paths[]) {
	if (GatorContext == nullptr || count <= 0) {
		return;
	}

	for (int i = 0; i < count; i++) {
		GatorContext->OnFileDropped(paths[i]);
	}
}
