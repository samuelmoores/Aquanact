#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#ifndef __EMSCRIPTEN__
#include <SFML/Audio.hpp>
#else
#include <miniaudio.h>
#include <memory>
#endif

class Audio {
public:
    static void Init();
    static void Resume();

    static void LoadSound(const std::string& name, const std::string& path);
    static void PlaySound(const std::string& name, float volume = 100.f);
    static void StopSound(const std::string& name);

    static void PlayMusic(const std::string& path, bool loop = true, float volume = 50.f);
    static void StopMusic();
    static void SetMusicVolume(float volume);

private:
#ifndef __EMSCRIPTEN__
    static std::unordered_map<std::string, sf::SoundBuffer> m_buffers;
    static std::unordered_map<std::string, sf::Sound> m_sounds;
    static sf::Music m_music;
#else
    static ma_engine m_engine;
    static bool m_initialized;
    static std::unordered_map<std::string, std::unique_ptr<ma_sound>> m_ma_sounds;
    static std::unique_ptr<ma_sound> m_ma_music;
    static std::string m_pending_music_path;
    static bool m_pending_music_loop;
    static float m_pending_music_volume;
    static bool m_audioUnlocked;
    static std::vector<std::pair<std::string, std::string>> m_pending_sounds;
#endif
};
