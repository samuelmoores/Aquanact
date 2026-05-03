#include <Audio.h>
#include <iostream>

#ifndef __EMSCRIPTEN__

std::unordered_map<std::string, sf::SoundBuffer> Audio::m_buffers;
std::unordered_map<std::string, sf::Sound> Audio::m_sounds;
sf::Music Audio::m_music;

void Audio::Init() {}
void Audio::Resume() {}

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

#include <emscripten.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE void Audio_Resume();
}

bool Audio::m_audioUnlocked = false;

void Audio::Init() {
    EM_ASM({
        if (Module.__aquanactAudio) return;

        function mimeFor(path) {
            var lower = path.toLowerCase();
            if (lower.endsWith(".mp3")) return "audio/mpeg";
            if (lower.endsWith(".wav")) return "audio/wav";
            return "application/octet-stream";
        }

        function fileUrl(path) {
            var bytes = FS.readFile(path);
            var blob = new Blob([bytes], { type: mimeFor(path) });
            return URL.createObjectURL(blob);
        }

        Module.__aquanactAudio = {};
        Module.__aquanactAudio.unlocked = false;
        Module.__aquanactAudio.sounds = Object.create(null);
        Module.__aquanactAudio.music = null;
        Module.__aquanactAudio.pendingMusic = null;
        Module.__aquanactAudio.fileUrl = fileUrl;
    });
}

void Audio::Resume() {
    if (m_audioUnlocked) return;
    Init();

    EM_ASM({
        var audio = Module.__aquanactAudio;
        audio.unlocked = true;

        if (audio.pendingMusic && !audio.music) {
            var pending = audio.pendingMusic;
            audio.pendingMusic = null;
            var elem = new Audio(audio.fileUrl(pending.path));
            elem.loop = pending.loop;
            elem.volume = pending.volume;
            elem.play().catch(function() {});
            audio.music = elem;
        }
    });

    m_audioUnlocked = true;
}

void Audio::LoadSound(const std::string& name, const std::string& path) {
    Init();
    EM_ASM({
        var name = UTF8ToString($0);
        var path = UTF8ToString($1);
        var audio = Module.__aquanactAudio;
        var url = audio.fileUrl(path);
        var elem = new Audio(url);
        elem.preload = "auto";
        audio.sounds[name] = {};
        audio.sounds[name].url = url;
        audio.sounds[name].elem = elem;
    }, name.c_str(), path.c_str());
}

void Audio::PlaySound(const std::string& name, float volume) {
    Init();
    EM_ASM({
        var name = UTF8ToString($0);
        var volume = Math.max(0, Math.min(1, $1 / 100.0));
        var audio = Module.__aquanactAudio;
        var sound = audio.sounds[name];
        if (!sound || !audio.unlocked) return;

        var elem = sound.elem.paused ? sound.elem : new Audio(sound.url);
        elem.currentTime = 0;
        elem.volume = volume;
        elem.play().catch(function() {});
    }, name.c_str(), volume);
}

void Audio::StopSound(const std::string& name) {
    Init();
    EM_ASM({
        var name = UTF8ToString($0);
        var sound = Module.__aquanactAudio.sounds[name];
        if (!sound) return;
        sound.elem.pause();
        sound.elem.currentTime = 0;
    }, name.c_str());
}

void Audio::PlayMusic(const std::string& path, bool loop, float volume) {
    Init();
    EM_ASM({
        var path = UTF8ToString($0);
        var loop = !!$1;
        var volume = Math.max(0, Math.min(1, $2 / 100.0));
        var audio = Module.__aquanactAudio;

        if (audio.music) {
            audio.music.pause();
            audio.music = null;
        }

        if (!audio.unlocked) {
            audio.pendingMusic = {};
            audio.pendingMusic.path = path;
            audio.pendingMusic.loop = loop;
            audio.pendingMusic.volume = volume;
            return;
        }

        var elem = new Audio(audio.fileUrl(path));
        elem.loop = loop;
        elem.volume = volume;
        elem.play().catch(function() {});
        audio.music = elem;
    }, path.c_str(), loop ? 1 : 0, volume);
}

void Audio::StopMusic() {
    Init();
    EM_ASM({
        var audio = Module.__aquanactAudio;
        audio.pendingMusic = null;
        if (audio.music) {
            audio.music.pause();
            audio.music.currentTime = 0;
            audio.music = null;
        }
    });
}

void Audio::SetMusicVolume(float volume) {
    Init();
    EM_ASM({
        var audio = Module.__aquanactAudio;
        var volume = Math.max(0, Math.min(1, $0 / 100.0));
        if (audio.music)
            audio.music.volume = volume;
        if (audio.pendingMusic)
            audio.pendingMusic.volume = volume;
    }, volume);
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE void Audio_Resume() {
        Audio::Resume();
    }
}

#endif
