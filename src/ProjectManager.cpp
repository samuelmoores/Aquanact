#include "ProjectManager.h"

#include "Debug.h"
#include "Controller.h"
#include "AnimatorComponent.h"
#include "Globals.h"
#include "Object3D.h"
#include "FrontEndManager.h"
#include "FileSystem.h"
#include "SceneManager.h"
#include "RenderManager.h"
#include "GameplayManager.h"

#include <glm/glm.hpp>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
	std::string EscapeField(const std::string& value)
	{
		std::string escaped;
		escaped.reserve(value.size());
		for (char ch : value)
		{
			if (ch == '\\' || ch == ';')
			{
				escaped.push_back('\\');
			}
			escaped.push_back(ch);
		}
		return escaped;
	}

	std::string UnescapeField(const std::string& value)
	{
		std::string unescaped;
		unescaped.reserve(value.size());
		bool escapeNext = false;
		for (char ch : value)
		{
			if (escapeNext)
			{
				unescaped.push_back(ch);
				escapeNext = false;
			}
			else if (ch == '\\')
			{
				escapeNext = true;
			}
			else
			{
				unescaped.push_back(ch);
			}
		}
		return unescaped;
	}

	std::vector<std::string> SplitFields(const std::string& line)
	{
		std::vector<std::string> fields;
		std::string current;
		bool escapeNext = false;
		for (char ch : line)
		{
			if (escapeNext)
			{
				current.push_back(ch);
				escapeNext = false;
			}
			else if (ch == '\\')
			{
				escapeNext = true;
			}
			else if (ch == ';')
			{
				fields.push_back(current);
				current.clear();
			}
			else
			{
				current.push_back(ch);
			}
		}
		fields.push_back(current);
		return fields;
	}

	bool IsRelativeToParent(const std::filesystem::path& relativePath)
	{
		auto it = relativePath.begin();
		if (it == relativePath.end())
		{
			return false;
		}

		const std::filesystem::path first = *it;
		return first == std::filesystem::path("..");
	}

	std::filesystem::path AssetsRoot()
	{
		return std::filesystem::path("C:/dev/Aquanact/assets");
	}

	std::filesystem::path ModelsRoot()
	{
		return AssetsRoot() / "models";
	}

	std::filesystem::path TexturesRoot()
	{
		return AssetsRoot() / "textures";
	}

	std::filesystem::path ProjectsRoot()
	{
		return AssetsRoot() / "projects";
	}

	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath)
	{
		if (!gFileSystem.Exists(sourcePath))
		{
			return sourcePath;
		}

		const std::filesystem::path projectDir = projectPath.parent_path();
		std::error_code ec;
		const std::filesystem::path projectRelative = gFileSystem.Relative(sourcePath, projectDir, ec);
		if (!ec && !projectRelative.empty() && !IsRelativeToParent(projectRelative))
		{
			return projectRelative;
		}

		const std::filesystem::path assetsRoot = AssetsRoot();
		const std::filesystem::path assetsRelative = gFileSystem.Relative(sourcePath, assetsRoot, ec);
		if (!ec && !assetsRelative.empty() && !IsRelativeToParent(assetsRelative))
		{
			return assetsRelative;
		}

		return sourcePath;
	}

	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath)
	{
		if (gFileSystem.Exists(sourcePath))
		{
			return sourcePath;
		}

		const std::filesystem::path projectDir = projectPath.parent_path();
		const std::filesystem::path projectRelative = projectDir / sourcePath;
		if (gFileSystem.Exists(projectRelative))
		{
			return projectRelative;
		}

		const std::filesystem::path assetsRelative = AssetsRoot() / sourcePath;
		if (gFileSystem.Exists(assetsRelative))
		{
			return assetsRelative;
		}

		const std::filesystem::path searchRoots[] = { ModelsRoot(), TexturesRoot(), ProjectsRoot() };
		for (const auto& searchRoot : searchRoots)
		{
			if (!gFileSystem.Exists(searchRoot) || !gFileSystem.IsDirectory(searchRoot))
			{
				continue;
			}

			for (const auto& entry : gFileSystem.ReadDirectoryRecursive(searchRoot))
			{
				if (!entry.is_regular_file())
				{
					continue;
				}

				if (entry.path().filename() == sourcePath.filename())
				{
					return entry.path();
				}
			}
		}

		return projectRelative;
	}
}

