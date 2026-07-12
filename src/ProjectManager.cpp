#include "ProjectManager.h"

#include "Debug.h"
#include "Globals.h"
#include "Object3D.h"
#include "SceneManager.h"

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
		if (!std::filesystem::exists(sourcePath))
		{
			return sourcePath;
		}

		const std::filesystem::path projectDir = projectPath.parent_path();
		std::error_code ec;
		const std::filesystem::path projectRelative = std::filesystem::relative(sourcePath, projectDir, ec);
		if (!ec && !projectRelative.empty() && !IsRelativeToParent(projectRelative))
		{
			return projectRelative;
		}

		const std::filesystem::path assetsRoot = AssetsRoot();
		const std::filesystem::path assetsRelative = std::filesystem::relative(sourcePath, assetsRoot, ec);
		if (!ec && !assetsRelative.empty() && !IsRelativeToParent(assetsRelative))
		{
			return assetsRelative;
		}

		return sourcePath;
	}

	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath)
	{
		if (std::filesystem::exists(sourcePath))
		{
			return sourcePath;
		}

		const std::filesystem::path projectDir = projectPath.parent_path();
		const std::filesystem::path projectRelative = projectDir / sourcePath;
		if (std::filesystem::exists(projectRelative))
		{
			return projectRelative;
		}

		const std::filesystem::path assetsRelative = AssetsRoot() / sourcePath;
		if (std::filesystem::exists(assetsRelative))
		{
			return assetsRelative;
		}

		const std::filesystem::path searchRoots[] = { ModelsRoot(), TexturesRoot(), ProjectsRoot() };
		for (const auto& searchRoot : searchRoots)
		{
			if (!std::filesystem::exists(searchRoot) || !std::filesystem::is_directory(searchRoot))
			{
				continue;
			}

			for (const auto& entry : std::filesystem::recursive_directory_iterator(searchRoot))
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

bool ProjectManager::SaveProject(const std::filesystem::path& path, const SceneManager& sceneManager) const
{
	std::ofstream file(path);
	if (!file.is_open())
	{
		return false;
	}

	file << "AquanactProject 1\n";
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
		file
			<< "object;"
			<< EscapeField(portableSourcePath.string()) << ";"
			<< position.x << ";" << position.y << ";" << position.z << ";"
			<< rotation.x << ";" << rotation.y << ";" << rotation.z << ";"
			<< scale.x << ";" << scale.y << ";" << scale.z << "\n";
	}

	return true;
}

bool ProjectManager::LoadProject(const std::filesystem::path& path, SceneManager& sceneManager) const
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		return false;
	}

	std::string header;
	std::getline(file, header);
	if (header != "AquanactProject 1")
	{
		return false;
	}

	std::vector<std::unique_ptr<Object3D>> loadedObjects;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}

		const std::vector<std::string> fields = SplitFields(line);
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
	for (auto& object : loadedObjects)
	{
		sceneManager.AddObject(std::move(object));
	}

	return true;
}
