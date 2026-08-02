#include "Engine/Core/ProjectStateFormat.h"

#include "Engine/Core/Entity.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/SceneManager.h"
#include "Engine/Core/ProjectStateData.h"
#include "Engine/Core/AnimatorComponent.h"
#include "Engine/Core/Controller.h"
#include "Engine/Core/PlayerController.h"
#include "Game/Enemy.h"
#include "Game/PlayerHealth.h"

#include <glm/glm.hpp>

namespace ProjectStateFormat {
	namespace {
		bool IsRelativeToParent(const std::filesystem::path& relativePath)
		{
			auto it = relativePath.begin();
			if (it == relativePath.end())
			{
				return false;
			}
			return *it == std::filesystem::path("..");
		}

		std::filesystem::path AssetsRoot()
		{
			return std::filesystem::path("C:/dev/Aquanact/assets");
		}

		std::filesystem::path ModelsRoot() { return AssetsRoot() / "models"; }
		std::filesystem::path TexturesRoot() { return AssetsRoot() / "textures"; }
		std::filesystem::path ProjectsRoot() { return AssetsRoot() / "projects"; }
	}

	std::string EscapeField(const std::string& value)
	{
		std::string escaped;
		escaped.reserve(value.size());
		for (char ch : value)
		{
			if (ch == '\\' || ch == ';') escaped.push_back('\\');
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
			if (escapeNext) { unescaped.push_back(ch); escapeNext = false; }
			else if (ch == '\\') { escapeNext = true; }
			else { unescaped.push_back(ch); }
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
			if (escapeNext) { current.push_back(ch); escapeNext = false; }
			else if (ch == '\\') { escapeNext = true; }
			else if (ch == ';') { fields.push_back(current); current.clear(); }
			else { current.push_back(ch); }
		}
		fields.push_back(current);
		return fields;
	}

	std::string HexEncode(const char* data, std::size_t size)
	{
		static constexpr char digits[] = "0123456789ABCDEF";
		std::string encoded;
		encoded.reserve(size * 2);
		for (std::size_t i = 0; i < size; ++i)
		{
			const unsigned char byte = static_cast<unsigned char>(data[i]);
			encoded.push_back(digits[(byte >> 4) & 0xF]);
			encoded.push_back(digits[byte & 0xF]);
		}
		return encoded;
	}

	std::string HexDecode(const std::string& text)
	{
		auto hexValue = [](char ch) -> int
		{
			if (ch >= '0' && ch <= '9') return ch - '0';
			if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
			if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
			return -1;
		};
		std::string decoded;
		if (text.size() % 2 != 0) return decoded;
		decoded.reserve(text.size() / 2);
		for (std::size_t i = 0; i < text.size(); i += 2)
		{
			const int hi = hexValue(text[i]);
			const int lo = hexValue(text[i + 1]);
			if (hi < 0 || lo < 0) return {};
			decoded.push_back(static_cast<char>((hi << 4) | lo));
		}
		return decoded;
	}

	std::filesystem::path MakePortableSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath)
	{
		if (!Root::Current().FileSystemRef().Exists(sourcePath)) return sourcePath;
		const std::filesystem::path projectDir = projectPath.parent_path();
		std::error_code ec;
		const std::filesystem::path projectRelative = Root::Current().FileSystemRef().Relative(sourcePath, projectDir, ec);
		if (!ec && !projectRelative.empty() && !IsRelativeToParent(projectRelative)) return projectRelative;
		const std::filesystem::path assetsRelative = Root::Current().FileSystemRef().Relative(sourcePath, AssetsRoot(), ec);
		if (!ec && !assetsRelative.empty() && !IsRelativeToParent(assetsRelative)) return assetsRelative;
		return sourcePath;
	}

	std::filesystem::path ResolveSourcePath(const std::filesystem::path& projectPath, const std::filesystem::path& sourcePath)
	{
		if (Root::Current().FileSystemRef().Exists(sourcePath)) return sourcePath;
		const std::filesystem::path projectDir = projectPath.parent_path();
		const std::filesystem::path projectRelative = projectDir / sourcePath;
		if (Root::Current().FileSystemRef().Exists(projectRelative)) return projectRelative;
		const std::filesystem::path assetsRelative = AssetsRoot() / sourcePath;
		if (Root::Current().FileSystemRef().Exists(assetsRelative)) return assetsRelative;
		const std::filesystem::path searchRoots[] = { ModelsRoot(), TexturesRoot(), ProjectsRoot() };
		for (const auto& searchRoot : searchRoots)
		{
			if (!Root::Current().FileSystemRef().Exists(searchRoot) || !Root::Current().FileSystemRef().IsDirectory(searchRoot)) continue;
			for (const auto& entry : Root::Current().FileSystemRef().ReadDirectoryRecursive(searchRoot))
			{
				if (!entry.is_regular_file()) continue;
				if (entry.path().filename() == sourcePath.filename()) return entry.path();
			}
		}
		return projectRelative;
	}

	void AppendLevelState(std::string& contents, const std::filesystem::path& projectPath, const SceneManager& SceneManager)
	{
		const auto& levels = SceneManager.Levels();
		if (levels.empty())
		{
			contents += "Scene;Default;1\n";
			return;
		}
		for (const auto& Scene : levels)
		{
			if (!Scene) continue;
			contents += "Scene;";
			contents += EscapeField(Scene->Name());
			contents += ";";
			contents += (SceneManager.ActiveLevel() == Scene.get()) ? "1" : "0";
			contents += ";";
			contents += SceneManager.SceneKindFor(Scene->Name()) == SceneManager::SceneKind::Cutscene ? "cutscene" : "Scene";
			contents += ";";
			contents += SceneManager.IsMainMenuScene(Scene->Name()) ? "1" : "0";
			contents += "\n";
			for (const auto& object : Scene->Objects())
			{
				if (!object) continue;
				const glm::vec3 position = object->Position();
				const glm::vec3 rotation = object->Rotation();
				const glm::vec3 scale = object->Scale();
				const std::filesystem::path portableSourcePath = MakePortableSourcePath(projectPath, object->SourcePath());
				contents += "object;";
				contents += EscapeField(portableSourcePath.string());
				contents += ";";
				contents += std::to_string(position.x) + ";" + std::to_string(position.y) + ";" + std::to_string(position.z) + ";";
				contents += std::to_string(rotation.x) + ";" + std::to_string(rotation.y) + ";" + std::to_string(rotation.z) + ";";
				contents += std::to_string(scale.x) + ";" + std::to_string(scale.y) + ";" + std::to_string(scale.z) + ";";
				contents += std::to_string(object->Id()) + ";";
				contents += (object->IgnoreCameraCollision() ? "1;" : "0;");
				contents += (object->ShowPhysicsBoundingBox() ? "1;" : "0;");
				contents += (object->GetPhysicsColliderShape() == PhysicsColliderShape::Capsule ? "1\n" : "0\n");
			}
		}
	}
}



