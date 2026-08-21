#include "Engine/UI/EngineGUI.h"

#include "Engine/Core/Debug.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/Scene.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/StbImage.h"
#include "Engine/Core/Window.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#ifdef AQUANACT_EDITOR
#include "ImGuizmo.h"
#endif

#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>

namespace
{
	std::filesystem::path SourceRoot()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
		return std::filesystem::current_path();
#endif
	}
}

// Lifecycle
void EngineGUI::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	// Set up ImGui once and keep a pointer to the host window for later menu actions.
	m_window = &window;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Input owns the native GLFW cursor visibility. Prevent the ImGui backend
	// from restoring or reshaping the Windows cursor during NewFrame().
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window.GLFW(), true);
	ImGui_ImplOpenGL3_Init("#version 330");
	m_initialized = true;

	try
	{
		// Load the boot image used during the first frame so the editor does not
		// appear empty while the rest of the frontend is initializing.
		StbImage bootImage;
		const std::filesystem::path bootImageRoot =
#ifdef AQUANACT_GAME
			Root::Current().FileSystemRef().ExecutableDirectory() / "assets" / "bootImage";
#else
			SourceRoot() / "assets" / "bootImage";
#endif
		const std::filesystem::path bootImagePath = bootImageRoot / "aquanact_transparent.png";
		bootImage.loadFromFile(bootImagePath.string());
		m_bootTextureWidth = bootImage.getWidth();
		m_bootTextureHeight = bootImage.getHeight();

		// Upload the image into an OpenGL texture for the splash frame.
		glGenTextures(1, &m_bootTexture);
		glBindTexture(GL_TEXTURE_2D, m_bootTexture);
		GLint previousUnpackAlignment = 4;
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			bootImage.getWidth(),
			bootImage.getHeight(),
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			bootImage.getData());
		glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	catch (const std::exception& ex)
	{
		// A missing splash image should not block the editor from starting.
		Root::Current().Debugger().LogMessage("Boot image failed to load: " + std::string(ex.what()));
		if (m_bootTexture != 0)
		{
			glDeleteTextures(1, &m_bootTexture);
			m_bootTexture = 0;
		}
		m_bootTextureWidth = 0;
		m_bootTextureHeight = 0;
	}

	// Present a first ImGui frame immediately so the window has visible content
	// while the remaining frontend systems and project assets finish starting up.
	BeginFrame();
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const ImVec2 center(displaySize.x * 0.5f, displaySize.y * 0.5f);
	if (m_bootTexture != 0)
	{
		const float imageAspect = static_cast<float>(m_bootTextureWidth) / static_cast<float>(m_bootTextureHeight);
		const float maxImageWidth = displaySize.x * 0.195f;
		const float maxImageHeight = displaySize.y * 0.175f;
		float imageWidth = maxImageWidth;
		float imageHeight = imageWidth / imageAspect;
		if (imageHeight > maxImageHeight)
		{
			imageHeight = maxImageHeight;
			imageWidth = imageHeight * imageAspect;
		}

		const ImVec2 imageMin(center.x - imageWidth * 0.5f, center.y - imageHeight * 0.5f - 24.0f);
		const ImVec2 imageMax(imageMin.x + imageWidth, imageMin.y + imageHeight);
			drawList->AddImage(
			reinterpret_cast<ImTextureID>(static_cast<intptr_t>(m_bootTexture)),
			imageMin,
			imageMax,
			ImVec2(0.0f, 0.0f),
			ImVec2(1.0f, 1.0f));
	}

	glClearColor(0.02f, 0.02f, 0.025f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	window.SwapBuffers();
	window.PollEvents();
}

void EngineGUI::shutDown()
{
	if (!m_initialized)
	{
		return;
	}

	// Release the temporary splash texture before shutting down ImGui.
	if (m_bootTexture != 0)
	{
		glDeleteTextures(1, &m_bootTexture);
		m_bootTexture = 0;
	}
	m_bootTextureWidth = 0;
	m_bootTextureHeight = 0;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	m_initialized = false;
	m_window = nullptr;
}

void EngineGUI::BeginFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
#ifdef AQUANACT_EDITOR
	ImGuizmo::BeginFrame();
#endif
}

