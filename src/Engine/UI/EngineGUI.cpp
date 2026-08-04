#include "Engine/UI/EngineGUI.h"

#include "Engine/Core/FileManager.h"
#include "Engine/Core/Debug.h"
#include "Engine/Core/RenderManager.h"
#include "Engine/Core/GameplayManager.h"
#include "Engine/Core/FrontEndManager.h"
#include "Engine/UI/GameGUICreator.h"
#include "Engine/Core/AquanactBuildSystem.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FrameProfiler.h"
#include "Engine/Core/Input.h"
#include "Engine/Core/InputManager.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectManager.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/Camera.h"
#include "Engine/Core/GameCamera.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/PlayerController.h"
#include "Engine/Core/AnimatorComponent.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"
#include "Engine/Core/GLHeaders.h"
#include "Engine/Core/StbImage.h"
#include "Engine/Core/FileSystem.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <string>

namespace {
	constexpr float worldUnitsPerMeter = 100.0f;

	std::string InputBindingLabel(const InputBinding& binding)
	{
		if (binding.type == InputBindingType::Key)
		{
			if (const char* name = glfwGetKeyName(binding.code, 0))
			{
				return std::string("Key: ") + name;
			}

			switch (binding.code)
			{
			case GLFW_KEY_SPACE: return "Key: Space";
			case GLFW_KEY_ENTER: return "Key: Enter";
			case GLFW_KEY_ESCAPE: return "Key: Escape";
			case GLFW_KEY_LEFT: return "Key: Left";
			case GLFW_KEY_RIGHT: return "Key: Right";
			case GLFW_KEY_UP: return "Key: Up";
			case GLFW_KEY_DOWN: return "Key: Down";
			default: return "Key: " + std::to_string(binding.code);
			}
		}

		if (binding.type == InputBindingType::MouseDelta)
		{
			return "Mouse";
		}

		if (binding.type == InputBindingType::ControllerStick)
		{
			return binding.stick == InputStick::Left ? "Left Stick" : "Right Stick";
		}

		if (binding.type == InputBindingType::ControllerDigital)
		{
			switch (binding.code)
			{
			case GLFW_GAMEPAD_BUTTON_A: return "A";
			case GLFW_GAMEPAD_BUTTON_B: return "B";
			case GLFW_GAMEPAD_BUTTON_X: return "X";
			case GLFW_GAMEPAD_BUTTON_Y: return "Y";
			case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return "Left Bumper";
			case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return "Right Bumper";
			case GLFW_GAMEPAD_BUTTON_DPAD_UP: return "D-pad Up";
			case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad Down";
			case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad Left";
			case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad Right";
			default: return "Controller digital: " + std::to_string(binding.code);
			}
		}

		return "Unknown binding";
	}

	std::filesystem::path SourceRoot()
	{
#ifdef AQUANACT_SOURCE_ROOT
		return std::filesystem::path(AQUANACT_SOURCE_ROOT);
#else
		return std::filesystem::current_path();
#endif
	}

	std::filesystem::path GameIncludeRoot()
	{
		return SourceRoot() / "include" / "Game";
	}

	std::filesystem::path GameSourceRoot()
	{
		return SourceRoot() / "src" / "Game";
	}

	struct AnimatorBindingSource
	{
		std::string componentName;
		std::string label;
		std::vector<BindableMember> members;
	};

