#include "EngineGUI.h"

#include "FileManager.h"
#include "Debug.h"
#include "RenderManager.h"
#include "GameplayManager.h"
#include "FrontEndManager.h"
#include "AquanactBuildSystem.h"
#include "Globals.h"
#include "SceneManager.h"
#include "ProjectManager.h"
#include "Window.h"
#include "Camera.h"
#include "Object3D.h"
#include "Controller.h"
#include "AnimatorComponent.h"
#include "GLHeaders.h"
#include "StbImage.h"
#include "FileSystem.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>

namespace {
	std::filesystem::path SourceRoot()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
		return std::filesystem::current_path();
#endif
	}
}

void EngineGUI::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	m_window = &window;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window.GLFW(), true);
	ImGui_ImplOpenGL3_Init("#version 330");
	m_initialized = true;

	try
	{
		StbImage bootImage;
		const std::filesystem::path bootImageRoot =
#ifdef AQUANACT_GAME
			gFileSystem.ExecutableDirectory() / "assets" / "bootImage";
#else
			SourceRoot() / "assets" / "bootImage";
#endif
		const std::filesystem::path bootImagePath = bootImageRoot / "aquanact_transparent.png";
		bootImage.loadFromFile(bootImagePath.string());
		m_bootTextureWidth = bootImage.getWidth();
		m_bootTextureHeight = bootImage.getHeight();

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
		gDebug.LogMessage("Boot image failed to load: " + std::string(ex.what()));
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
	float titleY = center.y + 100.0f;
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
		titleY = imageMax.y + 24.0f;
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
}

