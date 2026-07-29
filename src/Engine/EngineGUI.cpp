#include "Engine/EngineGUI.h"

#include "Engine/FileManager.h"
#include "Engine/Debug.h"
#include "Engine/RenderManager.h"
#include "Engine/GameplayManager.h"
#include "Engine/FrontEndManager.h"
#include "Engine/AquanactBuildSystem.h"
#include "Engine/Globals.h"
#include "Engine/Input.h"
#include "Engine/LevelManager.h"
#include "Engine/ProjectManager.h"
#include "Engine/Window.h"
#include "Engine/Camera.h"
#include "Engine/Entity.h"
#include "Engine/Controller.h"
#include "Engine/PlayerController.h"
#include "Engine/AnimatorComponent.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"
#include "Engine/GLHeaders.h"
#include "Engine/StbImage.h"
#include "Engine/FileSystem.h"

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
		const std::vector<AnimatorBindingSource>& sources)
	{
		ImGui::PushID(label);
		ImGui::TextUnformatted(label);

		const char* operandTypes[] = { "Constant", "Binding" };
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
			ImGui::InputFloat("Value", &operand.constantValue, 0.0f, 0.0f, "%.3f");
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
		if (ImGui::BeginCombo("Source", sourceLabel))
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
		if (ImGui::BeginCombo("Member", memberLabel))
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

void EngineGUI::Draw(const Camera&, FileManager& fileManager, LevelManager& levelManager, ProjectManager& projectManager)
{
	Level* activeLevel = levelManager.ActiveLevel();
	static const std::vector<std::unique_ptr<Entity>> emptyObjects;
	const auto& objects = activeLevel ? activeLevel->Objects() : emptyObjects;
	if (m_selectedLevelObjectIndex >= static_cast<int>(objects.size()))
	{
		m_selectedLevelObjectIndex = -1;
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
				projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", levelManager);
			}
			if (ImGui::MenuItem("Load Project"))
			{
				projectManager.LoadProject("C:/dev/Aquanact/assets/projects/project.aqua", levelManager);
			}
			ImGui::Separator();
			const bool canImport = fileManager.CanImportSelection();
			if (ImGui::MenuItem("Import Selected", nullptr, false, canImport))
			{
				fileManager.ImportSelected();
			}
			ImGui::Separator();
			ImGui::MenuItem("Show File Explorer", nullptr, &m_showFileExplorer);
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
			ImGui::MenuItem("Show Lighting Window", nullptr, &m_showLightingWindow);
			const bool canAddPointLight = gRenderManager.Lights().PointLights().size() < LightingManager::MaxPointLights;
			if (ImGui::MenuItem("Add Point Light", nullptr, false, canAddPointLight))
			{
				gRenderManager.Lights().AddPointLight();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug"))
		{
			bool showLogWindow = gDebug.ShowLogWindow();
			bool showStatsWindow = gDebug.ShowStatsWindow();
			if (ImGui::MenuItem("Show Log Window", nullptr, &showLogWindow))
			{
				gDebug.SetShowLogWindow(showLogWindow);
			}
			if (ImGui::MenuItem("Show Stats Window", nullptr, &showStatsWindow))
			{
				gDebug.SetShowStatsWindow(showStatsWindow);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Game"))
		{
			if (ImGui::MenuItem("Play Game"))
			{
				if (projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", levelManager))
				{
					gEngineState.SetMode(EngineMode::Game);
					gRenderManager.SetGameMode();
					gGameplayManager.BootMainMenu();
				}
				else
				{
					gDebug.LogMessage("Play Game aborted because project autosave failed.");
				}
			}
			if (ImGui::MenuItem("Play Level")) 
			{
				if (projectManager.SaveProject("C:/dev/Aquanact/assets/projects/project.aqua", levelManager))
				{
					gEngineState.SetMode(EngineMode::Game);
					gRenderManager.SetGameMode();
					gGameplayManager.StartGameSession();
				}
				else
				{
					gDebug.LogMessage("Play Level aborted because project autosave failed.");
				}
			}
			if (ImGui::MenuItem("Set Game Camera"))
			{
				gRenderManager.GetGameCamera().SetPose(
					gRenderManager.GetEngineCamera().GetPosition(),
					gRenderManager.GetEngineCamera().GetFacing());
			}
			if (ImGui::MenuItem("Build Game"))
			{
				m_buildGamePopupRequested = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Level"))
		{
			ImGui::MenuItem("Show Level Window", nullptr, &m_showLevelWindow);
			if (ImGui::BeginMenu("Levels"))
			{
				const auto& levels = levelManager.Levels();
				for (const auto& level : levels)
				{
					if (!level)
					{
						continue;
					}

					const bool active = levelManager.ActiveLevel() == level.get();
					if (ImGui::MenuItem(level->Name().c_str(), nullptr, active))
					{
						levelManager.SetActiveLevel(level->Name());
					}
				}
				if (levels.empty())
				{
					ImGui::MenuItem("No levels created.", nullptr, false, false);
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("New Level"))
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
				gFrontEndManager.OpenGameGUICreator();
				gRenderManager.SetGameMode();
				gDebug.LogMessage("GameGUI Creator opened.");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Entity"))
		{
			ImGui::MenuItem("Show Entity Window", nullptr, &m_showEntityWindow);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	DrawBuildGamePopup();
	DrawAddCodeFilePopup();
	DrawNewLevelPopup();

	if (m_showFileExplorer)
	{
		bool open = m_showFileExplorer;
		if (ImGui::Begin("File Explorer", &open))
		{
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
		}
		ImGui::End();
		m_showFileExplorer = open;
	}

	if (m_showLevelWindow)
	{
		bool open = m_showLevelWindow;
		const Level* activeLevelForTitle = levelManager.ActiveLevel();
		const std::string levelWindowTitle = activeLevelForTitle ? activeLevelForTitle->Name() : "Level";
		if (ImGui::Begin(levelWindowTitle.c_str(), &open))
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
					ImGui::TextUnformatted("No entities in this level.");
				}
			}
			else
			{
				ImGui::TextUnformatted("No active level selected.");
			}
		}
		ImGui::End();
		m_showLevelWindow = open;
	}

	if (m_showEntityWindow)
	{
		bool open = m_showEntityWindow;
		if (ImGui::Begin("Entity", &open))
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

						ImGui::BeginDisabled(hasController);
						if (ImGui::Selectable("PlayerController"))
						{
							object->AddComponent<PlayerController>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasController);
						if (ImGui::Selectable("Controller"))
						{
							object->AddComponent<Controller>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasPlayerHealth);
						if (ImGui::Selectable("PlayerHealth"))
						{
							object->AddComponent<PlayerHealth>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasEnemy);
						if (ImGui::Selectable("Enemy"))
						{
							object->AddComponent<Enemy>();
						}
						ImGui::EndDisabled();

						ImGui::BeginDisabled(hasAnimator || object->GetMesh() == nullptr || !object->GetMesh()->Skinned());
						if (ImGui::Selectable("Animator"))
						{
							object->AddComponent<AnimatorComponent>(object->GetMesh());
						}
						ImGui::EndDisabled();

						if (hasController && hasPlayerHealth && hasEnemy && hasAnimator)
						{
							ImGui::Separator();
							ImGui::TextDisabled("All components are already attached.");
						}
						ImGui::EndCombo();
					}

					if (ImGui::BeginPopupModal("Delete Entity##Confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
					{
						ImGui::Text("Delete %s from the scene?", object->Name().empty() ? "<unnamed>" : object->Name().c_str());
						ImGui::TextDisabled("This removes the entity from the active level.");
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
						const bool open = ImGui::CollapsingHeader(componentLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
						if (!open)
						{
							ImGui::PopID();
							if (componentIndex + 1 < components.size())
							{
								ImGui::Separator();
							}
							continue;
						}

						if (ImGui::SmallButton("Remove"))
						{
							ImGui::OpenPopup("Remove Component##Confirm");
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
		if (ImGui::Begin("Lighting", &open))
		{
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
		}
		ImGui::End();
		m_showLightingWindow = open;
	}

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
		ImGui::Text("Animator state machine");
		ImGui::Text("States: %zu", animator.States().size());
		ImGui::Text("Transitions: %zu", animator.Transitions().size());
		ImGui::Separator();

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
		for (const auto& transition : animator.Transitions())
		{
			const std::string leftOperand = AnimatorComponent::OperandToString(transition.condition.left);
			const std::string rightOperand = AnimatorComponent::OperandToString(transition.condition.right);
			ImGui::BulletText("%s -> %s  (%.2f s)  if %s %s %s",
				transition.from.c_str(),
				transition.to.c_str(),
				transition.blendSeconds,
				leftOperand.c_str(),
				AnimatorComponent::ComparatorToString(transition.condition.comparator),
				rightOperand.c_str());
		}
		if (animator.Transitions().empty())
		{
			ImGui::TextUnformatted("<no transitions>");
		}

		ImGui::Separator();
		if (ImGui::Button("Add Transition"))
		{
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
				SetDefaultAnimatorOperand(ui.leftOperand, bindingSources);
				ui.rightOperand = {};
				ui.rightOperand.constantValue = 1.0f;
				ui.comparator = AnimatorComponent::Comparator::Equal;
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

			ImGui::SeparatorText("Condition");
			DrawAnimatorOperandEditor("A", ui.leftOperand, bindingSources);

			const char* comparatorOptions[] = { "Equal", "Not Equal", "Greater", "Less", "Greater Equal", "Less Equal" };
			int comparatorIndex = static_cast<int>(ui.comparator);
			if (ImGui::Combo("Comparator", &comparatorIndex, comparatorOptions, IM_ARRAYSIZE(comparatorOptions)))
			{
				ui.comparator = static_cast<AnimatorComponent::Comparator>(comparatorIndex);
			}

			DrawAnimatorOperandEditor("B", ui.rightOperand, bindingSources);

			if (ImGui::Button("Create"))
			{
				AnimatorComponent::Condition condition;
				condition.left = ui.leftOperand;
				condition.comparator = ui.comparator;
				condition.right = ui.rightOperand;
				animator.AddTransition(ui.transitionFromState, ui.transitionToState, ui.transitionBlendSeconds, condition);
				ui.addTransitionPopupInitialized = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
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

	const bool headerWritten = gFileSystem.WriteTextFile(headerPath, MakeHeaderTemplate(className));
	const bool sourceWritten = gFileSystem.WriteTextFile(sourcePath, MakeSourceTemplate(className));

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

void EngineGUI::DrawNewLevelPopup()
{
	if (m_newLevelPopupRequested)
	{
		ImGui::OpenPopup("New Level##AquanactNewLevel");
		m_newLevelPopupRequested = false;
		m_newLevelStatusMessage.clear();
	}

	if (ImGui::BeginPopupModal("New Level##AquanactNewLevel", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Create a new level:");
		ImGui::InputText("Name", m_newLevelName, sizeof(m_newLevelName));

		if (ImGui::Button("Create"))
		{
			const std::string levelName = NormalizeLevelName(m_newLevelName);
			if (levelName.empty())
			{
				m_newLevelStatusMessage = "Enter a valid level name.";
			}
			else if (gLevelManager.FindLevel(levelName))
			{
				m_newLevelStatusMessage = "Level already exists.";
			}
			else
			{
				Level* level = gLevelManager.CreateLevel(levelName);
				if (level)
				{
					gLevelManager.SetActiveLevel(levelName);
					m_newLevelStatusMessage = "Created level " + levelName + ".";
				}
				else
				{
					m_newLevelStatusMessage = "Failed to create level.";
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