ProjectManager::ProjectManager(FileSystem& fileSystem)
	: m_fileSystem(&fileSystem)
{
}

bool ProjectManager::SaveProject(const std::filesystem::path& path, const SceneManager& sceneManager) const
{
	if (!m_fileSystem)
	{
		return false;
	}

	std::string contents = "AquanactProject 3\n";
	for (const auto& object : sceneManager.Objects())
	{
		if (!object)
		{
			continue;
		}

		const glm::vec3 position = object->Position();
		const glm::vec3 rotation = object->Rotation();
		const glm::vec3 scale = object->Scale();

		const std::filesystem::path portableSourcePath = MakePortableSourcePath(path, object->SourcePath());
		contents += "object;";
		contents += EscapeField(portableSourcePath.string());
		contents += ";";
		contents += std::to_string(position.x) + ";" + std::to_string(position.y) + ";" + std::to_string(position.z) + ";";
		contents += std::to_string(rotation.x) + ";" + std::to_string(rotation.y) + ";" + std::to_string(rotation.z) + ";";
		contents += std::to_string(scale.x) + ";" + std::to_string(scale.y) + ";" + std::to_string(scale.z) + "\n";
	}

	for (const auto& object : sceneManager.Objects())
	{
		if (!object)
		{
			continue;
		}

		if (Controller* controller = object->GetController())
		{
			contents += "component;";
			contents += EscapeField(MakePortableSourcePath(path, object->SourcePath()).string());
			contents += ";controller;";
			contents += std::to_string(controller->MoveSpeed());
			contents += "\n";
		}
	}

	const glm::vec3 gameCameraPosition = gRenderManager.GetGameCamera().GetPosition();
	const glm::vec3 gameCameraFacing = gRenderManager.GetGameCamera().GetFacing();
	contents += "gamecamera;";
	contents += std::to_string(gameCameraPosition.x) + ";" + std::to_string(gameCameraPosition.y) + ";" + std::to_string(gameCameraPosition.z) + ";";
	contents += std::to_string(gameCameraFacing.x) + ";" + std::to_string(gameCameraFacing.y) + ";" + std::to_string(gameCameraFacing.z) + "\n";

	// Persist the runtime GameGUI placement list with the project so the same
	// UI assets are restored when the scene is reopened.
	for (const auto& assetName : gFrontEndManager.RuntimeGUI().SceneAssets())
	{
		contents += "gameguiasset;";
		contents += EscapeField(assetName);
		contents += "\n";
	}

	// Keep track of which GameGUI asset was active when the project was saved.
	const std::string activeGameGUIAsset = gFrontEndManager.RuntimeGUI().ActiveAssetName();
	if (!activeGameGUIAsset.empty())
	{
		contents += "gameguiactive;";
		contents += EscapeField(activeGameGUIAsset);
		contents += "\n";
	}

	return m_fileSystem->WriteTextFile(path, contents);
}

