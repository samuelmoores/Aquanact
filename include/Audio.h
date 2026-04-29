#pragma once
#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>

class Audio {
public:
    static void LoadSound(const std::string& name, const std::string& path);
    static void PlaySound(const std::string& name, float volume = 100.f);
    static void StopSound(const std::string& name);

    static void PlayMusic(const std::string& path, bool loop = true, float volume = 50.f);
    static void StopMusic();
    static void SetMusicVolume(float volume);

private:
    static std::unordered_map<std::string, sf::SoundBuffer> m_buffers;
    static std::unordered_map<std::string, sf::Sound> m_sounds;
    static sf::Music m_music;
};