void EngineGUI::Draw(const Camera&, FileManager& fileManager, SceneManager& sceneManager, ProjectManager& projectManager)
{
	const auto& objects = sceneManager.Objects();
	if (m_selectedSceneObjectIndex >= static_cast<int>(objects.size()))
	{
		m_selectedSceneObjectIndex = -1;
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Aquanact"))
		{
			if (ImGui::MenuItem("Quit"))
			{
				if (m_window)
				{
					glfwSetWindowShouldClose(m_window->GLFW(), GLFW_TRUE);
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Project"))
			{
				projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", sceneManager);
			}
			if (ImGui::MenuItem("Load Project"))
			{
				projectManager.LoadProject("C:/dev/Aquanact/assets/projects/project.aqua", sceneManager);
			}
			ImGui::Separator();
			const bool canImport = fileManager.CanImportSelection();
			if (ImGui::MenuItem("Import Selected", nullptr, false, canImport))
			{
				fileManager.ImportSelected();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Axis", nullptr, &m_showAxis);
			if (ImGui::BeginMenu("EngineCamera"))
			{
				float moveSpeed = gRenderManager.GetEngineCamera().MoveSpeed();
				ImGui::SetNextItemWidth(140.0f);
				if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f"))
				{
					gRenderManager.GetEngineCamera().SetMoveSpeed(moveSpeed);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Grid"))
			{
				ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
				float gridSize = gDebug.GridSize();
				ImGui::SetNextItemWidth(140.0f);
				bool sizeChanged = ImGui::DragFloat("Size", &gridSize, 1.0f, 1.0f, 10000.0f, "%.1f");
				gridSize = std::max(gridSize, 1.0f);
				if (sizeChanged)
				{
					gDebug.SetGridSettings(gridSize);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Lighting"))
		{
			const bool canAddPointLight = gRenderManager.Lights().PointLights().size() < LightingManager::MaxPointLights;
			if (ImGui::MenuItem("Add Point Light", nullptr, false, canAddPointLight))
			{
				gRenderManager.Lights().AddPointLight();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Game"))
		{
			if (ImGui::MenuItem("Play"))
			{
				gEngineState.SetMode(EngineMode::Game);
				gRenderManager.SetActiveCamera(gRenderManager.GetGameCamera());
			}
			if (ImGui::MenuItem("Set Game Camera"))
			{
				gRenderManager.GetGameCamera().CopyFrom(gRenderManager.GetEngineCamera());
			}
			if (ImGui::MenuItem("Build Game"))
			{
				m_buildGamePopupRequested = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::MenuItem("GameGUI Creator"))
			{
				gFrontEndManager.OpenGameGUICreator();
				gRenderManager.SetActiveCamera(gRenderManager.GetGameCamera());
				gDebug.LogMessage("GameGUI Creator opened.");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawBuildGamePopup();

	ImGui::Begin("File Explorer");
	if (ImGui::Button("Models"))
	{
		fileManager.SetRootDirectory("C:/dev/Aquanact/assets/models");
	}
	ImGui::SameLine();
	if (ImGui::Button("Textures"))
	{
		fileManager.SetRootDirectory("C:/dev/Aquanact/assets/textures");
	}
	ImGui::SameLine();
	if (ImGui::Button("Projects"))
	{
		fileManager.SetRootDirectory("C:/dev/Aquanact/assets/projects");
	}

	if (fileManager.CanImportSelection())
	{
		if (ImGui::Button("Import Selected"))
		{
			fileManager.ImportSelected();
		}
	}

	ImGui::Separator();
	for (const auto& entry : fileManager.Entries())
	{
		if (entry.is_directory())
		{
			continue;
		}

		const std::filesystem::path entryPath = entry.path();
		const std::string label = entryPath.filename().string();
		const bool selected = fileManager.HasSelection() && fileManager.SelectedPath() == entryPath;

		if (ImGui::Selectable(label.c_str(), selected))
		{
			fileManager.SelectPath(entryPath);
		}
	}
	ImGui::End();

	ImGui::Begin("Scene");
	for (size_t i = 0; i < objects.size(); ++i)
	{
		const auto& object = objects[i];
		const std::string label = object ? object->Name() : std::string("<null>");
		const std::string visibleLabel = label.empty() ? "<unnamed>" : label;
		const std::string selectableId = visibleLabel + "##SceneObject" + std::to_string(i);
		const bool selected = m_selectedSceneObjectIndex == static_cast<int>(i);
		if (ImGui::Selectable(selectableId.c_str(), selected))
		{
			m_selectedSceneObjectIndex = static_cast<int>(i);
		}
	}
	ImGui::End();

	ImGui::Begin("Object");
	if (m_selectedSceneObjectIndex < 0 || m_selectedSceneObjectIndex >= static_cast<int>(objects.size()))
	{
		ImGui::TextUnformatted("No scene object selected.");
	}
	else
	{
		auto& object = objects[static_cast<std::size_t>(m_selectedSceneObjectIndex)];
		if (!object)
		{
			ImGui::TextUnformatted("Selected object is null.");
		}
		else
		{
			const glm::vec3 worldCenterPosition = object->WorldCenterPosition();
			const glm::vec3 defaultWorldCenterPosition = object->InitialWorldCenterPosition();
			ImGui::Text("Name: %s", object->Name().empty() ? "<unnamed>" : object->Name().c_str());
			AnimatorComponent* animatorComponent = object->GetAnimatorComponent();
			Controller* controller = object->GetController();

			ImGui::Separator();
			ImGui::TextUnformatted("Gameplay");
			if (controller)
			{
				float moveSpeed = controller->MoveSpeed();
				ImGui::SetNextItemWidth(140.0f);
				if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f"))
				{
					controller->SetMoveSpeed(moveSpeed);
				}
			}
			else if (ImGui::Button("Add Controller"))
			{
				controller = object->AddComponent<Controller>();
				controller->SetOwner(object.get());
				gGameplayManager.RegisterController(controller);
			}

			ImGui::Separator();
			ImGui::TextUnformatted("Position");
			float editedX = worldCenterPosition.x;
			float editedY = worldCenterPosition.y;
			float editedZ = worldCenterPosition.z;
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat("X##WorldCenter", &editedX, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				object->Translate(glm::vec3(editedX - worldCenterPosition.x, 0.0f, 0.0f));
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset X"))
			{
				object->Translate(glm::vec3(defaultWorldCenterPosition.x - worldCenterPosition.x, 0.0f, 0.0f));
				editedX = defaultWorldCenterPosition.x;
			}

			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat("Y##WorldCenter", &editedY, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				object->Translate(glm::vec3(0.0f, editedY - worldCenterPosition.y, 0.0f));
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Y"))
			{
				object->Translate(glm::vec3(0.0f, defaultWorldCenterPosition.y - worldCenterPosition.y, 0.0f));
				editedY = defaultWorldCenterPosition.y;
			}

			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat("Z##WorldCenter", &editedZ, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
			{
				object->Translate(glm::vec3(0.0f, 0.0f, editedZ - worldCenterPosition.z));
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Z"))
			{
				object->Translate(glm::vec3(0.0f, 0.0f, defaultWorldCenterPosition.z - worldCenterPosition.z));
				editedZ = defaultWorldCenterPosition.z;
			}

			if (animatorComponent)
			{
				static char selectedStateName[64] = "";
				static bool selectedStateInitialized = false;
				if (!selectedStateInitialized || animatorComponent->States().empty())
				{
					if (!animatorComponent->States().empty())
					{
						std::strncpy(selectedStateName, animatorComponent->States().front().name.c_str(), sizeof(selectedStateName) - 1);
						selectedStateName[sizeof(selectedStateName) - 1] = '\0';
					}
					selectedStateInitialized = true;
				}

				ImGui::Separator();
				ImGui::TextUnformatted("Initial Animation");
				if (ImGui::BeginCombo("##InitialAnimation", selectedStateName[0] != '\0' ? selectedStateName : "<select animation>"))
				{
					for (const auto& state : animatorComponent->States())
					{
						const bool selected = std::strcmp(selectedStateName, state.name.c_str()) == 0;
						if (ImGui::Selectable(state.name.c_str(), selected))
						{
							std::strncpy(selectedStateName, state.name.c_str(), sizeof(selectedStateName) - 1);
							selectedStateName[sizeof(selectedStateName) - 1] = '\0';
							animatorComponent->SetDesiredState(selectedStateName);
						}
						if (selected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
		}
	}
	ImGui::End();

	ImGui::Begin("Lighting");
	DirectionalLight& sunLight = gRenderManager.Lights().SunLight();
	if (ImGui::CollapsingHeader("Sun Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Direction", &sunLight.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
		ImGui::ColorEdit3("Color", &sunLight.color.x);
		ImGui::DragFloat("Intensity", &sunLight.intensity, 0.001f, 0.0f, 10.0f, "%.3f");
		ImGui::DragFloat("Ambient", &sunLight.ambient, 0.001f, 0.00f, 1.00f, "%.3f");
		if (ImGui::Button("Reset Sun"))
		{
			sunLight.direction = glm::vec3(-0.3f, -1.0f, 0.2f);
			sunLight.color = glm::vec3(1.0f);
			sunLight.intensity = 1.0f;
			sunLight.ambient = 0.5f;
		}
	}
	ImGui::SeparatorText("Point Lights");
	std::vector<PointLight>& pointLights = gRenderManager.Lights().PointLights();
	for (int i = 0; i < static_cast<int>(pointLights.size()); ++i)
	{
		PointLight& pointLight = pointLights[i];
		ImGui::PushID(i);
		const std::string header = "Point Light " + std::to_string(i + 1);
		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Position", &pointLight.position.x, 0.05f, -1000.0f, 1000.0f, "%.2f");
			ImGui::ColorEdit3("Color", &pointLight.color.x);
			ImGui::DragFloat("Intensity", &pointLight.intensity, 0.01f, 0.0f, 50.0f, "%.2f");
			ImGui::DragFloat("Ambient", &pointLight.ambient, 0.001f, 0.0f, 1.0f, "%.3f");
			float radius = pointLight.radius;
			if (ImGui::DragFloat("Radius", &radius, 5.0f, 0.001f, 5000.0f, "%.2f"))
			{
				pointLight.SetRadius(radius);
			}
			ImGui::DragFloat("Radius Fade", &pointLight.radiusFade, 0.01f, 0.0f, 1.0f, "%.2f");

		}
		ImGui::PopID();
	}
	ImGui::End();

	gFrontEndManager.RuntimeGUI().DrawEditorWindow();
}

void EngineGUI::DrawBuildGamePopup()
{
	static char buildPath[512] = "C:\\dev\\Aquanact\\out\\package";
	static std::string statusMessage;
	static bool requestedBuild = false;

	if (m_buildGamePopupRequested)
	{
		ImGui::OpenPopup("Build Game##AquanactBuildGame");
		m_buildGamePopupRequested = false;
	}

	if (ImGui::BeginPopupModal("Build Game##AquanactBuildGame", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Build the packaged game to this folder:");
		ImGui::InputText("Output", buildPath, sizeof(buildPath));

		if (ImGui::Button("Build"))
		{
			requestedBuild = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}

		if (requestedBuild)
		{
			requestedBuild = false;
			AquanactBuildSystem buildSystem;
			const std::filesystem::path sourceRoot = SourceRoot();
			const std::filesystem::path outputRoot = std::filesystem::path(buildPath);
			const std::filesystem::path projectFile = sourceRoot / "assets" / "projects" / "project.aqua";
			const std::filesystem::path executablePath = std::filesystem::current_path() / "AquanactGame.exe";
			const bool ok = buildSystem.Build(sourceRoot, outputRoot, projectFile, executablePath);
			statusMessage = ok ? "Build succeeded." : "Build failed.";
		}

		if (!statusMessage.empty())
		{
			ImGui::Separator();
			ImGui::TextUnformatted(statusMessage.c_str());
		}

		ImGui::EndPopup();
	}
}

void EngineGUI::EndFrame()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool EngineGUI::ShowAxis() const
{
	return m_showAxis;
}

bool EngineGUI::ShowGrid() const
{
	return m_showGrid;
}

void EngineGUI::SetShowAxis(bool showAxis)
{
	m_showAxis = showAxis;
}

void EngineGUI::SetShowGrid(bool showGrid)
{
	m_showGrid = showGrid;
}
