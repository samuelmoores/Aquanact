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
#include "Engine/Core/PathedCamera.h"
#include "Engine/Core/TriggerSphere.h"
#include "Engine/Core/Entity.h"
#include "Engine/Core/ComponentFactory.h"
#include "Engine/Core/Controller.h"
#include "Game/PlayerController.h"
#include "Engine/Core/EntityStateMachine.h"
#include "Game/Enemy.h"
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
#include <functional>
#include <sstream>
#include <string>

namespace {
	constexpr float worldUnitsPerMeter = 100.0f;

	std::string AnimationFileName(const std::string& animationPath)
	{
		const std::size_t separator = animationPath.find_last_of("/\\");
		return separator == std::string::npos ? animationPath : animationPath.substr(separator + 1);
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

	std::filesystem::path GeneratedRoot()
	{
		return SourceRoot() / "generated";
	}

	std::string MakeComponentRegistryTemplate(const std::vector<std::string>& componentNames)
	{
		// Keep the registry source in sync with the current game component set.
		// When the editor deletes a component type, it must also stop generating
		// registration code for that type or the next rebuild will fail.
		std::string contents =
			"#include \"Engine/Core/ComponentRegistry.h\"\n\n"
			"#include \"Engine/Core/ComponentFactory.h\"\n"
			"#include \"Engine/Core/Controller.h\"\n"
			"#include \"Engine/Core/Entity.h\"\n";

		for (const std::string& componentName : componentNames)
		{
			contents += "#include \"Game/" + componentName + ".h\"\n";
		}

		contents += "\n#include <memory>\n\n";
		contents += "void RegisterGameComponents()\n{\n";
		contents += "\tComponentFactory::Instance().Register(\"Controller\", [](Entity&) -> std::unique_ptr<Component>\n";
		contents += "\t{\n";
		contents += "\t\treturn std::make_unique<Controller>();\n";
		contents += "\t});\n";
		for (const std::string& componentName : componentNames)
		{
			// Each remaining component gets a direct registration entry so the
			// factory can create it by the class name saved in project data.
			contents +=
				"\tComponentFactory::Instance().Register(\"" + componentName + "\", [](Entity&) -> std::unique_ptr<Component>\n"
				"\t{\n"
				"\t\treturn std::make_unique<" + componentName + ">();\n"
				"\t});\n";
		}
		contents += "}\n";
		return contents;
	}

	std::filesystem::path GameRegistryPath()
	{
		return std::filesystem::path(AQUANACT_SOURCE_ROOT) / "src" / "Engine" / "Core" / "ComponentRegistry.cpp";
	}

	std::vector<std::filesystem::path> CollectGameSourceFiles()
	{
		// Treat the game folder as the source of truth for generated maintenance
		// files. If a file disappears from disk, the regenerated lists should stop
		// referencing it automatically.
		std::vector<std::filesystem::path> gameSources;
		const std::filesystem::path gameSourceDir = GameSourceRoot();
		for (const auto& entry : Root::Current().FileSystemRef().ReadDirectory(gameSourceDir))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			const std::filesystem::path filePath = entry.path();
			if (filePath.extension() != ".cpp")
			{
				continue;
			}

			gameSources.push_back(filePath);
		}
		std::sort(gameSources.begin(), gameSources.end());
		gameSources.erase(std::unique(gameSources.begin(), gameSources.end()), gameSources.end());
		return gameSources;
	}

	std::vector<std::string> CollectGameComponentNames()
	{
		// Build the registry from the on-disk source files, not from a hand-edited
		// list. That keeps deleted component types out of the registry even if the
		// project files were rearranged.
		std::vector<std::string> componentNames;
		for (const std::filesystem::path& filePath : CollectGameSourceFiles())
		{
			if (filePath.stem() == "ComponentRegistry")
			{
				continue;
			}

			componentNames.push_back(filePath.stem().string());
		}
		return componentNames;
	}

	std::string MakeGameSourcesList()
	{
		// The generated source list mirrors every game .cpp file so the build
		// system stays in sync with the physical source tree.
		std::string gameSources = "set(GAME_SOURCES\n";
		for (const std::filesystem::path& filePath : CollectGameSourceFiles())
		{
			gameSources += "    \"${CMAKE_SOURCE_DIR}/src/Game/" + filePath.filename().string() + "\"\n";
		}
		gameSources += "    \"${CMAKE_SOURCE_DIR}/src/Engine/Core/ComponentRegistry.cpp\"\n";
		gameSources += ")\n";
		return gameSources;
	}

	bool WriteGameSourcesList()
	{
		return Root::Current().FileSystemRef().WriteTextFile(GeneratedRoot() / "GameSources.cmake", MakeGameSourcesList());
	}

	bool WriteComponentRegistryFile()
	{
		// The registry file is regenerated from the same source list, so the
		// component factory always matches the actual game code that is present.
		return Root::Current().FileSystemRef().WriteTextFile(GameRegistryPath(), MakeComponentRegistryTemplate(CollectGameComponentNames()));
	}

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

