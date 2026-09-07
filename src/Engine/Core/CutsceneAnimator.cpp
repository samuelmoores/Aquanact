#include "Engine/Core/CutsceneAnimator.h"

#include "Engine/Core/Mesh.h"

#include <algorithm>
#include <cctype>

namespace
{
	std::string PortableAnimationPath(std::string value)
	{
		std::replace(value.begin(), value.end(), '\\', '/');
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		const std::size_t assetsMarker = value.find("assets/");
		return assetsMarker == std::string::npos
			? value
			: value.substr(assetsMarker + std::string("assets/").size());
	}

	std::string AnimationFileName(std::string value)
	{
		std::replace(value.begin(), value.end(), '\\', '/');
		const std::size_t separator = value.find_last_of('/');
		value = separator == std::string::npos ? value : value.substr(separator + 1);
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		return value;
	}
}

CutsceneAnimator::CutsceneAnimator(Mesh* mesh)
{
	if (!mesh || !mesh->Skinned())
	{
		return;
	}

	m_animator = std::make_unique<Animator>(mesh->GetRootNode(), mesh->GetSkeletonPtr());
	for (int index = 0; index < mesh->NumAnimations(); ++index)
	{
		m_animator->AddClip(new Animation(mesh->GetAnimation(index)));
		m_animationNames.push_back(mesh->GetAnimationSource(index));
	}
}

int CutsceneAnimator::FindAnimationIndex(const std::string& animationName) const
{
	const std::string requested = PortableAnimationPath(animationName);
	for (std::size_t index = 0; index < m_animationNames.size(); ++index)
	{
		if (m_animationNames[index] == animationName ||
			PortableAnimationPath(m_animationNames[index]) == requested)
		{
			return static_cast<int>(index);
		}
	}

	// Older projects may retain an absolute path from a different project
	// location. The filename is a final compatibility fallback after the
	// exact and assets-relative matches above.
	const std::string requestedFileName = AnimationFileName(animationName);
	for (std::size_t index = 0; index < m_animationNames.size(); ++index)
	{
		if (AnimationFileName(m_animationNames[index]) == requestedFileName)
		{
			return static_cast<int>(index);
		}
	}
	return -1;
}
