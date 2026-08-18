#include "Engine/Core/Audio.h"
#include "Engine/Core/Root.h"
#include "Engine/Core/FileSystem.h"
#include <miniaudio.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

struct Audio::State
{
	ma_engine engine{};
	std::unordered_map<std::string, std::unique_ptr<ma_sound>> sounds;
	std::unordered_map<std::string, std::string> soundPaths;
	struct UIVoice
	{
		std::string name;
		std::unique_ptr<ma_sound> sound;
	};
	std::vector<UIVoice> uiVoices;
	size_t nextUIVoice = 0;
	std::unique_ptr<ma_sound> music;
	bool initialized = false;

	~State()
	{
		for (auto& [name, sound] : sounds)
		{
			ma_sound_uninit(sound.get());
		}
		for (auto& voice : uiVoices)
		{
			if (voice.sound)
			{
				ma_sound_uninit(voice.sound.get());
			}
		}
		if (music)
		{
			ma_sound_uninit(music.get());
		}
		if (initialized)
		{
			ma_engine_uninit(&engine);
		}
	}
};

Audio::State* Audio::m_state = nullptr;

namespace
{
	std::string ResolveAudioPath(const std::string& path)
	{
		const std::filesystem::path requested(path);
		if (requested.is_absolute())
			return requested.string();
#ifdef AQUANACT_GAME
		return (Root::Current().FileSystemRef().ExecutableDirectory() / requested).string();
#else
		return path;
#endif
	}

	float ToGain(float volume)
	{
		return std::clamp(volume / 100.0f, 0.0f, 1.0f);
	}
}

void Audio::Init()
{
	if (m_state)
	{
		return;
	}

	auto* state = new State();
	const ma_result result = ma_engine_init(nullptr, &state->engine);
	if (result != MA_SUCCESS)
	{
		std::cerr << "Audio: failed to initialize miniaudio engine (error " << result << ")\n";
		delete state;
		return;
	}

	state->initialized = true;
	const ma_result startResult = ma_engine_start(&state->engine);
	if (startResult != MA_SUCCESS)
	{
		std::cerr << "Audio: failed to start miniaudio engine (error " << startResult << ")\n";
		delete state;
		return;
	}
	m_state = state;
}

void Audio::Shutdown()
{
	delete m_state;
	m_state = nullptr;
}

void Audio::Pause()
{
	if (m_state)
	{
		ma_engine_stop(&m_state->engine);
	}
}

void Audio::Resume()
{
	if (m_state)
	{
		ma_engine_start(&m_state->engine);
	}
}

void Audio::LoadSound(const std::string& name, const std::string& path) {
	if (!m_state)
	{
		std::cerr << "Audio: LoadSound called before initialization\n";
		return;
	}

	// A reload may happen when a GUI preview changes its assigned sound. Release
	// every pooled UI voice for this name so none can play the previous asset.
	for (auto& voice : m_state->uiVoices)
	{
		if (voice.name == name && voice.sound)
		{
			ma_sound_stop(voice.sound.get());
			ma_sound_uninit(voice.sound.get());
			voice.sound.reset();
			voice.name.clear();
		}
	}

	auto existing = m_state->sounds.find(name);
	if (existing != m_state->sounds.end())
	{
		ma_sound_uninit(existing->second.get());
		m_state->sounds.erase(existing);
	}

	auto sound = std::make_unique<ma_sound>();
	const std::string resolvedPath = ResolveAudioPath(path);
	const ma_result result = ma_sound_init_from_file(
		&m_state->engine, resolvedPath.c_str(), 0, nullptr, nullptr, sound.get());
	if (result != MA_SUCCESS)
	{
		std::cerr << "Audio: failed to load sound '" << name << "' from " << path
			          << " (error " << result << ")\n";
		return;
	}

	m_state->sounds.emplace(name, std::move(sound));
	m_state->soundPaths[name] = resolvedPath;
}

bool Audio::IsSoundLoaded(const std::string& name)
{
	return m_state && m_state->sounds.find(name) != m_state->sounds.end();
}