		if (binding.type == InputBindingType::MouseButton)
		{
			switch (binding.code)
			{
			case GLFW_MOUSE_BUTTON_LEFT: return "Mouse: Left";
			case GLFW_MOUSE_BUTTON_RIGHT: return "Mouse: Right";
			case GLFW_MOUSE_BUTTON_MIDDLE: return "Mouse: Middle";
			default: return "Mouse: Button " + std::to_string(binding.code + 1);
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

	bool DeleteComponentSourceFiles(const std::string& componentName)
	{
		const std::filesystem::path headerPath = GameIncludeRoot() / (componentName + ".h");
		const std::filesystem::path sourcePath = GameSourceRoot() / (componentName + ".cpp");
		bool deletedAny = false;
		if (std::filesystem::exists(headerPath))
		{
			deletedAny |= std::filesystem::remove(headerPath);
		}
		if (std::filesystem::exists(sourcePath))
		{
			deletedAny |= std::filesystem::remove(sourcePath);
		}
		return deletedAny;
	}

	void RemoveComponentFromBuildLists(const std::string&)
	{
		// Rebuild both generated files from the current source tree instead of
		// editing a stale line in place. That guarantees a deleted component type
		// disappears from the registry and from the generated source list.
		WriteGameSourcesList();
		WriteComponentRegistryFile();
	}

	std::size_t RemoveLiveComponentsFromScene(Scene* scene, const std::string& componentName)
	{
		if (!scene)
		{
			return 0;
		}

		std::size_t removedCount = 0;
		for (const std::unique_ptr<Entity>& entity : scene->Objects())
		{
			if (!entity)
			{
				continue;
			}

			std::vector<Component*> components = entity->Components();
			for (Component* component : components)
			{
				if (component && component->Name() == componentName)
				{
					if (entity->RemoveComponent(component))
					{
						++removedCount;
					}
				}
			}
		}

		return removedCount;
	}

	std::size_t CountLiveComponentsInScene(const Scene* scene, const std::string& componentName)
	{
		if (!scene)
		{
			return 0;
		}

		std::size_t count = 0;
		for (const std::unique_ptr<Entity>& entity : scene->Objects())
		{
			if (!entity)
			{
				continue;
			}

			for (const Component* component : entity->Components())
			{
				if (component && component->Name() == componentName)
				{
					++count;
				}
			}
		}

		return count;
	}

	std::size_t RemoveLiveComponentsFromAllScenes(const SceneManager& sceneManager, const std::string& componentName)
	{
		// The delete button must scrub every loaded scene, not just the active one,
		// so hidden scenes do not keep stale instances alive in memory.
		std::size_t removedCount = 0;
		for (const std::unique_ptr<Scene>& scene : sceneManager.Levels())
		{
			if (!scene)
			{
				continue;
			}

			// Reuse the per-scene cleanup so the deletion flow stays easy to follow.
			removedCount += RemoveLiveComponentsFromScene(scene.get(), componentName);
		}
		return removedCount;
	}

	std::size_t CountLiveComponentsInAllScenes(const SceneManager& sceneManager, const std::string& componentName)
	{
		// This is the preview path used by the confirmation popup. It reports the
		// blast radius without mutating any scene state yet.
		std::size_t count = 0;
		for (const std::unique_ptr<Scene>& scene : sceneManager.Levels())
		{
			if (!scene)
			{
				continue;
			}

			count += CountLiveComponentsInScene(scene.get(), componentName);
		}
		return count;
	}

	void DeleteComponentType(SceneManager& sceneManager, const std::string& componentName)
	{
		// Deletion is a multi-step maintenance action:
		// 1. remove live instances from scenes,
		// 2. unregister the type from the factory,
		// 3. remove the source files,
		// 4. regenerate the build-time lists.
		const std::size_t removedInstances = RemoveLiveComponentsFromAllScenes(sceneManager, componentName);
		const bool removedFromRegistry = ComponentFactory::Instance().Unregister(componentName);
		const bool removedFiles = DeleteComponentSourceFiles(componentName);
		RemoveComponentFromBuildLists(componentName);

		if (removedInstances > 0 || removedFromRegistry || removedFiles)
		{
			Root::Current().Debugger().LogMessage(
				"Component type deleted: " + componentName + " (removed " + std::to_string(removedInstances) + " live instances)");
		}
	}

	struct EntityStateBindingSource
	{
		std::string componentName;
		std::string label;
		std::vector<BindableMember> members;
	};

	std::string NormalizedBindableTypeName(const BindableMember& member)
	{
		std::string typeName = member.typeName;
		std::transform(typeName.begin(), typeName.end(), typeName.begin(), [](unsigned char ch)
		{
			return static_cast<char>(std::tolower(ch));
		});
		return typeName;
	}

	bool IsEntityStateConditionMember(const BindableMember& member)
	{
		const std::string typeName = NormalizedBindableTypeName(member);
		return typeName == "bool" || typeName == "int" || typeName == "float" || typeName == "double";
	}

	const BindableMember* FindEntityStateOperandMember(
		const EntityStateMachine::Operand& operand,
		const std::vector<EntityStateBindingSource>& sources)
	{
		// Constants do not have metadata of their own. Their presentation is inferred
		// from the binding on the other side of the comparison.
		if (operand.type != EntityStateMachine::OperandType::Binding)
		{
			return nullptr;
		}

		// Binding identity consists of both the owning component and member name.
		for (const EntityStateBindingSource& source : sources)
		{
			if (source.componentName != operand.componentName)
			{
				continue;
			}

			for (const BindableMember& member : source.members)
			{
				if (member.name == operand.memberName)
				{
					return &member;
				}
			}
		}

		return nullptr;
	}

	bool IsBooleanEntityStateOperand(
		const EntityStateMachine::Operand& operand,
		const std::vector<EntityStateBindingSource>& sources)
	{
		const BindableMember* member = FindEntityStateOperandMember(operand, sources);
		if (!member)
		{
			return false;
		}

		const std::string typeName = NormalizedBindableTypeName(*member);
		return typeName == "bool" || typeName == "boolean";
	}

	bool IsBooleanEntityStateCondition(
		const EntityStateMachine::Condition& condition,
		const std::vector<EntityStateBindingSource>& sources)
	{
		// A constant is boolean when it is compared with a boolean binding. Checking
		// both sides also supports conditions authored in either operand order.
		return IsBooleanEntityStateOperand(condition.left, sources) ||
			IsBooleanEntityStateOperand(condition.right, sources);
	}

	std::vector<BindableMember> EntityStateConditionMembers(const std::vector<BindableMember>& members)
	{
		std::vector<BindableMember> result;
		for (const BindableMember& member : members)
		{
			if (IsEntityStateConditionMember(member))
			{
				result.push_back(member);
			}
		}
		return result;
	}

	std::vector<EntityStateBindingSource> EntityStateBindingSources(Entity* owner)
	{
		std::vector<EntityStateBindingSource> sources;
		if (!owner)
		{
			return sources;
		}

		std::vector<BindableMember> entityMembers = EntityStateConditionMembers(owner->GetBindableMembers());
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

			std::vector<BindableMember> members = EntityStateConditionMembers(component->GetBindableMembers());
			if (!members.empty())
			{
				sources.push_back({ component->Name(), component->Name(), std::move(members) });
			}
		}
		return sources;
	}

	void SetDefaultEntityStateOperand(EntityStateMachine::Operand& operand, const std::vector<EntityStateBindingSource>& sources)
	{
		operand = {};
		for (const EntityStateBindingSource& source : sources)
		{
			for (const BindableMember& member : source.members)
			{
				if ((source.componentName == "PlayerController" || source.componentName == "Controller") && member.name == "IsMoving")
				{
					operand.type = EntityStateMachine::OperandType::Binding;
					operand.componentName = source.componentName;
					operand.memberName = member.name;
					return;
				}
			}
		}

		if (!sources.empty() && !sources.front().members.empty())
		{
			operand.type = EntityStateMachine::OperandType::Binding;
			operand.componentName = sources.front().componentName;
			operand.memberName = sources.front().members.front().name;
		}
	}

	void DrawEntityStateOperandEditor(
		const char* label,
		EntityStateMachine::Operand& operand,
		const std::vector<EntityStateBindingSource>& sources,
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
			operand.type = static_cast<EntityStateMachine::OperandType>(operandType);
			if (operand.type == EntityStateMachine::OperandType::Binding && operand.memberName.empty())
			{
				SetDefaultEntityStateOperand(operand, sources);
				operand.type = EntityStateMachine::OperandType::Binding;
			}
		}

		if (operand.type == EntityStateMachine::OperandType::Constant)
		{
			ImGui::SetNextItemWidth(160.0f);
			if (useBooleanConstant)
			{
				// Animator operands are stored as floats at runtime. Restrict boolean
				// constants to the equivalent 0/1 values while displaying true/false.
				const char* booleanValues[] = { "false", "true" };
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

		const EntityStateBindingSource* selectedSource = nullptr;
		for (const EntityStateBindingSource& source : sources)
		{
			if (source.componentName == operand.componentName)
			{
				selectedSource = &source;
				break;
			}
		}

		const char* sourceLabel = selectedSource ? selectedSource->label.c_str() : "<select variable>";
		if (ImGui::BeginCombo("Variable", sourceLabel))
		{
			for (const EntityStateBindingSource& source : sources)
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
			: "<select variable>";
		if (ImGui::BeginCombo("Variable Value", memberLabel))
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
}

void EngineGUI::Draw(const Camera&, FileManager& fileManager, SceneManager& SceneManager, ProjectManager& projectManager)
{
	// Keep local copies of the visibility flags so menu interaction and window
	// drawing happen against a stable snapshot during this frame.
	Scene* activeLevel = SceneManager.ActiveLevel();
	static const std::vector<std::unique_ptr<Entity>> emptyObjects;
	const std::vector<std::unique_ptr<Entity>>& objects = activeLevel ? activeLevel->Objects() : emptyObjects;

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

	// ************ Top Menu **********************
	if (ImGui::BeginMainMenuBar())
	{
		// Small helper so menu checkboxes stay consistent with the stored state.
		const std::function<bool(const char*, bool&)> ToggleMenuItem = [](const char* label, bool& value)
		{
			const bool clicked = ImGui::Checkbox(label, &value);
			if (clicked)
			{
				return true;
			}
			return false;
		};

		// ---- Aquanact ------
		if (ImGui::BeginMenu("Aquanact"))
		{
			// Project-level app actions.
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

		// ---- File ------
		if (ImGui::BeginMenu("File"))
		{
			// Project I/O.
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

		// ---- View ------
		if (ImGui::BeginMenu("View"))
		{
			// Editor windows and debug overlays.
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

			// clicked toggles
			const bool fileExplorerClicked = ToggleMenuItem("File Explorer", pendingFileExplorer);
			const bool levelWindowClicked = ToggleMenuItem("Scene Window", pendingLevelWindow);
			const bool entityWindowClicked = ToggleMenuItem("Entity Window", pendingEntityWindow);
			const bool lightingWindowClicked = ToggleMenuItem("Lighting Window", pendingLightingWindow);

			// changed toggles
			fileExplorerToggleChanged |= fileExplorerClicked;
			levelWindowToggleChanged |= levelWindowClicked;
			entityWindowToggleChanged |= entityWindowClicked;
			lightingWindowToggleChanged |= lightingWindowClicked;

			// debug windows
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
			ImGui::EndMenu(); // view
		}

		// ---- Lighting ------
		if (ImGui::BeginMenu("Lighting"))
		{
			// Scene lighting actions.
			const bool canAddPointLight = Root::Current().Render().Lights().PointLights().size() < LightingManager::MaxPointLights;

			if (ImGui::MenuItem("Add Point Light", nullptr, false, canAddPointLight))
			{
				Root::Current().Render().Lights().AddPointLight();
			}
			ImGui::EndMenu();
		}

		// ---- Game ------
		if (ImGui::BeginMenu("Game"))
		{
			// Runtime and packaging actions.
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
				Root::Current().Render().GetPathedCamera().SetPose(
					Root::Current().Render().GetEngineCamera().GetPosition(),
					Root::Current().Render().GetEngineCamera().GetFacing());
			}
			if (ImGui::MenuItem("Build Game"))
			{
				m_buildGamePopupRequested = true;
			}
			ImGui::EndMenu(); //game
		}

		// ---- Scene ------
		if (ImGui::BeginMenu("Scene"))
		{
			// Scene switching and scene creation.
			if (ImGui::BeginMenu("Levels"))
			{
				const std::vector<std::string> levelNames = SceneManager.SceneNames(SceneManager::SceneKind::Level);

				for (const std::string& levelName : levelNames)
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
				const std::vector<std::string> cutsceneNames = SceneManager.SceneNames(SceneManager::SceneKind::Cutscene);
				for (const std::string& cutsceneName : cutsceneNames)
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

			ImGui::EndMenu(); // scene
		}

		// ---- Code ------
		if (ImGui::BeginMenu("Code"))
		{
			// Gameplay class generation.
			if (ImGui::MenuItem("Add Code File"))
			{
				m_addCodeFilePopupRequested = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete Component Type"))
			{
				m_componentDeletePopupRequested = true;
			}
			ImGui::EndMenu();
		}

		// ---- UI ------
		if (ImGui::BeginMenu("UI"))
		{
			// Tooling and editor helpers.
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
	if (m_componentDeletePopupRequested)
	{
		ImGui::OpenPopup("Delete Component Type##AquanactDeleteComponentType");
	}
	if (ImGui::BeginPopupModal("Delete Component Type##AquanactDeleteComponentType", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// The popup is a destructive confirmation, so name the selected type and
		// show the current live-instance count before the user commits.
		const std::vector<std::string> componentNames = ComponentFactory::Instance().Names();
		// The combo is stateful across frames so the user can open the popup,
		// select a type, and confirm without the selection disappearing.
		if (ImGui::BeginCombo("Component Type", m_componentTypePendingDelete.empty() ? "<select component>" : m_componentTypePendingDelete.c_str()))
		{
			for (const std::string& componentName : componentNames)
			{
				const bool selected = m_componentTypePendingDelete == componentName;
				if (ImGui::Selectable(componentName.c_str(), selected))
				{
					m_componentTypePendingDelete = componentName;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		const bool canDelete = !m_componentTypePendingDelete.empty();
		const std::size_t liveInstanceCount = canDelete
			? CountLiveComponentsInAllScenes(SceneManager, m_componentTypePendingDelete)
			: 0;

		// Show the deletion impact before the user confirms. This reflects every
		// loaded scene, not just the active one.
		if (canDelete)
		{
			ImGui::Text("Delete %s?", m_componentTypePendingDelete.c_str());
			if (liveInstanceCount == 1)
			{
				ImGui::Text("1 live instance will be removed from loaded scenes.");
			}
			else
			{
				ImGui::Text("%zu live instances will be removed from loaded scenes.", liveInstanceCount);
			}
		}
		else
		{
			ImGui::TextDisabled("Select a component type to see how many live instances will be removed.");
		}

		ImGui::BeginDisabled(!canDelete);
		if (ImGui::Button("Delete"))
		{
			DeleteComponentType(SceneManager, m_componentTypePendingDelete);
			m_componentTypePendingDelete.clear();
			m_componentDeletePopupRequested = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			m_componentTypePendingDelete.clear();
			m_componentDeletePopupRequested = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Keep the file explorer responsive to the selected directory and import actions.
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
			for (const std::filesystem::directory_entry& entry : fileManager.Entries())
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
		// The scene window focuses on selecting an entity, not resizing the window.
		bool open = m_showLevelWindow;
		const Scene* activeLevelForTitle = SceneManager.ActiveLevel();
		const std::string levelWindowTitle = activeLevelForTitle ? activeLevelForTitle->Name() : "Scene";
		// The level list is intentionally the one auto-sizing exception: users
		// need to resize it when a scene contains many entities.
		if (ImGui::Begin(levelWindowTitle.c_str(), &open, ImGuiWindowFlags_NoFocusOnAppearing))
		{
			if (activeLevelForTitle)
			{
				const std::vector<std::unique_ptr<Entity>>& levelObjects = activeLevelForTitle->Objects();
				for (std::size_t i = 0; i < levelObjects.size(); ++i)
				{
					const std::unique_ptr<Entity>& object = levelObjects[i];
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
		// The entity inspector is intentionally large because it exposes many
		// nested controls for transform, physics, and component editing.
		// The structure below is:
		// 1. Validate selection.
		// 2. Handle entity-level actions like delete and add component.
		// 3. Edit core transform/physics properties.
		// 4. Render each attached component and its per-component controls.
		bool open = m_showEntityWindow;
		if (ImGui::Begin("Entity", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (m_selectedLevelObjectIndex < 0 || m_selectedLevelObjectIndex >= static_cast<int>(objects.size()))
			{
				ImGui::TextUnformatted("No entity selected.");
			}
			else
			{
				const std::unique_ptr<Entity>& object = objects[static_cast<std::size_t>(m_selectedLevelObjectIndex)];
				if (!object)
				{
					ImGui::TextUnformatted("Selected object is null.");
				}
				else
				{
					const bool activeSceneIsCutscene = SceneManager.SceneKindFor(activeLevel->Name()) == SceneManager::SceneKind::Cutscene;

					// Entity-level actions live at the top of the inspector so they are
					// easy to find before the user starts editing individual components.
					bool deleteEntity = false;
					if (ImGui::Button("Delete"))
					{
						ImGui::OpenPopup("Delete Entity##Confirm");
					}
					ImGui::SameLine();
					ImGui::SetNextItemWidth(120.0f);
					// The add-component combo only lists registered component types.
					// Runtime checks prevent duplicates and enforce scene-specific rules.
					if (ImGui::BeginCombo("##AddComponent", "Add Component"))
					{
						const std::vector<std::string> componentNames = ComponentFactory::Instance().Names();
						std::vector<std::string> attachedNames;
						attachedNames.reserve(object->Components().size());
						for (Component* component : object->Components())
						{
							if (component)
							{
								attachedNames.push_back(component->Name());
							}
						}

						for (const std::string& componentName : componentNames)
						{
							const bool alreadyAttached = std::find(attachedNames.begin(), attachedNames.end(), componentName) != attachedNames.end();
							const bool canAttachAnimator = componentName == "EntityStateMachine"
								? object->GetMesh() != nullptr && object->GetMesh()->Skinned()
								: true;
							const bool disabled = activeSceneIsCutscene || alreadyAttached || !canAttachAnimator;

							ImGui::BeginDisabled(disabled);
							if (ImGui::Selectable(componentName.c_str()))
							{
								std::unique_ptr<Component> component = ComponentFactory::Instance().Create(componentName, *object);
								if (component)
								{
									object->AddComponent(std::move(component));
								}
							}
							ImGui::EndDisabled();
						}

						if (activeSceneIsCutscene)
						{
							ImGui::Separator();
							ImGui::TextDisabled("Cutscenes cannot receive gameplay components.");
						}
						else if (componentNames.empty())
						{
							ImGui::Separator();
							ImGui::TextDisabled("No component types are registered.");
						}
						else if (attachedNames.size() >= componentNames.size())
						{
							ImGui::Separator();
							ImGui::TextDisabled("All components are already attached.");
						}
						ImGui::EndCombo();
					}

					// Confirm destructive actions inside a modal popup so the entity
					// cannot be removed by a stray click.
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

					// Apply the entity delete only after the confirmation popup closes.
					if (deleteEntity && activeLevel && activeLevel->RemoveObject(object.get()))
					{
						m_selectedLevelObjectIndex = -1;
						ImGui::End();
						m_showEntityWindow = open;
						return;
					}

					// Transform editing is kept separate from components because it
					// applies to every entity regardless of attached gameplay scripts.
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
					// The state machine is an entity-level gameplay controller, so it is
					// shown here with transform settings instead of buried in components.
					if (EntityStateMachine* entityStateMachine = object->GetEntityState())
					{
						ImGui::Separator();
						if (ImGui::Button("Entity State Machine"))
						{
							m_entityStateMachinePopupRequested = true;
							ImGui::OpenPopup("State Machine##AquanactEntityStateMachine");
						}
						if (m_entityStateMachinePopupRequested)
						{
							DrawEntityStateMachinePopup(*entityStateMachine);
						}
						ImGui::Separator();
					}

					// Physics belongs here because it is another entity-level concern,
					// separate from the component-specific editor controls below.
					if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
					{
						// Physics controls are grouped because they are only relevant when
						// editing collision and debug visualization.
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
						bool ignoreCameraCollision = object->IgnoreCameraCollision();
						if (ImGui::Checkbox("Ignore Camera Collision", &ignoreCameraCollision))
						{
							object->SetIgnoreCameraCollision(ignoreCameraCollision);
						}
						bool blocksCameraView = object->BlocksCameraView();
						if (ImGui::Checkbox("Blocks Camera View", &blocksCameraView))
						{
							object->SetBlocksCameraView(blocksCameraView);
						}

					}

					// Components are rendered last so entity-level settings stay at the
					// top and each attached component can manage its own UI independently.
					ImGui::Separator();
					ImGui::TextUnformatted("Components");
					ImGui::Separator();
					std::vector<Component*> components = object->Components();
					// Each attached component gets its own collapsible block and removal flow.
					for (std::size_t componentIndex = 0; componentIndex < components.size(); ++componentIndex)
					{
						Component* component = components[componentIndex];
						if (!component)
						{
							continue;
						}
						// EntityStateMachine has a dedicated editor directly below Rotation.
						if (dynamic_cast<EntityStateMachine*>(component))
						{
							continue;
						}

						if (componentIndex > 0)
						{
							ImGui::Separator();
						}

						// Keep each component's widgets isolated so popups and headers do
						// not collide when multiple components share the same label.
						ImGui::PushID(component);
						const std::string componentLabel = component->Name();
						const std::string removePopupId = std::string("Remove Component##Confirm_") + std::to_string(reinterpret_cast<std::uintptr_t>(component));
						const float removeButtonWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;
						bool open = false;
						bool openRemovePopup = false;
						
						const std::string componentHeaderId = std::string("ComponentHeader##") + std::to_string(reinterpret_cast<std::uintptr_t>(component));
						
						// The header row gives the user a stable place to collapse the
						// component while keeping the remove button aligned to the right.
						if (ImGui::BeginTable(componentHeaderId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
						{
							ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
							ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, removeButtonWidth);
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							open = ImGui::CollapsingHeader(componentLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
							ImGui::TableSetColumnIndex(1);
							if (ImGui::SmallButton("Remove"))
							{
								openRemovePopup = true;
							}
							ImGui::EndTable();
						}
						if (openRemovePopup)
						{
							ImGui::OpenPopup(removePopupId.c_str());
						}
						// Only draw the component's editor when the header is expanded.
						// The header row still stays visible so the user can reopen it or
						// remove the component even while the details are collapsed.
						if (open)
						{
							// Component-specific controls are selected by type so each component
							// can expose its own editor without the entity window knowing details.
							if (PlayerController* playerController = dynamic_cast<PlayerController*>(component))
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

								float maxSlopeAngle = playerController->MaxSlopeAngle();
								ImGui::SetNextItemWidth(140.0f);
								if (ImGui::DragFloat("Max Slope Angle", &maxSlopeAngle, 0.5f, 0.0f, 89.0f, "%.1f degrees"))
								{
									playerController->SetMaxSlopeAngle(maxSlopeAngle);
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
							else if (Enemy* enemy = dynamic_cast<Enemy*>(component))
							{
								ImGui::TextUnformatted("Enemy behavior component");
								(void)enemy;
							}
							else if (TriggerSphere* trigger = dynamic_cast<TriggerSphere*>(component))
							{
								float radius = trigger->Radius();
								ImGui::SetNextItemWidth(140.0f);
								if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.0f, 10000.0f, "%.2f"))
								{
									trigger->SetRadius(radius);
								}
								bool enabled = trigger->Enabled();
								if (ImGui::Checkbox("Enabled", &enabled))
								{
									trigger->SetEnabled(enabled);
								}
								bool showTriggers = Root::Current().Debugger().ShowTriggerSpheres();
								if (ImGui::Checkbox("Debug Draw", &showTriggers))
								{
									Root::Current().Debugger().SetShowTriggerSpheres(showTriggers);
								}
							}
							else
							{
								ImGui::TextUnformatted("No editor controls for this component.");
							}
						}

						// Component removal is confirmed separately so the header button is
						// never an immediate destructive action.
						bool removeComponent = false;
						if (ImGui::BeginPopupModal(removePopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
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
							if (EntityStateMachine* entityStateMachine = dynamic_cast<EntityStateMachine*>(component))
							{
								m_entityStateUiState.erase(entityStateMachine);
								m_entityStateMachinePopupRequested = false;
							}
							// Remove the component only after any component-specific cleanup.
							object->RemoveComponent(component);
							ImGui::PopID();
							continue;
						}

						// A closed collapsing header still needs its popup and ID cleanup, but
						// it should skip the trailing separator so the next component stays tidy.
						if (!open)
						{
							ImGui::PopID();
							if (componentIndex + 1 < components.size())
							{
								ImGui::Separator();
							}
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
		// Lighting controls are separated so they can be opened without the entity inspector.
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

// Popup and window drawing helpers
void EngineGUI::DrawCameraWindow()
{
	if (!m_showCameraWindow)
	{
		return;
	}

	bool open = m_showCameraWindow;
	if (ImGui::Begin("Camera", &open, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
	{
		CameraPathData& path = m_cameraPathCreator.Data();
		ImGui::TextUnformatted("Camera Path");
		ImGui::Checkbox("Show Camera Path", &m_showCameraPath);
		PathedCamera& pathedCamera = Root::Current().Render().GetPathedCamera();
		float followSharpness = pathedCamera.FollowSharpness();
		if (ImGui::DragFloat("Follow Sharpness", &followSharpness, 0.1f, 0.0f, 50.0f))
		{
			pathedCamera.SetFollowSharpness(followSharpness);
		}
		int curveSamples = pathedCamera.PathSamplesPerSegment();
		if (ImGui::SliderInt("Curve Samples", &curveSamples, 4, 256))
		{
			pathedCamera.SetPathSamplesPerSegment(curveSamples);
		}

		if (ImGui::Button("Add Point"))
		{
			const EngineCamera& editorCamera =
				Root::Current().Render().GetEngineCamera();
			m_cameraPathCreator.AddPoint(
				editorCamera.GetPosition());
		}
		ImGui::SameLine();
		const int selectedPoint = m_cameraPathCreator.SelectedPoint();
		if (ImGui::Button("Remove Point") && selectedPoint >= 0)
		{
			m_cameraPathCreator.RemovePoint(
				static_cast<std::size_t>(selectedPoint));
		}

		for (std::size_t i = 0; i < path.points.size(); ++i)
		{
			const std::string label = "Point " + std::to_string(i + 1);
			if (ImGui::Selectable(
				label.c_str(),
				m_cameraPathCreator.SelectedPoint() == static_cast<int>(i)))
			{
				m_cameraPathCreator.SelectPoint(static_cast<int>(i));
			}
		}

		const int editedPoint = m_cameraPathCreator.SelectedPoint();
		if (editedPoint >= 0 && editedPoint < static_cast<int>(path.points.size()))
		{
			CameraPathPoint& point = path.points[static_cast<std::size_t>(editedPoint)];
			ImGui::Separator();
			ImGui::DragFloat3("Position", &point.position.x, 0.1f);

			if (ImGui::Button("Capture Editor Camera Position"))
			{
				point.position = Root::Current().Render().GetEngineCamera().GetPosition();
			}
		}
	}
	ImGui::End();
	m_showCameraWindow = open;
}

void EngineGUI::DrawEntityStateMachinePopup(EntityStateMachine& entityStateMachine)
{
	EntityStateMachineUiState& ui = m_entityStateUiState[&entityStateMachine];
	const std::vector<EntityStateBindingSource> bindingSources = EntityStateBindingSources(entityStateMachine.Owner());
	const std::vector<EntityStateMachine::State> states = entityStateMachine.States();
	const std::vector<EntityStateMachine::Transition> transitions = entityStateMachine.Transitions();
	const std::vector<std::string> animationNames = [&]()
	{
		std::vector<std::string> names;
		if (Entity* owner = entityStateMachine.Owner())
		{
			if (const Mesh* mesh = owner->GetMesh())
			{
				names.reserve(static_cast<std::size_t>(mesh->NumAnimations()));
				for (int i = 0; i < mesh->NumAnimations(); ++i)
				{
					names.push_back(mesh->GetAnimationSource(i));
				}
			}
		}
		return names;
	}();

	const auto copyStateName = [](char* destination, std::size_t destinationSize, const std::string& value)
	{
		std::strncpy(destination, value.c_str(), destinationSize - 1);
		destination[destinationSize - 1] = '\0';
	};

	const auto operandToConditionText = [&](const EntityStateMachine::Operand& operand, bool booleanContext) -> std::string
	{
		if (!booleanContext || operand.type != EntityStateMachine::OperandType::Constant)
		{
			return EntityStateMachine::OperandToString(operand);
		}
		return operand.constantValue != 0.0f ? "true" : "false";
	};

	const auto conditionToBrowserText = [&](const EntityStateMachine::Condition& condition) -> std::string
	{
		const bool isBooleanCondition = IsBooleanEntityStateCondition(condition, bindingSources);
		return operandToConditionText(condition.left, isBooleanCondition) + " " +
			EntityStateMachine::ComparatorToString(condition.comparator) + " " +
			operandToConditionText(condition.right, isBooleanCondition);
	};

	const auto drawTransitionRow = [&](const EntityStateMachine::Transition& transition, std::size_t transitionIndex)
	{
		const std::string transitionKey = transition.from + "->" + transition.to + "#" + std::to_string(transitionIndex);
		bool& showConditions = ui.expandedTransitionConditions[transitionKey];

		ImGui::PushID(static_cast<int>(transitionIndex));
		ImGui::Text("%s -> %s", transition.from.c_str(), transition.to.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Edit"))
		{
			copyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), transition.from);
			copyStateName(ui.transitionToState, sizeof(ui.transitionToState), transition.to);
			ui.transitionBlendSeconds = transition.blendSeconds;
			ui.transitionWaitForCurrentStateComplete = transition.waitForCurrentStateComplete;
			ui.conditions = transition.conditions.empty() ? std::vector<EntityStateMachine::Condition>{ transition.condition } : transition.conditions;
			ui.editingTransitionIndex = static_cast<int>(transitionIndex);
			ui.addTransitionPopupInitialized = true;
			ui.editTransitionPopupRequested = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete"))
		{
			ui.expandedTransitionConditions.erase(transitionKey);
			entityStateMachine.RemoveTransition(transitionIndex);
			ImGui::PopID();
			return;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Condition"))
		{
			showConditions = !showConditions;
		}
		if (showConditions)
		{
			ImGui::Indent();
			for (const EntityStateMachine::Condition& condition : transition.conditions.empty() ? std::vector<EntityStateMachine::Condition>{ transition.condition } : transition.conditions)
			{
				ImGui::TextUnformatted(conditionToBrowserText(condition).c_str());
			}
			ImGui::Unindent();
		}
		ImGui::PopID();
	};

	if (!ui.initialized)
	{
		ui.stateEditName[0] = '\0';
		ui.transitionFromState[0] = '\0';
		ui.transitionToState[0] = '\0';
		ui.transitionFilterFromState[0] = '\0';
		ui.transitionFilterToState[0] = '\0';
		ui.visibleStateTransitions.clear();
		ui.showIncomingTransitions = true;
		ui.showOutgoingTransitions = true;
		ui.expandedTransitionConditions.clear();
		ui.initialized = true;
	}

	if (ui.stateEditName[0] == '\0' && !states.empty())
	{
		copyStateName(ui.stateEditName, sizeof(ui.stateEditName), states.front().name);
	}
	if (ui.transitionFromState[0] == '\0' && !states.empty())
	{
		copyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), states.front().name);
	}
	if (ui.transitionToState[0] == '\0' && states.size() > 1)
	{
		copyStateName(ui.transitionToState, sizeof(ui.transitionToState), states[1].name);
	}

	ImGui::SetNextWindowSize(ImVec2(900.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopupModal("State Machine##AquanactEntityStateMachine", nullptr))
	{
		auto setComboWidthToText = [](const char* text)
		{
			const ImGuiStyle& style = ImGui::GetStyle();
			const float width = ImGui::CalcTextSize(text).x + style.FramePadding.x * 2.0f + ImGui::GetFrameHeight();
			ImGui::SetNextItemWidth(width);
		};

		if (ImGui::Button("Add State"))
		{
			ui.editingStateIndex = -1;
			ui.addStatePopupInitialized = false;
			ui.editStatePopupRequested = true;
		}

		if (ui.editStatePopupRequested)
		{
			ImGui::OpenPopup("Add State##AquanactEntityStateMachine");
			ui.editStatePopupRequested = false;
		}

		if (ImGui::BeginPopupModal("Add State##AquanactEntityStateMachine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (!ui.addStatePopupInitialized)
			{
				if (ui.editingStateIndex >= 0 && static_cast<std::size_t>(ui.editingStateIndex) < states.size())
				{
					const EntityStateMachine::State& editedState = states[static_cast<std::size_t>(ui.editingStateIndex)];
					copyStateName(ui.stateEditName, sizeof(ui.stateEditName), editedState.name);
					ui.stateEditBlocksMovement = editedState.blocksMovement;
					ui.stateEditBlocksInput = editedState.blocksInput;
				}
				else
				{
					ui.stateEditName[0] = '\0';
					ui.stateEditAnimationName[0] = '\0';
					ui.stateEditBlocksMovement = false;
					ui.stateEditBlocksInput = false;
				}
				ui.addStatePopupInitialized = true;
			}

			ImGui::InputText("State Name", ui.stateEditName, sizeof(ui.stateEditName));
			const std::string currentAnimation = ui.stateEditAnimationName[0] != '\0'
				? AnimationFileName(ui.stateEditAnimationName)
				: "<select animation>";
			if (ImGui::BeginCombo("Animation", currentAnimation.c_str()))
			{
				for (int i = 0; i < static_cast<int>(animationNames.size()); ++i)
				{
					const std::string animationLabel = AnimationFileName(animationNames[static_cast<std::size_t>(i)]);
					const bool selected = animationNames[static_cast<std::size_t>(i)] == ui.stateEditAnimationName;
					if (ImGui::Selectable(animationLabel.c_str(), selected))
					{
						copyStateName(ui.stateEditAnimationName, sizeof(ui.stateEditAnimationName), animationNames[static_cast<std::size_t>(i)]);
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::Checkbox("Block Movement", &ui.stateEditBlocksMovement);
			ImGui::Checkbox("Blocks Input", &ui.stateEditBlocksInput);

			if (ImGui::Button("Create"))
			{
				if (ui.editingStateIndex >= 0)
				{
					const std::size_t editedStateIndex = static_cast<std::size_t>(ui.editingStateIndex);
					const std::string previousStateName = states[editedStateIndex].name;
					const bool transitionsWereVisible = ui.visibleStateTransitions[previousStateName];
					if (entityStateMachine.UpdateState(
						editedStateIndex,
						ui.stateEditName,
						ui.stateEditAnimationName,
						ui.stateEditBlocksMovement,
						ui.stateEditBlocksInput))
					{
						ui.visibleStateTransitions.erase(previousStateName);
						ui.visibleStateTransitions[ui.stateEditName] = transitionsWereVisible;
					}
				}
				else
				{
					entityStateMachine.AddState(
						ui.stateEditName,
						ui.stateEditAnimationName,
						ui.stateEditBlocksMovement,
						ui.stateEditBlocksInput);
				}
				copyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), ui.stateEditName);
				ui.editingStateIndex = -1;
				ui.addStatePopupInitialized = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				ui.editingStateIndex = -1;
				ui.addStatePopupInitialized = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Initial State");
		const char* currentInitialState = entityStateMachine.InitialState().empty() ? "<none>" : entityStateMachine.InitialState().c_str();
		setComboWidthToText(currentInitialState);
		if (ImGui::BeginCombo("##EntityStateInitialState", currentInitialState))
		{
			for (const EntityStateMachine::State& state : states)
			{
				const bool selected = entityStateMachine.InitialState() == state.name;
				if (ImGui::Selectable(state.name.c_str(), selected))
				{
					entityStateMachine.SetInitialState(state.name);
				}
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::TextUnformatted("States");
		for (std::size_t stateIndex = 0; stateIndex < states.size(); ++stateIndex)
		{
			const EntityStateMachine::State& state = states[stateIndex];
			ImGui::PushID(static_cast<int>(stateIndex));
			bool& showTransitions = ui.visibleStateTransitions[state.name];
			ImGui::Checkbox("##ShowTransitions", &showTransitions);
			ImGui::SameLine();
			ImGui::TextUnformatted(state.name.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Edit"))
			{
				copyStateName(ui.stateEditName, sizeof(ui.stateEditName), state.name);
				copyStateName(ui.stateEditAnimationName, sizeof(ui.stateEditAnimationName), state.animationName);
				ui.stateEditBlocksMovement = state.blocksMovement;
				ui.stateEditBlocksInput = state.blocksInput;
				ui.editingStateIndex = static_cast<int>(stateIndex);
				ui.addStatePopupInitialized = false;
				ui.editStatePopupRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				ui.visibleStateTransitions.erase(state.name);
				entityStateMachine.RemoveState(stateIndex);
				ImGui::PopID();
				break;
			}
			ImGui::PopID();
		}

		auto stateTransitionsAreVisible = [](const std::map<std::string, bool>& visibility, const std::string& stateName)
		{
			const auto visibleState = visibility.find(stateName);
			return visibleState != visibility.end() && visibleState->second;
		};

		ImGui::SeparatorText("Transitions");
		if (ImGui::BeginTable("EntityStateTransitionDirectionTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Checkbox("incoming", &ui.showIncomingTransitions);
			ImGui::TableSetColumnIndex(1);
			ImGui::Checkbox("outgoing", &ui.showOutgoingTransitions);
			ImGui::EndTable();
		}
		const float transitionBoxWidth = ImGui::GetContentRegionAvail().x;
		ImGui::BeginChild("EntityStateTransitionBox", ImVec2(transitionBoxWidth, 220.0f), true);
		if (ImGui::BeginTable("EntityStateTransitionTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("incoming", ImGuiTableColumnFlags_WidthStretch, 0.5f);
			ImGui::TableSetupColumn("outgoing", ImGuiTableColumnFlags_WidthStretch, 0.5f);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			for (std::size_t transitionIndex = 0; transitionIndex < transitions.size(); ++transitionIndex)
			{
				const EntityStateMachine::Transition& transition = transitions[transitionIndex];
				if (ui.showIncomingTransitions && stateTransitionsAreVisible(ui.visibleStateTransitions, transition.to))
				{
					drawTransitionRow(transition, transitionIndex);
				}
			}

			ImGui::TableSetColumnIndex(1);
			for (std::size_t transitionIndex = 0; transitionIndex < transitions.size(); ++transitionIndex)
			{
				const EntityStateMachine::Transition& transition = transitions[transitionIndex];
				if (ui.showOutgoingTransitions && stateTransitionsAreVisible(ui.visibleStateTransitions, transition.from))
				{
					drawTransitionRow(transition, transitionIndex);
				}
			}

			ImGui::EndTable();
		}
		ImGui::EndChild();

		if (ui.editTransitionPopupRequested)
		{
			ImGui::OpenPopup("Add Transition##AquanactEntityStateMachine");
			ui.editTransitionPopupRequested = false;
		}

		ImGui::Separator();
		if (ImGui::Button("Create Transition"))
		{
			ui.editingTransitionIndex = -1;
			ui.addTransitionPopupInitialized = false;
			ImGui::OpenPopup("Add Transition##AquanactEntityStateMachine");
		}

		ImGui::SetNextWindowSize(ImVec2(720.0f, 0.0f), ImGuiCond_FirstUseEver);
		if (ImGui::BeginPopupModal("Add Transition##AquanactEntityStateMachine", nullptr))
		{
			if (!ui.addTransitionPopupInitialized)
			{
				if (!states.empty())
				{
					copyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), states.front().name);
					if (states.size() > 1)
					{
						copyStateName(ui.transitionToState, sizeof(ui.transitionToState), states[1].name);
					}
				}
				ui.conditions.clear();
				EntityStateMachine::Condition defaultCondition;
				defaultCondition.right.constantValue = 1.0f;
				ui.conditions.push_back(std::move(defaultCondition));
				ui.transitionBlendSeconds = 0.25f;
				ui.transitionWaitForCurrentStateComplete = false;
				ui.addTransitionPopupInitialized = true;
			}

			ImGui::TextUnformatted("From");
			setComboWidthToText(ui.transitionFromState[0] != '\0' ? ui.transitionFromState : "<from>");
			if (ImGui::BeginCombo("##TransitionFrom", ui.transitionFromState[0] != '\0' ? ui.transitionFromState : "<from>"))
			{
				for (const EntityStateMachine::State& state : states)
				{
					const bool selected = std::strcmp(ui.transitionFromState, state.name.c_str()) == 0;
					if (ImGui::Selectable(state.name.c_str(), selected))
					{
						copyStateName(ui.transitionFromState, sizeof(ui.transitionFromState), state.name);
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::TextUnformatted("To");
			setComboWidthToText(ui.transitionToState[0] != '\0' ? ui.transitionToState : "<to>");
			if (ImGui::BeginCombo("##TransitionTo", ui.transitionToState[0] != '\0' ? ui.transitionToState : "<to>"))
			{
				for (const EntityStateMachine::State& state : states)
				{
					const bool selected = std::strcmp(ui.transitionToState, state.name.c_str()) == 0;
					if (ImGui::Selectable(state.name.c_str(), selected))
					{
						copyStateName(ui.transitionToState, sizeof(ui.transitionToState), state.name);
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SetNextItemWidth(120.0f);
			ImGui::InputFloat("Blend Seconds", &ui.transitionBlendSeconds, 0.0f, 0.0f, "%.2f");
			ImGui::Checkbox("Wait for source state to finish before transitioning", &ui.transitionWaitForCurrentStateComplete);
			ImGui::Separator();

			for (std::size_t conditionIndex = 0; conditionIndex < ui.conditions.size(); ++conditionIndex)
			{
				EntityStateMachine::Condition& condition = ui.conditions[conditionIndex];
				if (conditionIndex > 0)
				{
					ImGui::Separator();
				}
				ImGui::PushID(static_cast<int>(conditionIndex));
				ImGui::TextUnformatted("Left Operand");
				ImGui::PushID("Left");
				const bool isBooleanCondition = IsBooleanEntityStateCondition(condition, bindingSources);
				DrawEntityStateOperandEditor("", condition.left, bindingSources, isBooleanCondition);
				ImGui::PopID();
				if (isBooleanCondition && condition.comparator != EntityStateMachine::Comparator::Equal && condition.comparator != EntityStateMachine::Comparator::NotEqual)
				{
					condition.comparator = EntityStateMachine::Comparator::Equal;
				}
				ImGui::Separator();
				ImGui::TextUnformatted("Comparator");
				const char* comparatorOptions[] = { "Equal", "Not Equal", "Greater", "Less", "Greater Equal", "Less Equal" };
				int comparatorIndex = static_cast<int>(condition.comparator);
				if (isBooleanCondition)
				{
					const char* booleanComparatorOptions[] = { "Equal", "Not Equal" };
					comparatorIndex = condition.comparator == EntityStateMachine::Comparator::NotEqual ? 1 : 0;
					if (ImGui::Combo("##Comparator", &comparatorIndex, booleanComparatorOptions, IM_ARRAYSIZE(booleanComparatorOptions)))
					{
						condition.comparator = comparatorIndex == 1 ? EntityStateMachine::Comparator::NotEqual : EntityStateMachine::Comparator::Equal;
					}
				}
				else if (ImGui::Combo("##Comparator", &comparatorIndex, comparatorOptions, IM_ARRAYSIZE(comparatorOptions)))
				{
					condition.comparator = static_cast<EntityStateMachine::Comparator>(comparatorIndex);
				}
				ImGui::Separator();
				ImGui::TextUnformatted("Right Operand");
				ImGui::PushID("Right");
				DrawEntityStateOperandEditor("", condition.right, bindingSources, isBooleanCondition);
				ImGui::PopID();
				ImGui::Separator();
				ImGui::TextUnformatted(conditionToBrowserText(condition).c_str());
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
				EntityStateMachine::Condition condition;
				condition.right.constantValue = 1.0f;
				ui.conditions.push_back(std::move(condition));
			}

			if (ImGui::Button("Create"))
			{
				if (ui.editingTransitionIndex >= 0)
				{
					entityStateMachine.UpdateTransition(static_cast<std::size_t>(ui.editingTransitionIndex), ui.transitionFromState, ui.transitionToState, ui.transitionBlendSeconds, ui.transitionWaitForCurrentStateComplete, ui.conditions);
				}
				else
				{
					entityStateMachine.AddTransition(ui.transitionFromState, ui.transitionToState, ui.transitionBlendSeconds, ui.transitionWaitForCurrentStateComplete, ui.conditions);
					ui.visibleStateTransitions[ui.transitionFromState] = true;
				}
				ui.editingTransitionIndex = -1;
				ui.addTransitionPopupInitialized = false;
				ui.transitionListNeedsRefresh = true;
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
			m_entityStateMachinePopupRequested = false;
			m_entityStateUiState.erase(&entityStateMachine);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	else if (m_entityStateMachinePopupRequested && !ImGui::IsPopupOpen("State Machine##AquanactEntityStateMachine"))
	{
		m_entityStateMachinePopupRequested = false;
		m_entityStateUiState.erase(&entityStateMachine);
	}
}

void EngineGUI::DrawBuildGamePopup()
{
	static char buildPath[512] = "C:\\dev\\Aquanact\\out\\package";
	static std::string statusMessage;
	static bool requestedBuild = false;

	if (m_buildGamePopupRequested)
	{
		// Defer opening until the next frame so the popup state stays stable.
		ImGui::OpenPopup("Build Game##AquanactBuildGame");
		m_buildGamePopupRequested = false;
	}

	if (ImGui::BeginPopupModal("Build Game##AquanactBuildGame", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Let the user choose an output folder, then invoke the build system on demand.
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
		"#include \"Engine/Core/Component.h\"\n\n"
		"class Entity;\n"
		"class EntityStateMachine;\n"
		"class Input;\n"
		"class InputManager;\n"
		"class Root;\n\n"
		"// Generated gameplay component scaffold.\n"
		"//\n"
		"// Contract:\n"
		"// - Name() identifies the component type for the factory and editor\n"
		"// - startUp/Update/FirstFrame provide the runtime lifecycle hooks\n"
		"// - the bindable macros expose values and one-shot events to the UI\n"
		"//\n"
		"// Keep the binding lists as the single source of truth for exposed data.\n"
		"// The macros generate both editor metadata and lookup code.\n"
		"//\n"
		"// Example values:\n"
		"//   #define " + className + "_BINDABLES(BIND_VALUE, BIND_FUNCTION) \\\n"
		"//   \tBIND_VALUE(m_health) \\\n"
		"//   \tBIND_FUNCTION(Health)\n"
		"//   AQUA_DECLARE_BINDABLES(" + className + "_BINDABLES)\n"
		"//   #undef " + className + "_BINDABLES\n"
		"//\n"
		"// Example events:\n"
		"//   #define " + className + "_EVENTS(EVENT) \\\n"
		"//   \tEVENT(HealthChanged, \"Health changed\") \\\n"
		"//   \tEVENT(Died, \"Died\")\n"
		"//   AQUA_EVENTS_BEGIN\n"
		"//   \t" + className + "_EVENTS(AQUA_EVENT)\n"
		"//   AQUA_EVENTS_TEXT_END\n"
		"//   \t" + className + "_EVENTS(AQUA_EVENT_TEXT)\n"
		"//   AQUA_EVENTS_END\n"
		"class " + className + " final : public Component\n"
		"{\n"
		"public:\n"
		"\t" + className + "() = default;\n\n"
		"\tconst char* Name() const override { return \"" + className + "\"; }\n"
		"\tvoid startUp(Entity&) override;\n"
		"\tvoid Update(Entity&, float) override {}\n"
		"\tvoid FirstFrame(Entity&) override {}\n"
		"\n"
		"\t// Put the component's exposed value list here. This is the only place\n"
		"\t// that should enumerate values the editor needs to see.\n"
		"};\n";
}

std::string EngineGUI::MakeSourceTemplate(const std::string& className)
{
	return
		"#include \"Game/" + className + ".h\"\n\n"
		"#include \"Engine/Core/Entity.h\"\n"
		"#include \"Engine/Core/EntityStateMachine.h\"\n"
		"#include \"Engine/Core/Input.h\"\n"
		"#include \"Engine/Core/InputManager.h\"\n"
		"#include \"Engine/Core/Root.h\"\n\n"
		"void " + className + "::startUp(Entity&)\n"
		"{\n"
		"}\n\n"
		"// Keep component metadata in the header with the binding/event list macros.\n"
		"// Add runtime logic here only if the component needs it.\n";
}

void EngineGUI::CreateGameCodeFile(const std::string& className)
{
	// Generated code belongs in the game include/source folders.
	const std::filesystem::path headerPath = GameIncludeRoot() / (className + ".h");
	const std::filesystem::path sourcePath = GameSourceRoot() / (className + ".cpp");
	const std::filesystem::path generatedDir = GeneratedRoot();
	const std::filesystem::path generatedSourcesPath = generatedDir / "GameSources.cmake";
	const std::filesystem::path registryPath = GameRegistryPath();

	// Create parent directories if they do not already exist.
	const std::filesystem::path headerDir = headerPath.parent_path();
	const std::filesystem::path sourceDir = sourcePath.parent_path();
	std::error_code ec;
	std::filesystem::create_directories(headerDir, ec);
	std::filesystem::create_directories(sourceDir, ec);

	// Write the new gameplay class first so the source tree contains the new
	// component before we regenerate any build-time lists from disk.
	const bool headerWritten = Root::Current().FileSystemRef().WriteTextFile(headerPath, MakeHeaderTemplate(className));
	const bool sourceWritten = Root::Current().FileSystemRef().WriteTextFile(sourcePath, MakeSourceTemplate(className));
	bool sourcesListWritten = false;
	bool registryWritten = false;
	if (headerWritten && sourceWritten)
	{
		std::error_code generatedEc;
		std::filesystem::create_directories(generatedDir, generatedEc);
		// Regenerate the maintenance files from the actual source tree so the
		// build list and the factory registry stay aligned with what exists.
		sourcesListWritten = Root::Current().FileSystemRef().WriteTextFile(generatedSourcesPath, MakeGameSourcesList());
		registryWritten = Root::Current().FileSystemRef().WriteTextFile(registryPath, MakeComponentRegistryTemplate(CollectGameComponentNames()));
	}

	// Report one success/failure message back to the popup.
	if (headerWritten && sourceWritten && sourcesListWritten && registryWritten)
	{
		m_addCodeFileStatusMessage = "Created " + headerPath.string() + ", " + sourcePath.string() + ", " + registryPath.string() + " and updated " + generatedSourcesPath.string();
	}
	else
	{
		m_addCodeFileStatusMessage = "Failed to create one or more files.";
	}
}

void EngineGUI::CreateEntity()
{

}

void EngineGUI::UpdateEntity(Entity* entity)
{

}

void EngineGUI::StartRebuild()
{
	// TODO
}

void EngineGUI::OnRebuildFinished()
{
	//TODO
}

void EngineGUI::SaveNewClassConfiguration(NewClassConfiguration& configuration)
{
	std::string filename = "NewClassConfiguration";

	// Open the text file for writing
	std::ofstream outFile(filename);

	// Check if the file opened successfully
	if (!outFile)
	{
		std::cerr << "Error: Could not open file " << filename << " for writing.\n";
		return;
	}

	// std::boolalpha forces bools to print as "true"/"false" instead of 1/0
	outFile << "ClassName: " << configuration.className << "\n";
	outFile << "AttachToExistingEntity: " << std::boolalpha << configuration.attachToExistingEntity << "\n";
	outFile << "CreateNewEntity: " << std::boolalpha << configuration.createNewEntity << "\n";
	outFile << "TargetEntityName: " << configuration.targetEntityName << "\n";

	// File closes automatically when outFile goes out of scope
}

void EngineGUI::DrawAddCodeFilePopup()
{

	if (m_addCodeFilePopupRequested)
	{
		// Clear stale text so each open starts fresh.
		m_newCodeFileName[0] = '\0';
		ImGui::OpenPopup("Add Code File##AquanactAddCodeFile");
		m_addCodeFilePopupRequested = false;
		m_addCodeFileCreated = false;
		m_addCodeFileStatusMessage.clear();
	}

	Entity* newEntity    = nullptr;
	Entity* updateEntity = nullptr;

		if (ImGui::BeginPopupModal("Add Code File##AquanactAddCodeFile", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			// Normalize the class name before generating files so the output is valid C++.
			ImGui::TextUnformatted("Create a new gameplay class:");
			ImGui::InputText("Class Name", m_newCodeFileName, sizeof(m_newCodeFileName));

		// Build the entity list for the attachment dropdown.
		Scene* activeScene = Root::Current().Scenes().ActiveLevel();
		static const std::vector<std::unique_ptr<Entity>> emptyEntities;
		const std::vector<std::unique_ptr<Entity>>& entities = activeScene ? activeScene->Objects() : emptyEntities;
		std::vector<std::string> entityNames;
		entityNames.reserve(entities.size() + 1);
		entityNames.push_back("none");

		for (std::size_t i = 0; i < entities.size(); ++i)
		{
			const std::unique_ptr<Entity>& entity = entities[i];
			entityNames.push_back(entity ? entity->Name() : "<unnamed>");
		}
		
		if (m_createAndBuildEntityIndex < 0 || m_createAndBuildEntityIndex >= static_cast<int>(entityNames.size()))
		{
			m_createAndBuildEntityIndex = 0;
		}
		const char* default_item = entityNames[static_cast<std::size_t>(m_createAndBuildEntityIndex)].c_str();
		
		// Selection index 0 means "none", which creates a brand new entity on startup.
		if (ImGui::BeginCombo("Entity", default_item))
		{
			// The combo includes a synthetic "none" entry at index 0, so real
			// entities are shifted by one slot.
			for (int i = 0; i < static_cast<int>(entityNames.size()); ++i)
			{
				const bool is_selected = (m_createAndBuildEntityIndex == i);
				if (ImGui::Selectable(entityNames[i].c_str(), is_selected))
				{
					// Store the choice immediately so the next button press reads the
					// same target the user just selected.
					m_createAndBuildEntityIndex = i;
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		// Resolve the selected target after the combo so the button logic always
		// sees the same entity the popup is displaying.
		updateEntity = (m_createAndBuildEntityIndex > 0
			&& static_cast<std::size_t>(m_createAndBuildEntityIndex - 1) < entities.size())
			? entities[static_cast<std::size_t>(m_createAndBuildEntityIndex - 1)].get()
			: nullptr;

		if (ImGui::Button("Create and Build"))
		{
			const std::string className = NormalizeGameClassName(m_newCodeFileName);
			if (className.empty())
			{
				m_addCodeFileStatusMessage = "Enter a valid class name.";
			}
			else
			{
				// Stage the normalized name and open a confirmation popup so the
				// user sees the shutdown/rebuild behavior before it happens.
				m_createAndBuildClassName = className;
				m_createAndBuildPopupRequested = true;
			}
		}

		if (m_createAndBuildPopupRequested)
		{
			// Open the modal once per request; the persistent state keeps the
			// selection alive until the user confirms or cancels.
			ImGui::OpenPopup("Create and Build##AquanactCreateAndBuild");
			m_createAndBuildPopupRequested = false;
		}

		if (ImGui::BeginPopupModal("Create and Build##AquanactCreateAndBuild", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			// The popup explains the workflow before any files are generated so the
			// user knows the editor will save, exit, and expect a rebuild.
			const bool attachToExistingEntity = (updateEntity != nullptr);
			const std::string selectedEntityLabel = updateEntity ? updateEntity->Name() : "none";
			ImGui::Text("Create a new component type: %s", m_createAndBuildClassName.c_str());
			ImGui::Text("Selected entity: %s", selectedEntityLabel.c_str());
			// This branch mirrors the dropdown selection above: if an entity was
			// selected, the new class will be attached to it; otherwise startup will
			// create a brand new entity for the component.
			if (attachToExistingEntity)
			{
				ImGui::Text("It will be attached to entity: %s", updateEntity->Name().c_str());
			}
			else
			{
				ImGui::TextUnformatted("It will be created as a new entity component.");
			}
			ImGui::TextWrapped("The editor will generate the new class, save the project, and then shut down so you can rebuild the game.");
			ImGui::TextWrapped("Please rebuild after the editor closes to compile the new type into the project.");

			if (ImGui::Button("Create and Exit"))
			{
				// Build the class files and record the startup handoff before closing
				// the editor. The generated files are written first so the rebuild has
				// the new source available immediately.
				CreateGameCodeFile(m_createAndBuildClassName);

				// Save the exact startup choice the popup described above so the next
				// launch can attach to the same entity or create a new one.
				NewClassConfiguration configuration;
				configuration.className = m_createAndBuildClassName;
				// Match the confirmation text exactly: attach to the selected entity
				// when one is chosen, otherwise create a brand new entity on startup.
				configuration.attachToExistingEntity = attachToExistingEntity;
				configuration.targetEntityName = updateEntity ? updateEntity->Name() : "";
				configuration.createNewEntity = !attachToExistingEntity;
				SaveNewClassConfiguration(configuration);

				// Preserve all existing editor changes before leaving for the rebuild.
				ProjectManager& projectManager = Root::Current().Projects();
				if (!projectManager.CurrentProjectPath().empty())
				{
					projectManager.SaveProject(projectManager.CurrentProjectPath(), Root::Current().Scenes());
				}

				// The project needs to restart/build outside the editor after the new
				// type is generated, so request window close now.
				glfwSetWindowShouldClose(m_window->GLFW(), GLFW_TRUE);
				m_createAndBuildClassName.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				m_createAndBuildClassName.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
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

void EngineGUI::DrawInputMapWindow()
{
	if (!m_showInputMapWindow)
	{
		return;
	}

	InputManager& inputManager = Root::Current().InputActions();
	InputMapUiState& ui = m_inputMapUi;

	// Find bindings by their purpose rather than their current button. This keeps
	// an edited direction associated with its row after its key changes.
	const auto findDirectionalBinding =
		[](std::vector<InputBinding>& bindings, InputBindingType type, const glm::vec2& direction) -> InputBinding*
	{
		for (InputBinding& binding : bindings)
		{
			if (binding.type == type && binding.vector == direction)
			{
				return &binding;
			}
		}
		return nullptr;
	};

	const auto findFirstBindingOfType =
		[](std::vector<InputBinding>& bindings, InputBindingType type) -> InputBinding*
	{
		for (InputBinding& binding : bindings)
		{
			if (binding.type == type)
			{
				return &binding;
			}
		}
		return nullptr;
	};

	// Keep Move first, then present every other runtime action alphabetically.
	std::vector<std::string> actionNames;
	actionNames.reserve(inputManager.Bindings().size());
	for (const auto& [actionName, bindings] : inputManager.Bindings())
	{
		(void)bindings;
		actionNames.push_back(actionName);
	}
	std::sort(actionNames.begin(), actionNames.end(), [](const std::string& left, const std::string& right)
	{
		if (left == right) return false;
		if (left == "Move") return true;
		if (right == "Move") return false;
		return left < right;
	});

	if (inputManager.Bindings().find(ui.selectedAction) == inputManager.Bindings().end())
	{
		ui.selectedAction = inputManager.Bindings().contains("Move")
			? "Move"
			: (actionNames.empty() ? std::string{} : actionNames.front());
	}

	bool open = m_showInputMapWindow;
	ImGui::SetNextWindowSize(ImVec2(540.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Input Map", &open, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		// Action selection and creation
		ImGui::TextUnformatted("Mapping Action");
		ImGui::SameLine();
			// Keep the action selector compact so the creation button stays visible.
			ImGui::SetNextItemWidth(180.0f);
		if (ImGui::BeginCombo("##InputAction", ui.selectedAction.empty() ? "<select action>" : ui.selectedAction.c_str()))
		{
			for (const std::string& actionName : actionNames)
			{
				const bool selected = actionName == ui.selectedAction;
				if (ImGui::Selectable(actionName.c_str(), selected))
				{
					ui.selectedAction = actionName;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Add New Action"))
		{
			ui.newActionName[0] = '\0';
			ui.statusMessage.clear();
			ui.addActionPopupRequested = true;
		}

		if (ui.addActionPopupRequested)
		{
			ImGui::OpenPopup("Add Input Action##AquanactInputMap");
			ui.addActionPopupRequested = false;
		}

		if (ImGui::BeginPopupModal("Add Input Action##AquanactInputMap", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Action Name");
			ImGui::SetNextItemWidth(280.0f);
			ImGui::InputText("##NewInputActionName", ui.newActionName, sizeof(ui.newActionName));

			if (ImGui::Button("Create"))
			{
				std::string actionName = ui.newActionName;
				const auto firstCharacter = std::find_if_not(actionName.begin(), actionName.end(), [](unsigned char ch) { return std::isspace(ch); });
				const auto lastCharacter = std::find_if_not(actionName.rbegin(), actionName.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
				actionName = firstCharacter < lastCharacter ? std::string(firstCharacter, lastCharacter) : std::string{};

				if (actionName.empty())
				{
					ui.statusMessage = "Enter an action name.";
				}
				else if (inputManager.Bindings().contains(actionName))
				{
					ui.statusMessage = "That action already exists.";
				}
				else
				{
					inputManager.SetBindings(actionName, {});
					ui.selectedAction = actionName;
					ui.statusMessage.clear();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				ui.statusMessage.clear();
				ImGui::CloseCurrentPopup();
			}
			if (!ui.statusMessage.empty())
			{
				ImGui::TextUnformatted(ui.statusMessage.c_str());
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();

		const auto bindingIt = inputManager.Bindings().find(ui.selectedAction);
		if (bindingIt != inputManager.Bindings().end())
		{
			std::vector<InputBinding> editedBindings = bindingIt->second;
			bool bindingsChanged = false;

			// Shared keyboard selector for Move's four vector directions.
			const auto drawKeyboardDirection = [&](const char* label, int defaultKey, const glm::vec2& direction)
			{
				InputBinding* binding = findDirectionalBinding(editedBindings, InputBindingType::Key, direction);
				if (!binding)
				{
					editedBindings.push_back({ InputBindingType::Key, defaultKey, GLFW_JOYSTICK_1, 1.0f, direction });
					binding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo(label, InputBindingLabel(*binding).c_str()))
				{
					const int keyOptions[] = {
						GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
						GLFW_KEY_UP, GLFW_KEY_LEFT, GLFW_KEY_DOWN, GLFW_KEY_RIGHT
					};
					for (const int keyCode : keyOptions)
					{
						const InputBinding option{ InputBindingType::Key, keyCode, GLFW_JOYSTICK_1, 1.0f, direction };
						const bool selected = keyCode == binding->code;
						const std::string optionLabel = InputBindingLabel(option);
						if (ImGui::Selectable(optionLabel.c_str(), selected))
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

			ImGui::PushID(ui.selectedAction.c_str());

			if (ui.selectedAction == "Move")
			{
				ImGui::TextDisabled("Keyboard / Mouse");
				ImGui::Separator();
				drawKeyboardDirection("Up##KeyboardUp", GLFW_KEY_W, glm::vec2(0.0f, 1.0f));
				drawKeyboardDirection("Down##KeyboardDown", GLFW_KEY_S, glm::vec2(0.0f, -1.0f));
				drawKeyboardDirection("Left##KeyboardLeft", GLFW_KEY_A, glm::vec2(-1.0f, 0.0f));
				drawKeyboardDirection("Right##KeyboardRight", GLFW_KEY_D, glm::vec2(1.0f, 0.0f));

				ImGui::Spacing();
				ImGui::TextDisabled("Controller");
				ImGui::Separator();
				const char* controllerModes[] = { "Digital", "Analog" };
				int controllerMode = findFirstBindingOfType(editedBindings, InputBindingType::ControllerStick) ? 1 : 0;
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
					const InputBinding defaults[] = {
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, 1.0f) },
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f, -1.0f) },
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(-1.0f, 0.0f) },
						{ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
					};

					for (int directionIndex = 0; directionIndex < IM_ARRAYSIZE(defaults); ++directionIndex)
					{
						InputBinding* binding = findDirectionalBinding(editedBindings, InputBindingType::ControllerDigital, defaults[directionIndex].vector);
						if (!binding)
						{
							editedBindings.push_back(defaults[directionIndex]);
							binding = &editedBindings.back();
							bindingsChanged = true;
						}

						if (ImGui::BeginCombo(labels[directionIndex], InputBindingLabel(*binding).c_str()))
						{
							for (const InputBinding& optionTemplate : defaults)
							{
								InputBinding option = optionTemplate;
								option.vector = defaults[directionIndex].vector;
								const bool selected = option.code == binding->code;
								const std::string optionLabel = InputBindingLabel(option);
								if (ImGui::Selectable(optionLabel.c_str(), selected))
								{
									*binding = option;
									bindingsChanged = true;
								}
								if (selected) ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
				}
				else
				{
					InputBinding* stickBinding = findFirstBindingOfType(editedBindings, InputBindingType::ControllerStick);
					if (!stickBinding)
					{
						editedBindings.push_back({ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Left });
						stickBinding = &editedBindings.back();
						bindingsChanged = true;
					}

					if (ImGui::BeginCombo("Stick##MoveControllerStick", InputBindingLabel(*stickBinding).c_str()))
					{
						const InputStick stickOptions[] = { InputStick::Left, InputStick::Right };
						for (const InputStick stick : stickOptions)
						{
							const bool selected = stick == stickBinding->stick;
							const char* label = stick == InputStick::Left ? "Left Stick" : "Right Stick";
							if (ImGui::Selectable(label, selected))
							{
								stickBinding->stick = stick;
								bindingsChanged = true;
							}
							if (selected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}
			else if (ui.selectedAction == "Look")
			{
				ImGui::TextDisabled("Keyboard / Mouse");
				ImGui::Separator();
				ImGui::TextUnformatted("Mouse Movement");

				ImGui::Spacing();
				ImGui::TextDisabled("Controller");
				ImGui::Separator();
				InputBinding* stickBinding = findFirstBindingOfType(editedBindings, InputBindingType::ControllerStick);
				if (!stickBinding)
				{
					editedBindings.push_back({ InputBindingType::ControllerStick, 0, GLFW_JOYSTICK_1, 1.0f, glm::vec2(0.0f), InputStick::Right });
					stickBinding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo("Stick##LookControllerStick", InputBindingLabel(*stickBinding).c_str()))
				{
					const InputStick stickOptions[] = { InputStick::Left, InputStick::Right };
					for (const InputStick stick : stickOptions)
					{
						const bool selected = stick == stickBinding->stick;
						const char* label = stick == InputStick::Left ? "Left Stick" : "Right Stick";
						if (ImGui::Selectable(label, selected))
						{
							stickBinding->stick = stick;
							bindingsChanged = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			else
			{
				// New actions are digital and receive one desktop and one controller binding.
				ImGui::TextDisabled("Keyboard / Mouse");
				ImGui::Separator();
				InputBinding* desktopBinding = nullptr;
				for (InputBinding& binding : editedBindings)
				{
					if (binding.type == InputBindingType::Key || binding.type == InputBindingType::MouseButton)
					{
						desktopBinding = &binding;
						break;
					}
				}
				if (!desktopBinding)
				{
					editedBindings.push_back({ InputBindingType::Key, GLFW_KEY_SPACE, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) });
					desktopBinding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo("Button##DesktopActionButton", InputBindingLabel(*desktopBinding).c_str()))
				{
					const InputBinding desktopOptions[] = {
						{ InputBindingType::Key, GLFW_KEY_SPACE, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_ENTER, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_E, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_F, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::Key, GLFW_KEY_Q, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::MouseButton, GLFW_MOUSE_BUTTON_LEFT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::MouseButton, GLFW_MOUSE_BUTTON_RIGHT, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
						{ InputBindingType::MouseButton, GLFW_MOUSE_BUTTON_MIDDLE, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) },
					};
					for (const InputBinding& option : desktopOptions)
					{
						const bool selected = option.type == desktopBinding->type && option.code == desktopBinding->code;
						const std::string optionLabel = InputBindingLabel(option);
						if (ImGui::Selectable(optionLabel.c_str(), selected))
						{
							*desktopBinding = option;
							bindingsChanged = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::Spacing();
				ImGui::TextDisabled("Controller");
				ImGui::Separator();
				InputBinding* controllerBinding = findFirstBindingOfType(editedBindings, InputBindingType::ControllerDigital);
				if (!controllerBinding)
				{
					editedBindings.push_back({ InputBindingType::ControllerDigital, GLFW_GAMEPAD_BUTTON_A, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) });
					controllerBinding = &editedBindings.back();
					bindingsChanged = true;
				}

				if (ImGui::BeginCombo("Button##ControllerActionButton", InputBindingLabel(*controllerBinding).c_str()))
				{
					const int buttonOptions[] = {
						GLFW_GAMEPAD_BUTTON_A, GLFW_GAMEPAD_BUTTON_B, GLFW_GAMEPAD_BUTTON_X, GLFW_GAMEPAD_BUTTON_Y,
						GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
						GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
						GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT
					};
					for (const int buttonCode : buttonOptions)
					{
						const InputBinding option{ InputBindingType::ControllerDigital, buttonCode, GLFW_JOYSTICK_1, 1.0f, glm::vec2(1.0f, 0.0f) };
						const bool selected = buttonCode == controllerBinding->code;
						const std::string optionLabel = InputBindingLabel(option);
						if (ImGui::Selectable(optionLabel.c_str(), selected))
						{
							*controllerBinding = option;
							bindingsChanged = true;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}

			ImGui::PopID();

			if (bindingsChanged)
			{
				inputManager.SetBindings(ui.selectedAction, std::move(editedBindings));
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
		// Clear the previous result so the popup reads like a fresh workflow.
		ImGui::OpenPopup("New Scene##AquanactNewLevel");
		m_newLevelPopupRequested = false;
		m_newLevelStatusMessage.clear();
	}

	if (ImGui::BeginPopupModal("New Scene##AquanactNewLevel", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Create a new scene only after sanitizing the user-provided name.
		ImGui::TextUnformatted("Create a new scene:");
		ImGui::InputText("Name", m_newLevelName, sizeof(m_newLevelName));

		if (ImGui::Button("Create"))
		{
			const std::string levelName = NormalizeLevelName(m_newLevelName);
			if (levelName.empty())
			{
				m_newLevelStatusMessage = "Enter a valid scene name.";
			}
			else if (Root::Current().Scenes().FindLevel(levelName))
			{
				m_newLevelStatusMessage = "Scene already exists.";
			}
			else
			{
				Scene* newScene = Root::Current().Scenes().CreateLevel(levelName);
				if (newScene)
				{
					Root::Current().Scenes().SetActiveLevel(levelName);
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