	bool IsAnimatorConditionMember(const BindableMember& member)
	{
		std::string typeName = member.typeName;
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::tolower(ch));
		});
		return typeName == "bool" || typeName == "int" || typeName == "float" || typeName == "double";
	}

	std::vector<BindableMember> AnimatorConditionMembers(const std::vector<BindableMember>& members)
	{
		std::vector<BindableMember> result;
		for (const BindableMember& member : members)
		{
			if (IsAnimatorConditionMember(member))
			{
				result.push_back(member);
			}
		}
		return result;
	}

	std::vector<AnimatorBindingSource> AnimatorBindingSources(Entity* owner)
	{
		std::vector<AnimatorBindingSource> sources;
		if (!owner)
		{
			return sources;
		}

		std::vector<BindableMember> entityMembers = AnimatorConditionMembers(owner->GetBindableMembers());
		if (!entityMembers.empty())
		{
			sources.push_back({ {}, "Entity", std::move(entityMembers) });
		}

		for (Component* component : owner->Components())
		{
			if (!component)
			{
				continue;
			}

			std::vector<BindableMember> members = AnimatorConditionMembers(component->GetBindableMembers());
			if (!members.empty())
			{
				sources.push_back({ component->Name(), component->Name(), std::move(members) });
			}
		}
		return sources;
	}

	void SetDefaultAnimatorOperand(AnimatorComponent::Operand& operand, const std::vector<AnimatorBindingSource>& sources)
	{
		operand = {};
		for (const AnimatorBindingSource& source : sources)
		{
			for (const BindableMember& member : source.members)
			{
				if ((source.componentName == "PlayerController" || source.componentName == "Controller") && member.name == "IsMoving")
				{
					operand.type = AnimatorComponent::OperandType::Binding;
					operand.componentName = source.componentName;
					operand.memberName = member.name;
					return;
				}
			}
		}

		if (!sources.empty() && !sources.front().members.empty())
		{
			operand.type = AnimatorComponent::OperandType::Binding;
			operand.componentName = sources.front().componentName;
			operand.memberName = sources.front().members.front().name;
		}
	}

	void DrawAnimatorOperandEditor(
		const char* label,
		AnimatorComponent::Operand& operand,
		const std::vector<AnimatorBindingSource>& sources,
		bool useBooleanConstant = false)
	{
		ImGui::PushID(label);
		if (label && label[0] != '\0')
		{
			ImGui::TextUnformatted(label);
		}

		const char* operandTypes[] = { "Constant", "Entity" };
		int operandType = static_cast<int>(operand.type);
		if (ImGui::Combo("Type", &operandType, operandTypes, IM_ARRAYSIZE(operandTypes)))
		{
			operand.type = static_cast<AnimatorComponent::OperandType>(operandType);
			if (operand.type == AnimatorComponent::OperandType::Binding && operand.memberName.empty())
			{
				SetDefaultAnimatorOperand(operand, sources);
				operand.type = AnimatorComponent::OperandType::Binding;
			}
		}

		if (operand.type == AnimatorComponent::OperandType::Constant)
		{
			ImGui::SetNextItemWidth(160.0f);
			if (useBooleanConstant)
			{
				const char* booleanValues[] = { "False", "True" };
				int booleanValue = operand.constantValue != 0.0f ? 1 : 0;
				operand.constantValue = booleanValue == 1 ? 1.0f : 0.0f;
				if (ImGui::Combo("Value", &booleanValue, booleanValues, IM_ARRAYSIZE(booleanValues)))
				{
					operand.constantValue = booleanValue == 1 ? 1.0f : 0.0f;
				}
			}
			else
			{
				ImGui::InputFloat("Value", &operand.constantValue, 0.0f, 0.0f, "%.3f");
			}
			ImGui::PopID();
			return;
		}

		const AnimatorBindingSource* selectedSource = nullptr;
		for (const AnimatorBindingSource& source : sources)
		{
			if (source.componentName == operand.componentName)
			{
				selectedSource = &source;
				break;
			}
		}

		const char* sourceLabel = selectedSource ? selectedSource->label.c_str() : "<select source>";
		if (ImGui::BeginCombo("Component", sourceLabel))
		{
			for (const AnimatorBindingSource& source : sources)
			{
				const bool selected = source.componentName == operand.componentName;
				if (ImGui::Selectable(source.label.c_str(), selected))
				{
					operand.componentName = source.componentName;
					operand.memberName = source.members.empty() ? std::string{} : source.members.front().name;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (!selectedSource)
		{
			ImGui::TextDisabled("No bindable source selected.");
			ImGui::PopID();
			return;
		}

		const BindableMember* selectedMember = nullptr;
		for (const BindableMember& member : selectedSource->members)
		{
			if (member.name == operand.memberName)
			{
				selectedMember = &member;
				break;
			}
		}
		const char* memberLabel = selectedMember
			? (selectedMember->displayName.empty() ? selectedMember->name.c_str() : selectedMember->displayName.c_str())
			: "<select member>";
		if (ImGui::BeginCombo("Value", memberLabel))
		{
			for (const BindableMember& member : selectedSource->members)
			{
				const bool selected = member.name == operand.memberName;
				const char* displayName = member.displayName.empty() ? member.name.c_str() : member.displayName.c_str();
				if (ImGui::Selectable(displayName, selected))
				{
					operand.memberName = member.name;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
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
	// Input owns the native GLFW cursor visibility. Prevent the ImGui backend
	// from restoring or reshaping the Windows cursor during NewFrame().
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window.GLFW(), true);
	ImGui_ImplOpenGL3_Init("#version 330");
	m_initialized = true;

	try
	{
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

void EngineGUI::Draw(const Camera&, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager)
{
	Scene* activeLevel = SceneManager.ActiveLevel();
	static const std::vector<std::unique_ptr<Entity>> emptyObjects;
	const auto& objects = activeLevel ? activeLevel->Objects() : emptyObjects;
	if (m_selectedLevelObjectIndex >= static_cast<int>(objects.size()))
	{
		m_selectedLevelObjectIndex = -1;
	}

	bool pendingFileExplorer = m_showFileExplorer;
	bool pendingLevelWindow = m_showLevelWindow;
	bool pendingEntityWindow = m_showEntityWindow;
	bool pendingLightingWindow = m_showLightingWindow;
	bool fileExplorerToggleChanged = false;
	bool levelWindowToggleChanged = false;
	bool entityWindowToggleChanged = false;
	bool lightingWindowToggleChanged = false;
	if (ImGui::BeginMainMenuBar())
	{
		const auto ToggleMenuItem = [this](const char* label, bool& value)
		{
			const bool clicked = ImGui::Checkbox(label, &value);
			if (clicked)
			{
				return true;
			}
			return false;
		};

		if (ImGui::BeginMenu("Aquanact"))
		{
			if (ImGui::MenuItem("Input Map"))
			{
				m_showInputMapWindow = true;
			}
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
				projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", SceneManager);
			}
			if (ImGui::MenuItem("Load Project"))
			{
				projectManager.LoadProject("C:/dev/Aquanact/assets/projects/project.aqua", SceneManager);
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
			ImGui::TextDisabled("Engine");
			ImGui::Separator();
			ToggleMenuItem("Axis", m_showAxis);
			ToggleMenuItem("Camera Window", m_showCameraWindow);
			if (ImGui::BeginMenu("EngineCamera"))
			{
				float moveSpeed = Root::Current().Render().GetEngineCamera().MoveSpeed();
				ImGui::SetNextItemWidth(140.0f);
				if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f"))
				{
					Root::Current().Render().GetEngineCamera().SetMoveSpeed(moveSpeed);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Grid"))
			{
				if (ToggleMenuItem("Show Grid", m_showGrid))
				{
				}
				float gridSize = Root::Current().Debugger().GridSize();
				ImGui::SetNextItemWidth(140.0f);
				bool sizeChanged = ImGui::DragFloat("Size", &gridSize, 1.0f, 1.0f, 10000.0f, "%.1f");
				gridSize = std::max(gridSize, 1.0f);
				if (sizeChanged)
				{
					Root::Current().Debugger().SetGridSettings(gridSize);
				}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			const bool fileExplorerClicked = ToggleMenuItem("File Explorer", pendingFileExplorer);
			const bool levelWindowClicked = ToggleMenuItem("Scene Window", pendingLevelWindow);
			const bool entityWindowClicked = ToggleMenuItem("Entity Window", pendingEntityWindow);
			const bool lightingWindowClicked = ToggleMenuItem("Lighting Window", pendingLightingWindow);
			fileExplorerToggleChanged |= fileExplorerClicked;
			levelWindowToggleChanged |= levelWindowClicked;
			entityWindowToggleChanged |= entityWindowClicked;
			lightingWindowToggleChanged |= lightingWindowClicked;
			bool showLogWindow = Root::Current().Debugger().ShowLogWindow();
			bool showStatsWindow = Root::Current().Debugger().ShowStatsWindow();
			if (ToggleMenuItem("Log Window", showLogWindow))
			{
				Root::Current().Debugger().SetShowLogWindow(showLogWindow);
			}
			if (ToggleMenuItem("Stats Window", showStatsWindow))
			{
				Root::Current().Debugger().SetShowStatsWindow(showStatsWindow);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Lighting"))
		{
			const bool canAddPointLight = Root::Current().Render().Lights().PointLights().size() < LightingManager::MaxPointLights;
			if (ImGui::MenuItem("Add Point Light", nullptr, false, canAddPointLight))
			{
				Root::Current().Render().Lights().AddPointLight();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Game"))
		{
			if (ImGui::BeginMenu("Camera"))
			{
				const bool thirdPerson = Root::Current().Render().CameraModeValue() == RenderManager::CameraMode::ThirdPerson;
				if (ImGui::MenuItem("Third Person", nullptr, thirdPerson))
				{
					Root::Current().Render().SetCameraMode(RenderManager::CameraMode::ThirdPerson);
				}
				ImGui::EndMenu();
			}
		if (ImGui::MenuItem("Play Game"))
		{
			Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
			Root::Current().FrontEnd().RuntimeGUI().ReloadAssetsFromDisk();
			Root::Current().Render().GetGameCamera().CaptureEditorState();
			if (projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", SceneManager))
				{
					Root::Current().FrontEnd().RestoreRuntimeLayout();
					Root::Current().State().SetMode(EngineMode::Game);
					Root::Current().EditorLaunchedGameSession() = true;
					Root::Current().Render().SetGameMode();
					Root::Current().Gameplay().startUp(
						SceneManager,
						Root::Current().FrontEnd(),
						Root::Current().Debugger(),
						Root::Current().State());
					Root::Current().Gameplay().BootMainMenu(Root::Current().FrontEnd(), Root::Current().Debugger());
				}
				else
				{
					Root::Current().Debugger().LogMessage("Play Game aborted because project autosave failed.");
				}
			}
		if (ImGui::MenuItem("Play Scene")) 
		{
			Root::Current().FrontEnd().Creator().SaveAllRoleGUIs();
			Root::Current().FrontEnd().RuntimeGUI().ReloadAssetsFromDisk();
			Root::Current().Render().GetGameCamera().CaptureEditorState();
			if (projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", SceneManager))
				{
					Root::Current().FrontEnd().RestoreRuntimeLayout();
					Root::Current().State().SetMode(EngineMode::Game);
					Root::Current().EditorLaunchedGameSession() = true;
					Root::Current().Render().SetGameMode();
					Root::Current().Gameplay().startUp(
						SceneManager,
						Root::Current().FrontEnd(),
						Root::Current().Debugger(),
						Root::Current().State());
					Root::Current().Gameplay().StartGameSession(Root::Current().FrontEnd(), Root::Current().Debugger(), Root::Current().State());
				}
				else
				{
					Root::Current().Debugger().LogMessage("Play Scene aborted because project autosave failed.");
				}
			}
			if (ImGui::MenuItem("Set Game Camera"))
			{
				Root::Current().Render().GetGameCamera().SetPose(
					Root::Current().Render().GetEngineCamera().GetPosition(),
					Root::Current().Render().GetEngineCamera().GetFacing());
			}
			if (ImGui::MenuItem("Build Game"))
			{
				m_buildGamePopupRequested = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::BeginMenu("Levels"))
			{
				const auto levelNames = SceneManager.SceneNames(SceneManager::SceneKind::Level);
				for (const auto& levelName : levelNames)
				{
					const Scene* Scene = SceneManager.FindLevel(levelName);
					const bool active = SceneManager.ActiveLevel() == Scene;
					if (ImGui::MenuItem(levelName.c_str(), nullptr, active))
					{
						SceneManager.SetActiveLevel(levelName);
						SceneManager.SetStartupLevelName(levelName);
					}
				}
				if (levelNames.empty())
				{
					ImGui::MenuItem("No gameplay scenes created.", nullptr, false, false);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Cutscenes"))
			{
				const auto cutsceneNames = SceneManager.SceneNames(SceneManager::SceneKind::Cutscene);
				for (const auto& cutsceneName : cutsceneNames)
				{
					const Scene* cutscene = SceneManager.FindLevel(cutsceneName);
					const bool active = SceneManager.ActiveLevel() == cutscene;
					if (ImGui::MenuItem(cutsceneName.c_str(), nullptr, active))
					{
						SceneManager.SetActiveLevel(cutsceneName);
						SceneManager.SetStartupLevelName(cutsceneName);
					}
				}
				if (cutsceneNames.empty())
				{
					ImGui::MenuItem("No cutscenes created.", nullptr, false, false);
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("New Scene"))
			{
				m_newLevelPopupRequested = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Code"))
		{
			if (ImGui::MenuItem("Add Code File"))
			{
				m_addCodeFilePopupRequested = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("UI"))
		{
			if (ImGui::MenuItem("GameGUI Creator"))
			{
				Root::Current().FrontEnd().OpenGameGUICreator();
				Root::Current().Render().SetGameMode();
				Root::Current().Debugger().LogMessage("GameGUI Creator opened.");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawBuildGamePopup();
	DrawAddCodeFilePopup();
	DrawNewLevelPopup();
	DrawInputMapWindow();
	DrawCameraWindow();

	if (m_showFileExplorer)
	{
		bool open = m_showFileExplorer;
		if (ImGui::Begin("File Explorer", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (ImGui::Button("Models"))
			{
				fileManager.SetRootDirectory("C:/dev/Aquanact/assets/models");
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
		}
		ImGui::End();
		m_showFileExplorer = open;
	}

	if (m_showLevelWindow)
	{
		bool open = m_showLevelWindow;
		const Scene* activeLevelForTitle = SceneManager.ActiveLevel();
		const std::string levelWindowTitle = activeLevelForTitle ? activeLevelForTitle->Name() : "Scene";
		// The level list is intentionally the one auto-sizing exception: users
		// need to resize it when a scene contains many entities.
		if (ImGui::Begin(levelWindowTitle.c_str(), &open, ImGuiWindowFlags_NoFocusOnAppearing))
		{
			if (activeLevelForTitle)
			{
				const auto& levelObjects = activeLevelForTitle->Objects();
				for (std::size_t i = 0; i < levelObjects.size(); ++i)
				{
					const auto& object = levelObjects[i];
					const std::string label = object ? object->Name() : std::string("<null>");
					const std::string visibleLabel = label.empty() ? "<unnamed>" : label;
					const std::string selectableId = visibleLabel + "##LevelObject" + std::to_string(i);
					const bool selected = m_selectedLevelObjectIndex == static_cast<int>(i);
					if (ImGui::Selectable(selectableId.c_str(), selected))
					{
						m_selectedLevelObjectIndex = static_cast<int>(i);
						m_showEntityWindow = true;
					}
				}

				if (levelObjects.empty())
				{
					ImGui::TextUnformatted("No entities in this Scene.");
				}
			}
			else
			{
				ImGui::TextUnformatted("No active Scene selected.");
			}
		}
		ImGui::End();
		m_showLevelWindow = open;
	}

	if (m_showEntityWindow)
	{
		bool open = m_showEntityWindow;
		if (ImGui::Begin("Entity", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (m_selectedLevelObjectIndex < 0 || m_selectedLevelObjectIndex >= static_cast<int>(objects.size()))
			{
				ImGui::TextUnformatted("No entity selected.");
			}
			else
			{
				auto& object = objects[static_cast<std::size_t>(m_selectedLevelObjectIndex)];
				if (!object)
				{
					ImGui::TextUnformatted("Selected object is null.");
				}
					else
					{
						const bool activeSceneIsCutscene = SceneManager.SceneKindFor(activeLevel->Name()) == SceneManager::SceneKind::Cutscene;
						const glm::vec3 worldCenterPosition = object->WorldCenterPosition();
						const glm::vec3 defaultWorldCenterPosition = object->InitialWorldCenterPosition();

					bool deleteEntity = false;
					if (ImGui::Button("Delete"))
					{
						ImGui::OpenPopup("Delete Entity##Confirm");
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.0f);
					if (ImGui::BeginCombo("##AddComponent", "Add Component"))
					{
						const bool hasController = object->GetComponent<Controller>() != nullptr;
						const bool hasPlayerHealth = object->GetComponent<PlayerHealth>() != nullptr;
						const bool hasEnemy = object->GetComponent<Enemy>() != nullptr;
						const bool hasAnimator = object->GetComponent<AnimatorComponent>() != nullptr;

						ImGui::BeginDisabled(hasController || activeSceneIsCutscene);
						if (ImGui::Selectable("PlayerController"))
						{
							object->AddComponent<PlayerController>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasController || activeSceneIsCutscene);
						if (ImGui::Selectable("Controller"))
						{
							object->AddComponent<Controller>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasPlayerHealth || activeSceneIsCutscene);
						if (ImGui::Selectable("PlayerHealth"))
						{
							object->AddComponent<PlayerHealth>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasEnemy || activeSceneIsCutscene);
						if (ImGui::Selectable("Enemy"))
						{
							object->AddComponent<Enemy>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasAnimator || object->GetMesh() == nullptr || !object->GetMesh()->Skinned() || activeSceneIsCutscene);
						if (ImGui::Selectable("Animator"))
						{
							object->AddComponent<AnimatorComponent>(object->GetMesh());
						}
						ImGui::EndDisabled();

						if (activeSceneIsCutscene)
						{
							ImGui::Separator();
							ImGui::TextDisabled("Cutscenes cannot receive gameplay components.");
						}
						else if (hasController && hasPlayerHealth && hasEnemy && hasAnimator)
						{
							ImGui::Separator();
							ImGui::TextDisabled("All components are already attached.");
						}
						ImGui::EndCombo();
					}

					if (ImGui::BeginPopupModal("Delete Entity##Confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
					{
						ImGui::Text("Delete %s from the scene?", object->Name().empty() ? "<unnamed>" : object->Name().c_str());
						ImGui::TextDisabled("This removes the entity from the active Scene.");
						if (ImGui::Button("Delete"))
						{
							deleteEntity = true;
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();
						if (ImGui::Button("Cancel"))
						{
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}

					if (deleteEntity && activeLevel && activeLevel->RemoveObject(object.get()))
					{
						m_selectedLevelObjectIndex = -1;
					}

					ImGui::Separator();
					ImGui::TextUnformatted("Position");
					const glm::vec3 position = object->Position();
					const glm::vec3 defaultPosition = object->DefaultPosition();
					ImGui::SameLine();
					if (ImGui::SmallButton("Reset##Position"))
					{
						object->Translate(defaultPosition - position);
					}
					float editedX = position.x;
					float editedY = position.y;
					float editedZ = position.z;
					ImGui::SetNextItemWidth(55.0f);
					if (ImGui::DragFloat("X##Position", &editedX, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
					{
						object->Translate(glm::vec3(editedX - position.x, 0.0f, 0.0f));
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(55.0f);
					if (ImGui::DragFloat("Y##Position", &editedY, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
					{
						object->Translate(glm::vec3(0.0f, editedY - position.y, 0.0f));
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(55.0f);
					if (ImGui::DragFloat("Z##Position", &editedZ, 0.1f, -FLT_MAX, FLT_MAX, "%.2f"))
					{
						object->Translate(glm::vec3(0.0f, 0.0f, editedZ - position.z));
					}

					ImGui::Separator();
					ImGui::TextUnformatted("Rotation");
					const glm::vec3 defaultRotation = object->DefaultRotation();
					ImGui::SameLine();
					if (ImGui::SmallButton("Reset##Rotation"))
					{
						object->SetRotation(defaultRotation);
					}
					const glm::vec3 rotation = object->Rotation();
					float editedRotX = rotation.x;
					float editedRotY = rotation.y;
					float editedRotZ = rotation.z;
					ImGui::SetNextItemWidth(55.0f);
					if (ImGui::DragFloat("X##Rotation", &editedRotX, 0.1f, -360.0f, 360.0f, "%.1f"))
					{
						object->SetRotation(glm::vec3(editedRotX, rotation.y, rotation.z));
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(55.0f);
					if (ImGui::DragFloat("Y##Rotation", &editedRotY, 0.1f, -360.0f, 360.0f, "%.1f"))
					{
						object->SetRotation(glm::vec3(rotation.x, editedRotY, rotation.z));
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(55.0f);
					if (ImGui::DragFloat("Z##Rotation", &editedRotZ, 0.1f, -360.0f, 360.0f, "%.1f"))
					{
						object->SetRotation(glm::vec3(rotation.x, rotation.y, editedRotZ));
					}

					if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
					{
						const char* colliderShapes[] = { "Box", "Capsule", "Convex" };
						int colliderShape = object->GetPhysicsColliderShape() == PhysicsColliderShape::Capsule ? 1
							: object->GetPhysicsColliderShape() == PhysicsColliderShape::Convex ? 2 : 0;
						if (ImGui::Combo("Collider", &colliderShape, colliderShapes, IM_ARRAYSIZE(colliderShapes)))
						{
							object->SetPhysicsColliderShape(colliderShape == 1
								? PhysicsColliderShape::Capsule
								: colliderShape == 2 ? PhysicsColliderShape::Convex : PhysicsColliderShape::Box);
						}
						bool showBoundingBox = object->ShowPhysicsBoundingBox();
						if (ImGui::Checkbox("Draw Bounding Volume", &showBoundingBox))
						{
							object->SetShowPhysicsBoundingBox(showBoundingBox);
						}

					}

					ImGui::Separator();
					ImGui::TextUnformatted("Components");
					ImGui::Separator();
					std::vector<Component*> components = object->Components();
					for (std::size_t componentIndex = 0; componentIndex < components.size(); ++componentIndex)
					{
						Component* component = components[componentIndex];
						if (!component)
						{
							continue;
						}

						if (componentIndex > 0)
						{
							ImGui::Separator();
						}

						ImGui::PushID(component);
						const std::string componentLabel = component->Name();
						const float removeButtonWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;
						bool open = false;
						if (ImGui::BeginTable("ComponentHeader", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
						{
							ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
							ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, removeButtonWidth);
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							open = ImGui::CollapsingHeader(componentLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
							ImGui::TableSetColumnIndex(1);
							if (ImGui::SmallButton("Remove"))
							{
								ImGui::OpenPopup("Remove Component##Confirm");
							}
							ImGui::EndTable();
						}
						if (!open)
						{
							ImGui::PopID();
							if (componentIndex + 1 < components.size())
							{
								ImGui::Separator();
							}
							continue;
						}

						if (AnimatorComponent* animator = dynamic_cast<AnimatorComponent*>(component))
						{
							if (ImGui::Button("Open State Machine"))
							{
								m_animatorStateMachinePopupRequested = true;
								ImGui::OpenPopup("State Machine##AquanactAnimatorStateMachine");
							}
							if (m_animatorStateMachinePopupRequested)
							{
								DrawAnimatorStateMachinePopup(*animator);
							}
						}
						else if (PlayerController* playerController = dynamic_cast<PlayerController*>(component))
						{
							float moveSpeed = playerController->MoveSpeed();
							ImGui::SetNextItemWidth(140.0f);
							if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f"))
							{
								playerController->SetMoveSpeed(moveSpeed);
							}

							float turnSpeed = playerController->TurnSpeed();
							ImGui::SetNextItemWidth(140.0f);
							if (ImGui::InputFloat("Turn Speed", &turnSpeed, 0.0f, 0.0f, "%.2f"))
							{
								playerController->SetTurnSpeed(turnSpeed);
							}
						}
						else if (Controller* controller = dynamic_cast<Controller*>(component))
						{
							float moveSpeed = controller->MoveSpeed();
							ImGui::SetNextItemWidth(140.0f);
							if (ImGui::InputFloat("Move Speed", &moveSpeed, 0.0f, 0.0f, "%.1f"))
							{
								controller->SetMoveSpeed(moveSpeed);
							}
						}
						else if (PlayerHealth* playerHealth = dynamic_cast<PlayerHealth*>(component))
						{
							ImGui::Text("Health: %s", playerHealth->GetHealthText().c_str());
							if (ImGui::Button("Heal to Max"))
							{
								playerHealth->SetHealth(playerHealth->MaxHealth());
							}
						}
						else if (Enemy* enemy = dynamic_cast<Enemy*>(component))
						{
							ImGui::TextUnformatted("Enemy behavior component");
							(void)enemy;
						}
						else
						{
							ImGui::TextUnformatted("No editor controls for this component.");
						}

						bool removeComponent = false;
						if (ImGui::BeginPopupModal("Remove Component##Confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
						{
							ImGui::Text("Remove %s from %s?", componentLabel.c_str(), object->Name().c_str());
							ImGui::TextDisabled("This change is permanent after the project is saved.");
							if (ImGui::Button("Remove"))
							{
								removeComponent = true;
								ImGui::CloseCurrentPopup();
							}
							ImGui::SameLine();
							if (ImGui::Button("Cancel"))
							{
								ImGui::CloseCurrentPopup();
							}
							ImGui::EndPopup();
						}

						if (removeComponent)
						{
							if (AnimatorComponent* animator = dynamic_cast<AnimatorComponent*>(component))
							{
								m_animatorUiState.erase(animator);
								m_animatorStateMachinePopupRequested = false;
							}
							object->RemoveComponent(component);
							ImGui::PopID();
							continue;
						}
						ImGui::PopID();
						if (componentIndex + 1 < components.size())
						{
							ImGui::Separator();
						}
					}
				}
			}
		}
		ImGui::End();
		m_showEntityWindow = open;
	}

	if (m_showLightingWindow)
	{
		bool open = m_showLightingWindow;
		if (ImGui::Begin("Lighting", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize))
		{
			bool shadowsEnabled = Root::Current().Render().Lights().ShadowsEnabled();
			if (ImGui::Checkbox("Enable Shadows", &shadowsEnabled))
			{
				Root::Current().Render().Lights().SetShadowsEnabled(shadowsEnabled);
			}
			ImGui::Separator();
			DirectionalLight& sunLight = Root::Current().Render().Lights().SunLight();
			if (ImGui::CollapsingHeader("Sun Light", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::DragFloat3("Direction", &sunLight.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
				ImGui::ColorEdit3("Color", &sunLight.color.x);
				ImGui::DragFloat("Intensity", &sunLight.intensity, 0.001f, 0.0f, 10.0f, "%.3f");
				ImGui::DragFloat("Ambient", &sunLight.ambient, 0.001f, 0.00f, 1.00f, "%.3f");
				ImGui::Checkbox("Casts Shadow", &sunLight.castsShadows);
				if (ImGui::Button("Reset Sun"))
				{
					sunLight.direction = glm::vec3(-0.3f, -1.0f, 0.2f);
					sunLight.color = glm::vec3(1.0f);
					sunLight.intensity = 1.0f;
					sunLight.ambient = 0.5f;
					sunLight.castsShadows = true;
				}
			}
			ImGui::SeparatorText("Point Lights");
			std::vector<PointLight>& pointLights = Root::Current().Render().Lights().PointLights();
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
					ImGui::Checkbox("Casts Shadow", &pointLight.castsShadows);
					float radius = pointLight.radius;
					if (ImGui::DragFloat("Radius", &radius, 5.0f, 0.001f, 5000.0f, "%.2f"))
					{
						pointLight.SetRadius(radius);
					}
					ImGui::DragFloat("Radius Fade", &pointLight.radiusFade, 0.01f, 0.0f, 1.0f, "%.2f");
				}
				ImGui::PopID();
			}
		}
		ImGui::End();
		m_showLightingWindow = open;
	}

	// Apply menu changes after engine windows have been drawn. A window opened
	// from the menu therefore cannot cover or close that menu in this frame.
	if (fileExplorerToggleChanged)
	{
		m_showFileExplorer = pendingFileExplorer;
	}
	if (levelWindowToggleChanged)
	{
		m_showLevelWindow = pendingLevelWindow;
	}
	if (entityWindowToggleChanged)
	{
		m_showEntityWindow = pendingEntityWindow;
	}
	if (lightingWindowToggleChanged)
	{
		m_showLightingWindow = pendingLightingWindow;
	}

}

void EngineGUI::DrawCameraWindow()
{
	if (!m_showCameraWindow)
	{
		return;
	}

	Scene* activeLevel = Root::Current().Levels().ActiveLevel();
	static const std::vector<std::unique_ptr<Entity>> emptyObjects;
	const auto& objects = activeLevel ? activeLevel->Objects() : emptyObjects;

	bool open = m_showCameraWindow;
	if (ImGui::Begin("Camera", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		GameCamera& camera = Root::Current().Render().GetGameCamera();
		ImGui::TextUnformatted("Target");

		if (ImGui::BeginCombo("##CameraTarget", camera.Target() ? camera.Target()->Name().c_str() : "<select entity>"))
		{
			for (const auto& object : objects)
			{
				if (!object)
				{
					continue;
				}

				const bool selected = camera.Target() == object.get();
				if (ImGui::Selectable(object->Name().c_str(), selected))
				{
					camera.SetTarget(object.get());
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		float distance = camera.Radius();
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::InputFloat("Distance", &distance, 0.0f, 0.0f, "%.2f"))
		{
			camera.SetRadius(distance);
		}
		ImGui::Separator();
		ImGui::TextUnformatted("Collider");
		float colliderRadius = camera.ColliderRadius();
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::DragFloat("Sphere Radius", &colliderRadius, 0.1f, 0.01f, 10000.0f, "%.2f"))
		{
			camera.SetColliderRadius(colliderRadius);
		}
		bool showCameraCollisionDebug = Root::Current().Debugger().ShowCameraCollisionDebug();
		if (ImGui::Checkbox("Camera Collision Debug", &showCameraCollisionDebug))
		{
			Root::Current().Debugger().SetShowCameraCollisionDebug(showCameraCollisionDebug);
		}
	}
	ImGui::End();
	m_showCameraWindow = open;
}

void EngineGUI::DrawAnimatorStateMachinePopup(AnimatorComponent& animator)
{
	AnimatorStateMachineUiState& ui = m_animatorUiState[&animator];
	const std::vector<AnimatorBindingSource> bindingSources = AnimatorBindingSources(animator.Owner());

	if (!ui.initialized || animator.States().empty())
	{
		ui.initialStateName[0] = '\0';
		ui.transitionFromState[0] = '\0';
		ui.transitionToState[0] = '\0';
		ui.transitionFilterFromState[0] = '\0';
		ui.transitionFilterToState[0] = '\0';
		if (!animator.States().empty())
		{
			std::strncpy(ui.initialStateName, animator.States().front().name.c_str(), sizeof(ui.initialStateName) - 1);
			ui.initialStateName[sizeof(ui.initialStateName) - 1] = '\0';
			std::strncpy(ui.transitionFromState, animator.States().front().name.c_str(), sizeof(ui.transitionFromState) - 1);
			ui.transitionFromState[sizeof(ui.transitionFromState) - 1] = '\0';
			if (animator.States().size() > 1)
			{
				std::strncpy(ui.transitionToState, animator.States()[1].name.c_str(), sizeof(ui.transitionToState) - 1);
				ui.transitionToState[sizeof(ui.transitionToState) - 1] = '\0';
			}
		}
		ui.initialized = true;
	}

	if (ImGui::BeginPopupModal("State Machine##AquanactAnimatorStateMachine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Initial animation");
		if (ImGui::BeginCombo("##InitialAnimation", ui.initialStateName[0] != '\0' ? ui.initialStateName : "<select animation>"))
		{
			for (const auto& state : animator.States())
			{
				const bool selected = std::strcmp(ui.initialStateName, state.name.c_str()) == 0;
				if (ImGui::Selectable(state.name.c_str(), selected))
				{
					std::strncpy(ui.initialStateName, state.name.c_str(), sizeof(ui.initialStateName) - 1);
					ui.initialStateName[sizeof(ui.initialStateName) - 1] = '\0';
					animator.SetInitialState(ui.initialStateName);
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Transitions");
		auto drawTransitionFilter = [](const char* id, const char* preview, char* selectedState, const std::vector<AnimatorComponent::State>& states)
		{
			if (!ImGui::BeginCombo(id, selectedState[0] != '\0' ? selectedState : preview))
			{
				return;
			}

			const bool anySelected = selectedState[0] == '\0';
			if (ImGui::Selectable("<any>", anySelected))
			{
				selectedState[0] = '\0';
			}
			if (anySelected)
			{
				ImGui::SetItemDefaultFocus();
			}
			for (const auto& state : states)
			{
				const bool selected = std::strcmp(selectedState, state.name.c_str()) == 0;
				if (ImGui::Selectable(state.name.c_str(), selected))
				{
					std::strncpy(selectedState, state.name.c_str(), 63);
					selectedState[63] = '\0';
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		};

		drawTransitionFilter("From##TransitionFilter", "<select from>", ui.transitionFilterFromState, animator.States());
		ImGui::SameLine();
		drawTransitionFilter("To##TransitionFilter", "<select to>", ui.transitionFilterToState, animator.States());

		const bool hasTransitionFilter = ui.transitionFilterFromState[0] != '\0' || ui.transitionFilterToState[0] != '\0';
		bool displayedTransition = false;
		for (std::size_t transitionIndex = 0; hasTransitionFilter && transitionIndex < animator.Transitions().size(); ++transitionIndex)
		{
			const auto& transition = animator.Transitions()[transitionIndex];
			if ((ui.transitionFilterFromState[0] != '\0' && transition.from != ui.transitionFilterFromState) ||
				(ui.transitionFilterToState[0] != '\0' && transition.to != ui.transitionFilterToState))
			{
				continue;
			}
			displayedTransition = true;
			bool editRequested = false;
			ImGui::PushID(static_cast<int>(transitionIndex));
			ImGui::Text("%s -> %s", transition.from.c_str(), transition.to.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Edit"))
			{
				std::strncpy(ui.transitionFromState, transition.from.c_str(), sizeof(ui.transitionFromState) - 1);
				ui.transitionFromState[sizeof(ui.transitionFromState) - 1] = '\0';
				std::strncpy(ui.transitionToState, transition.to.c_str(), sizeof(ui.transitionToState) - 1);
				ui.transitionToState[sizeof(ui.transitionToState) - 1] = '\0';
				ui.transitionBlendSeconds = transition.blendSeconds;
				ui.conditions = transition.conditions;
				if (ui.conditions.empty())
				{
					ui.conditions.push_back(transition.condition);
				}
				ui.editingTransitionIndex = static_cast<int>(transitionIndex);
				ui.addTransitionPopupInitialized = true;
				editRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				animator.RemoveTransition(transitionIndex);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
			if (editRequested)
			{
				ImGui::OpenPopup("Add Transition##AquanactAnimatorStateMachine");
			}
		}
		if (hasTransitionFilter && !displayedTransition)
		{
			ImGui::TextUnformatted("<no matching transitions>");
		}

		ImGui::Separator();
		if (ImGui::Button("Create Transition"))
		{
			ui.editingTransitionIndex = -1;
			ui.addTransitionPopupInitialized = false;
			ImGui::OpenPopup("Add Transition##AquanactAnimatorStateMachine");
		}

		if (ImGui::BeginPopupModal("Add Transition##AquanactAnimatorStateMachine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (!ui.addTransitionPopupInitialized)
			{
				if (!animator.States().empty())
				{
					std::strncpy(ui.transitionFromState, animator.States().front().name.c_str(), sizeof(ui.transitionFromState) - 1);
					ui.transitionFromState[sizeof(ui.transitionFromState) - 1] = '\0';
					if (animator.States().size() > 1)
					{
						std::strncpy(ui.transitionToState, animator.States()[1].name.c_str(), sizeof(ui.transitionToState) - 1);
						ui.transitionToState[sizeof(ui.transitionToState) - 1] = '\0';
					}
				}
				ui.conditions.clear();
				AnimatorComponent::Condition defaultCondition;
				defaultCondition.right.constantValue = 1.0f;
				ui.conditions.push_back(std::move(defaultCondition));
				ui.transitionBlendSeconds = 0.25f;
				ui.addTransitionPopupInitialized = true;
			}

			ImGui::TextUnformatted("From");
			if (ImGui::BeginCombo("##TransitionFrom", ui.transitionFromState[0] != '\0' ? ui.transitionFromState : "<from>"))
			{
				for (const auto& state : animator.States())
				{
					const bool selected = std::strcmp(ui.transitionFromState, state.name.c_str()) == 0;
					if (ImGui::Selectable(state.name.c_str(), selected))
					{
						std::strncpy(ui.transitionFromState, state.name.c_str(), sizeof(ui.transitionFromState) - 1);
						ui.transitionFromState[sizeof(ui.transitionFromState) - 1] = '\0';
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::TextUnformatted("To");
			if (ImGui::BeginCombo("##TransitionTo", ui.transitionToState[0] != '\0' ? ui.transitionToState : "<to>"))
			{
				for (const auto& state : animator.States())
				{
					const bool selected = std::strcmp(ui.transitionToState, state.name.c_str()) == 0;
					if (ImGui::Selectable(state.name.c_str(), selected))
					{
						std::strncpy(ui.transitionToState, state.name.c_str(), sizeof(ui.transitionToState) - 1);
						ui.transitionToState[sizeof(ui.transitionToState) - 1] = '\0';
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SetNextItemWidth(120.0f);
			ImGui::InputFloat("Blend Seconds", &ui.transitionBlendSeconds, 0.0f, 0.0f, "%.2f");
			ImGui::Separator();

			const auto isBooleanBinding = [&bindingSources](const AnimatorComponent::Operand& operand)
			{
				if (operand.type != AnimatorComponent::OperandType::Binding)
				{
					return false;
				}
				for (const AnimatorBindingSource& source : bindingSources)
				{
					if (source.componentName != operand.componentName)
					{
						continue;
					}
					for (const BindableMember& member : source.members)
					{
						if (member.name == operand.memberName)
						{
							return member.typeName == "bool" || member.typeName == "boolean";
						}
					}
				}
				return false;
			};

			for (std::size_t conditionIndex = 0; conditionIndex < ui.conditions.size(); ++conditionIndex)
			{
				AnimatorComponent::Condition& condition = ui.conditions[conditionIndex];
				if (conditionIndex > 0)
				{
					ImGui::Separator();
				}
				ImGui::PushID(static_cast<int>(conditionIndex));
				ImGui::TextUnformatted("Left Operand");
				ImGui::PushID("Left");
				DrawAnimatorOperandEditor("", condition.left, bindingSources);
				ImGui::PopID();

				const bool booleanCondition = isBooleanBinding(condition.left) || isBooleanBinding(condition.right);
				if (booleanCondition && condition.comparator != AnimatorComponent::Comparator::Equal && condition.comparator != AnimatorComponent::Comparator::NotEqual)
				{
					condition.comparator = AnimatorComponent::Comparator::Equal;
				}
				ImGui::Separator();
				ImGui::TextUnformatted("Comparator");
				const char* comparatorOptions[] = { "Equal", "Not Equal", "Greater", "Less", "Greater Equal", "Less Equal" };
				int comparatorIndex = static_cast<int>(condition.comparator);
				if (booleanCondition)
				{
					const char* booleanComparatorOptions[] = { "Equal", "Not Equal" };
					comparatorIndex = condition.comparator == AnimatorComponent::Comparator::NotEqual ? 1 : 0;
					if (ImGui::Combo("##Comparator", &comparatorIndex, booleanComparatorOptions, IM_ARRAYSIZE(booleanComparatorOptions)))
					{
						condition.comparator = comparatorIndex == 1 ? AnimatorComponent::Comparator::NotEqual : AnimatorComponent::Comparator::Equal;
					}
				}
				else if (ImGui::Combo("##Comparator", &comparatorIndex, comparatorOptions, IM_ARRAYSIZE(comparatorOptions)))
				{
					condition.comparator = static_cast<AnimatorComponent::Comparator>(comparatorIndex);
				}
				ImGui::Separator();

				ImGui::TextUnformatted("Right Operand");
				ImGui::PushID("Right");
				DrawAnimatorOperandEditor("", condition.right, bindingSources, isBooleanBinding(condition.left));
				ImGui::PopID();
				ImGui::Separator();
				const bool booleanConstant = isBooleanBinding(condition.left)
					&& condition.right.type == AnimatorComponent::OperandType::Constant;
				const std::string rightOperandText = booleanConstant
					? (condition.right.constantValue != 0.0f ? "true" : "false")
					: AnimatorComponent::OperandToString(condition.right);
				ImGui::Text("Condition %zu: %s %s %s", conditionIndex + 1,
					AnimatorComponent::OperandToString(condition.left).c_str(),
					AnimatorComponent::ComparatorToString(condition.comparator),
					rightOperandText.c_str());
				ImGui::Separator();
				if (ui.conditions.size() > 1 && ImGui::SmallButton("Remove Condition"))
				{
					ui.conditions.erase(ui.conditions.begin() + static_cast<std::ptrdiff_t>(conditionIndex));
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}

			ImGui::Separator();
			if (ImGui::Button("Add Condition"))
			{
				AnimatorComponent::Condition condition;
				condition.right.constantValue = 1.0f;
				ui.conditions.push_back(std::move(condition));
			}

			if (ImGui::Button("Create"))
			{
				if (ui.editingTransitionIndex >= 0)
				{
					animator.UpdateTransition(static_cast<std::size_t>(ui.editingTransitionIndex), ui.transitionFromState, ui.transitionToState, ui.transitionBlendSeconds, ui.conditions);
				}
				else
				{
					animator.AddTransition(ui.transitionFromState, ui.transitionToState, ui.transitionBlendSeconds, ui.conditions);
				}
				ui.editingTransitionIndex = -1;
				ui.addTransitionPopupInitialized = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				ui.editingTransitionIndex = -1;
				ui.addTransitionPopupInitialized = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();
		if (ImGui::Button("Close"))
		{
			m_animatorStateMachinePopupRequested = false;
			m_animatorUiState.erase(&animator);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	else if (m_animatorStateMachinePopupRequested && !ImGui::IsPopupOpen("State Machine##AquanactAnimatorStateMachine"))
	{
		m_animatorStateMachinePopupRequested = false;
		m_animatorUiState.erase(&animator);
	}
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

std::string EngineGUI::NormalizeGameClassName(const std::string& input)
{
	std::string output;
	output.reserve(input.size());
	bool capitalizeNext = true;
	for (unsigned char ch : input)
	{
		if (std::isalnum(ch))
		{
			output.push_back(capitalizeNext ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch));
			capitalizeNext = false;
		}
		else
		{
			capitalizeNext = true;
		}
	}
	return output;
}

std::string EngineGUI::MakeHeaderTemplate(const std::string& className)
{
	return
		"#pragma once\n\n"
		"#include \"Engine/Entity.h\"\n\n"
		"// Generated gameplay class. Start here if you want to add game behavior.\n"
		"//\n"
		"// This class inherits from Entity, so it must implement:\n"
		"// - TypeName()\n"
		"// - GetBindableMembers()\n"
		"//\n"
		"// TypeName() tells the engine/editor what this gameplay type is called.\n"
		"// GetBindableMembers() tells the engine/editor which variables or\n"
		"// functions are available for UI binding later.\n"
		"class " + className + " final : public Entity\n"
		"{\n"
		"public:\n"
		"\texplicit " + className + "(std::string name = \"" + className + "\");\n\n"
		"\tconst char* TypeName() const override;\n"
		"\tstd::vector<BindableMember> GetBindableMembers() const override;\n"
		"};\n";
}

std::string EngineGUI::MakeSourceTemplate(const std::string& className)
{
	return
		"#include \"Game/" + className + ".h\"\n\n"
		"#include <utility>\n\n"
		"" + className + "::" + className + "(std::string name)\n"
		"\t: Entity(std::move(name))\n"
		"{\n"
		"}\n\n"
		"const char* " + className + "::TypeName() const\n"
		"{\n"
		"\treturn \"" + className + "\";\n"
		"}\n\n"
		"std::vector<BindableMember> " + className + "::GetBindableMembers() const\n"
		"{\n"
		"\treturn {};\n"
		"}\n";
}

void EngineGUI::CreateGameCodeFile(const std::string& className)
{
	const std::filesystem::path headerPath = GameIncludeRoot() / (className + ".h");
	const std::filesystem::path sourcePath = GameSourceRoot() / (className + ".cpp");

	const std::filesystem::path headerDir = headerPath.parent_path();
	const std::filesystem::path sourceDir = sourcePath.parent_path();
	std::error_code ec;
	std::filesystem::create_directories(headerDir, ec);
	std::filesystem::create_directories(sourceDir, ec);

	const bool headerWritten = Root::Current().FileSystemRef().WriteTextFile(headerPath, MakeHeaderTemplate(className));
	const bool sourceWritten = Root::Current().FileSystemRef().WriteTextFile(sourcePath, MakeSourceTemplate(className));

	if (headerWritten && sourceWritten)
	{
		m_addCodeFileStatusMessage = "Created " + headerPath.string() + " and " + sourcePath.string();
	}
	else
	{
		m_addCodeFileStatusMessage = "Failed to create one or more files.";
	}
}

void EngineGUI::DrawAddCodeFilePopup()
{
	if (m_addCodeFilePopupRequested)
	{
		ImGui::OpenPopup("Add Code File##AquanactAddCodeFile");
		m_addCodeFilePopupRequested = false;
		m_addCodeFileCreated = false;
		m_addCodeFileStatusMessage.clear();
	}

	if (ImGui::BeginPopupModal("Add Code File##AquanactAddCodeFile", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Create a new gameplay class:");
		ImGui::InputText("Class Name", m_newCodeFileName, sizeof(m_newCodeFileName));

		if (ImGui::Button("Create"))
		{
			const std::string className = NormalizeGameClassName(m_newCodeFileName);
			if (className.empty())
			{
				m_addCodeFileStatusMessage = "Enter a valid class name.";
			}
			else
			{
				std::strncpy(m_newCodeFileName, className.c_str(), sizeof(m_newCodeFileName) - 1);
				m_newCodeFileName[sizeof(m_newCodeFileName) - 1] = '\0';
				CreateGameCodeFile(className);
				m_addCodeFileCreated = true;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}

		if (!m_addCodeFileStatusMessage.empty())
		{
			ImGui::Separator();
			ImGui::TextUnformatted(m_addCodeFileStatusMessage.c_str());
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

bool EngineGUI::ShowLevelWindow() const
{
	return m_showLevelWindow;
}

bool EngineGUI::ShowEntityWindow() const
{
	return m_showEntityWindow;
}

bool EngineGUI::ShowLightingWindow() const
{
	return m_showLightingWindow;
}

bool EngineGUI::ShowFileExplorer() const
{
	return m_showFileExplorer;
}

bool EngineGUI::ShowInputMapWindow() const
{
	return m_showInputMapWindow;
}

bool EngineGUI::ShowCameraWindow() const
{
	return m_showCameraWindow;
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
	m_showLevelWindow = showLevelWindow;
}

void EngineGUI::SetShowEntityWindow(bool showEntityWindow)
{
	m_showEntityWindow = showEntityWindow;
}

void EngineGUI::SetShowLightingWindow(bool showLightingWindow)
{
	m_showLightingWindow = showLightingWindow;
}

void EngineGUI::SetShowFileExplorer(bool showFileExplorer)
{
	m_showFileExplorer = showFileExplorer;
}

void EngineGUI::SetShowInputMapWindow(bool showInputMapWindow)
{
	m_showInputMapWindow = showInputMapWindow;
}

void EngineGUI::SetShowCameraWindow(bool showCameraWindow)
{
	m_showCameraWindow = showCameraWindow;
}

void EngineGUI::DrawInputMapWindow()
{
	if (!m_showInputMapWindow)
	{
		return;
	}

	InputManager& inputManager = Root::Current().InputActions();
	auto findBinding = [](std::vector<InputBinding>& bindings, InputBindingType type, int code, InputStick stick = InputStick::Left) -> InputBinding*
	{
		for (InputBinding& binding : bindings)
		{
			if (binding.type == type && binding.code == code && binding.stick == stick)
			{
				return &binding;
			}
		}
		return nullptr;
	};

	auto ensureBinding = [](std::vector<InputBinding>& bindings, InputBinding binding)
	{
		for (InputBinding& existing : bindings)
		{
			if (existing.type == binding.type && existing.code == binding.code && existing.stick == binding.stick)
			{
				existing = binding;
				return;
			}
		}
		bindings.push_back(binding);
	};

	bool open = m_showInputMapWindow;
	if (ImGui::Begin("Input Map", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		const auto bindingIt = inputManager.Bindings().find("Move");
		if (bindingIt != inputManager.Bindings().end())
		{
			std::vector<InputBinding> editedBindings = bindingIt->second;
			bool bindingsChanged = false;

			auto drawKeyboardBinding = [&](const char* label, int keyCode, const glm::vec2& vector)
			{
				InputBinding* binding = findBinding(editedBindings, InputBindingType::Key, keyCode);
				if (!binding)
				{
					ensureBinding(editedBindings, { InputBindingType::Key, keyCode, GLFW_JOYSTICK_1, 1.0f, vector });
					binding = findBinding(editedBindings, InputBindingType::Key, keyCode);
				}

				if (!binding)
				{
					return;
				}

				if (ImGui::BeginCombo(label, InputBindingLabel(*binding).c_str()))
				{
					const InputBinding options[] = {
						{ InputBindingType::Key, GLFW_KEY_W, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) },
						{ InputBindingType::Key, GLFW_KEY_A, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_S, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) },
						{ InputBindingType::Key, GLFW_KEY_D, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
					};
					for (const InputBinding& option : options)
					{
						const bool selected = option.code == binding->code;
						if (ImGui::Selectable(InputBindingLabel(option).c_str(), selected))
						{
							*binding = option;
							bindingsChanged = true;
						}
						if (selected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			};

			ImGui::PushID("Move");
			ImGui::TextUnformatted("Move");
			ImGui::TextDisabled("Keyboard");
			ImGui::Separator();
			drawKeyboardBinding("Up##KeyboardUp", GLFW_KEY_W, glm::vec2(0.0f, 1.0f));
			drawKeyboardBinding("Down##KeyboardDown", GLFW_KEY_S, glm::vec2(0.0f, -1.0f));
			drawKeyboardBinding("Left##KeyboardLeft", GLFW_KEY_A, glm::vec2(-1.0f, 0.0f));
			drawKeyboardBinding("Right##KeyboardRight", GLFW_KEY_D, glm::vec2(1.0f, 0.0f));

			ImGui::Spacing();
			ImGui::TextDisabled("Controller");
			ImGui::Separator();
			static const char* controllerModes[] = { "Digital", "Analog" };
			int controllerMode = findBinding(editedBindings, InputBindingType::ControllerStick, 0, InputStick::Left) ? 1 : 0;
			if (ImGui::Combo("##MoveControllerMode", &controllerMode, controllerModes, IM_ARRAYSIZE(controllerModes)))
			{
				editedBindings.erase(std::remove_if(editedBindings.begin(), editedBindings.end(), [](const InputBinding& binding)
				{
					return binding.type == InputBindingType::ControllerDigital || binding.type == InputBindingType::ControllerStick;
				}), editedBindings.end());

				if (controllerMode == 0)
				{
					editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) });
					editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) });
					editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) });
					editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) });
				}
				else
				{
					editedBindings.push_back({ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Left });
				}
				bindingsChanged = true;
			}

			if (controllerMode == 0)
			{
				const char* labels[] = { "Up", "Down", "Left", "Right" };
				const InputBinding options[] = {
					{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) },
					{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) },
					{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) },
					{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
				};

				for (int i = 0; i < 4; ++i)
				{
					InputBinding* binding = findBinding(editedBindings, InputBindingType::ControllerDigital, options[i].code);
					if (!binding)
					{
						ensureBinding(editedBindings, options[i]);
						binding = findBinding(editedBindings, InputBindingType::ControllerDigital, options[i].code);
					}

					if (!binding)
					{
						continue;
					}

					if (ImGui::BeginCombo(labels[i], InputBindingLabel(*binding).c_str()))
					{
						for (const InputBinding& option : options)
						{
							const bool selected = option.code == binding->code;
							if (ImGui::Selectable(InputBindingLabel(option).c_str(), selected))
							{
								*binding = option;
								bindingsChanged = true;
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
			else
			{
				InputBinding* stickBinding = findBinding(editedBindings, InputBindingType::ControllerStick, 0, InputStick::Left);
				if (!stickBinding)
				{
					ensureBinding(editedBindings, { InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Left });
					stickBinding = findBinding(editedBindings, InputBindingType::ControllerStick, 0, InputStick::Left);
				}

				if (stickBinding && ImGui::BeginCombo("##ControllerAnalog", InputBindingLabel(*stickBinding).c_str()))
				{
					const InputBinding options[] = {
						{ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Left },
						{ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Right },
					};
					for (const InputBinding& option : options)
					{
						const bool selected = option.stick == stickBinding->stick;
						if (ImGui::Selectable(InputBindingLabel(option).c_str(), selected))
						{
							*stickBinding = option;
							bindingsChanged = true;
						}
						if (selected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}

			ImGui::PopID();

			if (bindingsChanged)
			{
				inputManager.SetBindings("Move", std::move(editedBindings));
			}
		}
	}
	ImGui::End();
	m_showInputMapWindow = open;
}

void EngineGUI::DrawNewLevelPopup()
{
	if (m_newLevelPopupRequested)
	{
		ImGui::OpenPopup("New Scene##AquanactNewLevel");
		m_newLevelPopupRequested = false;
		m_newLevelStatusMessage.clear();
	}

	if (ImGui::BeginPopupModal("New Scene##AquanactNewLevel", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Create a new scene:");
		ImGui::InputText("Name", m_newLevelName, sizeof(m_newLevelName));

		if (ImGui::Button("Create"))
		{
			const std::string levelName = NormalizeLevelName(m_newLevelName);
			if (levelName.empty())
			{
				m_newLevelStatusMessage = "Enter a valid scene name.";
			}
			else if (Root::Current().Levels().FindLevel(levelName))
			{
				m_newLevelStatusMessage = "Scene already exists.";
			}
			else
			{
				Scene* Scene = Root::Current().Levels().CreateLevel(levelName);
				if (Scene)
				{
					Root::Current().Levels().SetActiveLevel(levelName);
					m_newLevelStatusMessage = "Created scene " + levelName + ".";
				}
				else
				{
					m_newLevelStatusMessage = "Failed to create scene.";
				}
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}

		if (!m_newLevelStatusMessage.empty())
		{
			ImGui::Separator();
			ImGui::TextUnformatted(m_newLevelStatusMessage.c_str());
		}

		ImGui::EndPopup();
	}
}

std::string EngineGUI::NormalizeLevelName(const std::string& input)
{
	std::string output;
	output.reserve(input.size());
	bool capitalizeNext = true;
	for (unsigned char ch : input)
	{
		if (std::isalnum(ch))
		{
			output.push_back(capitalizeNext ? static_cast<char>(std::toupper(ch)) : static_cast<char>(ch));
			capitalizeNext = false;
		}
		else
		{
			capitalizeNext = true;
		}
	}
	return output;
}






