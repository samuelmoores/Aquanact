#pragma once
#include <string>

class Audio {
public:
    static void Init();
    static void Shutdown();
    static void Pause();
    static void Resume();

    static void LoadSound(const std::string& name, const std::string& path);
    static bool IsSoundLoaded(const std::string& name);
    static void PlaySound(const std::string& name, float volume = 100.f);
    static void PlayUISound(const std::string& name, float volume = 100.f);
    static void StopSound(const std::string& name);

    static void PlayMusic(const std::string& path, bool loop = true, float volume = 50.f);
    static void StopMusic();
    static void SetMusicVolume(float volume);

private:
    struct State;
    static State* m_state;
};