void EngineGUI::Draw(const Camera& camera, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager)
{
	EngineGuiFrameContext context{
		m_window,
		&camera,
		&fileManager,
		&SceneManager,
		&projectManager,
		&m_selection };

	Scene* activeLevel = SceneManager.ActiveLevel();
	bool selectedEntityStillExists = false;
	if (activeLevel && m_selection.entityId != 0)
	{
		for (const auto& object : activeLevel->Objects())
		{
			if (object && object->Id() == m_selection.entityId)
			{
				selectedEntityStillExists = true;
				break;
			}
		}
	}
	if (!selectedEntityStillExists)
		m_selection.entityId = 0;
	if (m_selection.entityId != 0)
		m_selection.pointLightIndex = -1;
	const std::size_t pointLightCount = Root::Current().Render().Lights().PointLights().size();
	if (m_selection.pointLightIndex < 0 ||
		m_selection.pointLightIndex >= static_cast<int>(pointLightCount))
	{
		m_selection.pointLightIndex = -1;
	}

	const EngineMenuBarResult viewResult =
		m_menuBar.Draw(context, m_showAxis, m_showGrid, m_windowState, m_popupRequests);

	m_buildGameWindow.Draw(m_popupRequests.buildGame);
	m_codeCreationWindow.Draw(m_window, m_popupRequests.addCodeFile, projectManager);
	m_sceneWindow.DrawNewLevelPopup(SceneManager, m_popupRequests.newScene);
	m_inputMapWindow.SetOpen(m_windowState.showInputMapWindow);
	m_inputMapWindow.Draw();
	m_windowState.showInputMapWindow = m_inputMapWindow.IsOpen();
	m_cameraWindow.Draw(m_cameraPathCreator, m_windowState.showCameraWindow, m_showCameraPath);
	m_audioWindow.Draw(SceneManager, projectManager, m_windowState.showAudioWindow);
	m_componentDeletionWindow.Draw(SceneManager, m_popupRequests.deleteComponent);

	m_fileExplorerWindow.Draw(fileManager, m_windowState.showFileExplorer);

	if (m_windowState.showSceneWindow)
	{
		m_sceneWindow.Draw(context, m_windowState.showSceneWindow, m_windowState.showEntityWindow);
	}

	// EntityWindow owns the complete entity inspector.
	if (m_windowState.showEntityWindow)
	{
		const EntityWindowResult result = m_entityWindow.Draw(context, m_windowState.showEntityWindow);
		if (result.stateMachineToDraw)
		{
			if (result.openStateMachine)
				m_stateMachineWindow.Open(*result.stateMachineToDraw);
			if (m_stateMachineWindow.OpenRequested())
				m_stateMachineWindow.Draw(*result.stateMachineToDraw);
		}
	}

	m_lightingWindow.Draw(Root::Current().Render().Lights(), m_windowState.showLightingWindow);

	// Evaluate scene interaction after every editor window has been submitted so
	// real UI ownership can be distinguished from ImGuizmo's next-frame mouse
	// capture request.
	m_sceneInteraction.Draw(context);

	// Apply menu changes after engine windows have been drawn. A window opened
	// from the menu therefore cannot cover or close that menu in this frame.
	if (viewResult.showFileExplorer)
	{
		m_windowState.showFileExplorer = *viewResult.showFileExplorer;
	}
	if (viewResult.showSceneWindow)
	{
		m_windowState.showSceneWindow = *viewResult.showSceneWindow;
	}
	if (viewResult.showEntityWindow)
	{
		m_windowState.showEntityWindow = *viewResult.showEntityWindow;
	}
	if (viewResult.showLightingWindow)
	{
		m_windowState.showLightingWindow = *viewResult.showLightingWindow;
	}

}

void EngineGUI::EndFrame()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Visibility Flags
bool EngineGUI::ShowAxis() const
{
	return m_showAxis;
}

bool EngineGUI::ShowGrid() const
{
	return m_showGrid;
}

bool EngineGUI::ShowLevelWindow() const
{
	return m_windowState.showSceneWindow;
}

bool EngineGUI::ShowEntityWindow() const
{
	return m_windowState.showEntityWindow;
}

bool EngineGUI::ShowLightingWindow() const
{
	return m_windowState.showLightingWindow;
}

bool EngineGUI::ShowFileExplorer() const
{
	return m_windowState.showFileExplorer;
}

bool EngineGUI::ShowInputMapWindow() const
{
	return m_windowState.showInputMapWindow;
}

bool EngineGUI::ShowCameraWindow() const
{
	return m_windowState.showCameraWindow;
}

void EngineGUI::SetShowAxis(bool showAxis)
{
	m_showAxis = showAxis;
}

void EngineGUI::SetShowGrid(bool showGrid)
{
	m_showGrid = showGrid;
}

void EngineGUI::SetShowLevelWindow(bool showLevelWindow)
{
	m_windowState.showSceneWindow = showLevelWindow;
}

void EngineGUI::SetShowEntityWindow(bool showEntityWindow)
{
	m_windowState.showEntityWindow = showEntityWindow;
}

void EngineGUI::SetShowLightingWindow(bool showLightingWindow)
{
	m_windowState.showLightingWindow = showLightingWindow;
}

void EngineGUI::SetShowFileExplorer(bool showFileExplorer)
{
	m_windowState.showFileExplorer = showFileExplorer;
}

void EngineGUI::SetShowInputMapWindow(bool showInputMapWindow)
{
	m_windowState.showInputMapWindow = showInputMapWindow;
}

void EngineGUI::SetShowCameraWindow(bool showCameraWindow)
{
	m_windowState.showCameraWindow = showCameraWindow;
}

bool EngineGUI::ShowCameraPath() const
{
	return m_showCameraPath;
}

void EngineGUI::SetShowCameraPath(bool showCameraPath)
{
	m_showCameraPath = showCameraPath;
}

CameraPathCreator& EngineGUI::CameraPath()
{
	return m_cameraPathCreator;
}

const CameraPathCreator& EngineGUI::CameraPath() const
{
	return m_cameraPathCreator;
}