bool ProjectManager::LoadProject(const std::filesystem::path& path, SceneManager& sceneManager) const
{
	if (!m_fileSystem)
	{
		return false;
	}

	const std::string fileContents = m_fileSystem->ReadTextFile(path);
	if (fileContents.empty())
	{
		return false;
	}

	std::istringstream file(fileContents);
	std::string header;
	std::getline(file, header);
	if (header != "AquanactProject 1" && header != "AquanactProject 2" && header != "AquanactProject 3")
	{
		return false;
	}

	std::vector<std::unique_ptr<Object3D>> loadedObjects;
	struct PendingController {
		std::filesystem::path sourcePath;
		float moveSpeed = 50.0f;
	};
	std::vector<PendingController> pendingControllers;
	// GameGUI state is loaded in two phases: we collect the saved asset names
	// here, then hand them back to the runtime GUI after the scene finishes loading.
	std::vector<std::string> pendingGameGUIAssets;
	std::string pendingActiveGameGUIAsset;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}

		const std::vector<std::string> fields = SplitFields(line);
		if (fields.size() == 7 && fields[0] == "gamecamera")
		{
			try
			{
				gRenderManager.GetGameCamera().SetPose(
					glm::vec3(
						std::stof(fields[1]),
						std::stof(fields[2]),
						std::stof(fields[3])),
					glm::vec3(
						std::stof(fields[4]),
						std::stof(fields[5]),
						std::stof(fields[6])));
			}
			catch (const std::exception& ex)
			{
				gDebug.LogMessage("Failed to load game camera from line: " + line);
				gDebug.LogMessage("Reason: " + std::string(ex.what()));
				return false;
			}
			continue;
		}

		if (fields.size() == 2 && fields[0] == "gameguiasset")
		{
			pendingGameGUIAssets.push_back(UnescapeField(fields[1]));
			continue;
		}

		if (fields.size() == 2 && fields[0] == "gameguiactive")
		{
			pendingActiveGameGUIAsset = UnescapeField(fields[1]);
			continue;
		}

		if (fields.size() == 4 && fields[0] == "component" && fields[2] == "controller")
		{
			try
			{
				pendingControllers.push_back(PendingController{
					ResolveSourcePath(path, fields[1]),
					std::stof(fields[3])
				});
			}
			catch (const std::exception& ex)
			{
				gDebug.LogMessage("Failed to load controller component from line: " + line);
				gDebug.LogMessage("Reason: " + std::string(ex.what()));
				return false;
			}
			continue;
		}

		if (fields.size() != 11 || fields[0] != "object")
		{
			continue;
		}

		try
		{
			const std::filesystem::path sourcePath = ResolveSourcePath(path, fields[1]);
			gDebug.LogMessage("Loading mesh: " + sourcePath.string());
			const std::string sourcePathString = sourcePath.string();
			auto object = std::make_unique<Object3D>(sourcePathString.c_str());
			object->Translate(glm::vec3(
				std::stof(fields[2]),
				std::stof(fields[3]),
				std::stof(fields[4])));
			object->SetRotation(glm::vec3(
				std::stof(fields[5]),
				std::stof(fields[6]),
				std::stof(fields[7])));
			object->SetScale(glm::vec3(
				std::stof(fields[8]),
				std::stof(fields[9]),
				std::stof(fields[10])));
			object->SetIgnoreCameraCollision(true);
			loadedObjects.push_back(std::move(object));
		}
		catch (const std::exception& ex)
		{
			gDebug.LogMessage("Failed to load project object from line: " + line);
			gDebug.LogMessage("Reason: " + std::string(ex.what()));
			return false;
		}
	}

	sceneManager.Clear();
	gGameplayManager.shutDown();
	for (auto& object : loadedObjects)
	{
		Object3D* loadedObject = sceneManager.AddObject(std::move(object));
		if (loadedObject)
		{
			if (Controller* controller = loadedObject->GetController())
			{
				gGameplayManager.RegisterController(controller);
			}
		}
	}

	for (const auto& pendingController : pendingControllers)
	{
		for (const auto& object : sceneManager.Objects())
		{
			if (!object || object->SourcePath() != pendingController.sourcePath.string())
			{
				continue;
			}

			if (object->GetController())
			{
				continue;
			}

			Controller* controller = object->AddComponent<Controller>();
			controller->SetOwner(object.get());
			controller->SetMoveSpeed(pendingController.moveSpeed);
			gGameplayManager.RegisterController(controller);
			break;
		}
	}

	// Rebuild the runtime GameGUI placement list after the scene and controllers
	// are restored so the UI matches the saved project state.
	gFrontEndManager.RuntimeGUI().SetSceneAssets(pendingGameGUIAssets);
	if (!pendingActiveGameGUIAsset.empty())
	{
		gFrontEndManager.RuntimeGUI().ActivateAsset(pendingActiveGameGUIAsset);
	}

	return true;
}