void Audio::PlaySound(const std::string& name, float volume) {
	if (!m_state)
	{
		return;
	}
	auto it = m_state->sounds.find(name);
	if (it == m_state->sounds.end()) {
		std::cerr << "Audio: sound '" << name << "' not loaded\n";
		return;
	}

	ma_sound_set_volume(it->second.get(), ToGain(volume));
	ma_sound_seek_to_pcm_frame(it->second.get(), 0);
	const ma_result result = ma_sound_start(it->second.get());
	if (result != MA_SUCCESS)
	{
		std::cerr << "Audio: failed to play sound '" << name
		          << "' (error " << result << ")\n";
	}
}

void Audio::PlayUISound(const std::string& name, float volume)
{
	if (!m_state)
	{
		return;
	}

	const auto pathIt = m_state->soundPaths.find(name);
	if (pathIt == m_state->soundPaths.end())
	{
		std::cerr << "Audio: UI sound '" << name << "' not loaded\n";
		return;
	}

	constexpr std::size_t poolSize = 4;
	if (m_state->uiVoices.size() < poolSize)
	{
		m_state->uiVoices.push_back({});
	}

	Audio::State::UIVoice* selected = nullptr;
	for (auto& voice : m_state->uiVoices)
	{
		if (voice.sound && voice.name == name && !ma_sound_is_playing(voice.sound.get()))
		{
			selected = &voice;
			break;
		}
	}
	if (!selected)
	{
		for (auto& voice : m_state->uiVoices)
		{
			if (!voice.sound)
			{
				selected = &voice;
				break;
			}
		}
	}
	if (!selected)
	{
		selected = &m_state->uiVoices[m_state->nextUIVoice % m_state->uiVoices.size()];
		m_state->nextUIVoice = (m_state->nextUIVoice + 1) % m_state->uiVoices.size();
		ma_sound_uninit(selected->sound.get());
		selected->sound.reset();
	}

	if (!selected->sound)
	{
		selected->sound = std::make_unique<ma_sound>();
		const ma_result result = ma_sound_init_from_file(
			&m_state->engine, ResolveAudioPath(pathIt->second).c_str(), 0, nullptr, nullptr, selected->sound.get());
		if (result != MA_SUCCESS)
		{
			std::cerr << "Audio: failed to initialize UI sound '" << name
			          << "' (error " << result << ")\n";
			selected->sound.reset();
			return;
		}
	}

	selected->name = name;
	ma_sound_set_volume(selected->sound.get(), ToGain(volume));
	ma_sound_seek_to_pcm_frame(selected->sound.get(), 0);
	ma_sound_start(selected->sound.get());
}

void Audio::StopSound(const std::string& name) {
	if (!m_state)
	{
		return;
	}
	auto it = m_state->sounds.find(name);
	if (it != m_state->sounds.end())
	{
		ma_sound_stop(it->second.get());
	}
}

void Audio::PlayMusic(const std::string& path, bool loop, float volume) {
	if (!m_state)
	{
		return;
	}
	if (m_state->music)
	{
		ma_sound_uninit(m_state->music.get());
		m_state->music.reset();
	}

	auto sound = std::make_unique<ma_sound>();
	const std::string resolvedPath = ResolveAudioPath(path);
	const ma_result result = ma_sound_init_from_file(
		&m_state->engine, resolvedPath.c_str(), 0, nullptr, nullptr, sound.get());
	if (result != MA_SUCCESS)
	{
		std::cerr << "Audio: failed to open music from " << path
			          << " (error " << result << ")\n";
		return;
	}

	m_state->music = std::move(sound);
	ma_sound_set_looping(m_state->music.get(), loop);
	ma_sound_set_volume(m_state->music.get(), ToGain(volume));
	const ma_result startResult = ma_sound_start(m_state->music.get());
	if (startResult != MA_SUCCESS)
	{
		std::cerr << "Audio: failed to start music '" << path
			          << "' (error " << startResult << ")\n";
	}
}

void Audio::StopMusic() {
	if (m_state && m_state->music)
	{
		ma_sound_stop(m_state->music.get());
	}
}

void Audio::SetMusicVolume(float volume) {
	if (m_state && m_state->music)
	{
		ma_sound_set_volume(m_state->music.get(), ToGain(volume));
	}
}

