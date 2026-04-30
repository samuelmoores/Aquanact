#include <Audio.h>
#include <iostream>

#ifndef __EMSCRIPTEN__

std::unordered_map<std::string, sf::SoundBuffer> Audio::m_buffers;
std::unordered_map<std::string, sf::Sound> Audio::m_sounds;
sf::Music Audio::m_music;

void Audio::LoadSound(const std::string& name, const std::string& path) {
    auto& buf = m_buffers[name];
    if (!buf.loadFromFile(path)) {
        std::cerr << "Audio: failed to load sound '" << name << "' from " << path << "\n";
        return;
    }

    for (auto& [n, sound] : m_sounds)
        sound.setBuffer(m_buffers[n]);

    m_sounds[name].setBuffer(m_buffers[name]);
}

void Audio::PlaySound(const std::string& name, float volume) {
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) {
        std::cerr << "Audio: sound '" << name << "' not loaded\n";
        return;
    }
    it->second.setVolume(volume);
    it->second.play();
}

void Audio::StopSound(const std::string& name) {
    auto it = m_sounds.find(name);
    if (it != m_sounds.end())
        it->second.stop();
}

void Audio::PlayMusic(const std::string& path, bool loop, float volume) {
    if (!m_music.openFromFile(path)) {
        std::cerr << "Audio: failed to open music from " << path << "\n";
        return;
    }
    m_music.setLoop(loop);
    m_music.setVolume(volume);
    m_music.play();
}

void Audio::StopMusic() {
    m_music.stop();
}

void Audio::SetMusicVolume(float volume) {
    m_music.setVolume(volume);
}

#else

void Audio::LoadSound(const std::string&, const std::string&) {}
void Audio::PlaySound(const std::string&, float) {}
void Audio::StopSound(const std::string&) {}
void Audio::PlayMusic(const std::string&, bool, float) {}
void Audio::StopMusic() {}
void Audio::SetMusicVolume(float) {}

#endif
