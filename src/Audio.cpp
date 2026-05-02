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

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <emscripten.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE void Audio_Resume();
}

ma_engine Audio::m_engine;
bool Audio::m_initialized = false;
bool Audio::m_audioUnlocked = false;
std::unordered_map<std::string, std::unique_ptr<ma_sound>> Audio::m_ma_sounds;
std::unique_ptr<ma_sound> Audio::m_ma_music;
std::string Audio::m_pending_music_path;
bool Audio::m_pending_music_loop = true;
float Audio::m_pending_music_volume = 50.f;
std::vector<std::pair<std::string, std::string>> Audio::m_pending_sounds;

void Audio::Init() {
    // Intentionally deferred — the miniaudio engine creates a ScriptProcessorNode
    // immediately on init, and its onaudioprocess fires even while AudioContext is
    // suspended, causing buffer-undefined crashes. Engine init is done in Resume()
    // after the user gesture unlocks the AudioContext.
}

void Audio::Resume() {
    if (m_audioUnlocked) return;

    // Initialize the engine here, after a user gesture, so the ScriptProcessorNode
    // is only created once the AudioContext is ready to run.
    if (!m_initialized) {
        ma_result result = ma_engine_init(NULL, &m_engine);
        if (result != MA_SUCCESS) {
            std::cerr << "Audio: Failed to initialize miniaudio engine (" << result << ")\n";
            return;
        }
        m_initialized = true;
    }

    std::cout << "Audio: Resuming audio engine..." << std::endl;

    EM_ASM({
        if (typeof window !== 'undefined' && window.miniaudio) {
            if (typeof window.miniaudio.unlock === 'function') {
                window.miniaudio.unlock();
            } else if (window.miniaudio.devices) {
                for (var i = 0; i < window.miniaudio.devices.length; i++) {
                    var d = window.miniaudio.devices[i];
                    if (d && d.webaudio && d.webaudio.context)
                        d.webaudio.context.resume();
                }
            }
        }
    });

    ma_engine_start(&m_engine);
    m_audioUnlocked = true;

    // Load any sounds that were queued before the engine was ready.
    for (auto& [name, path] : m_pending_sounds)
        LoadSound(name, path);
    m_pending_sounds.clear();

    // audioContext.resume() is async; defer music start slightly so the context
    // is actually running before ma_sound_start() fires.
    if (!m_pending_music_path.empty() && !m_ma_music) {
        emscripten_async_call([](void*) {
            std::cout << "Audio: Playing pending music: " << m_pending_music_path << std::endl;
            PlayMusic(m_pending_music_path, m_pending_music_loop, m_pending_music_volume);
        }, nullptr, 100);
    }
}

void Audio::LoadSound(const std::string& name, const std::string& path) {
    if (!m_initialized) {
        m_pending_sounds.push_back({name, path});
        return;
    }

    auto sound = std::make_unique<ma_sound>();
    ma_result result = ma_sound_init_from_file(&m_engine, path.c_str(), 0, NULL, NULL, sound.get());
    if (result != MA_SUCCESS) {
        std::cerr << "Audio: Failed to load sound '" << name << "' from " << path << " (" << result << ")\n";
        return;
    }
    m_ma_sounds[name] = std::move(sound);
}

void Audio::PlaySound(const std::string& name, float volume) {
    auto it = m_ma_sounds.find(name);
    if (it == m_ma_sounds.end()) {
        std::cerr << "Audio: sound '" << name << "' not loaded\n";
        return;
    }
    ma_sound_set_volume(it->second.get(), volume / 100.0f);
    ma_sound_seek_to_pcm_frame(it->second.get(), 0);
    ma_sound_start(it->second.get());
}

void Audio::StopSound(const std::string& name) {
    auto it = m_ma_sounds.find(name);
    if (it != m_ma_sounds.end())
        ma_sound_stop(it->second.get());
}

void Audio::PlayMusic(const std::string& path, bool loop, float volume) {
    m_pending_music_path = path;
    m_pending_music_loop = loop;
    m_pending_music_volume = volume;

    if (!m_initialized) Init();
    if (!m_initialized) return;

    // Defer actual playback until the browser's AudioContext is unlocked by a user gesture
    if (!m_audioUnlocked) {
        std::cout << "Audio: Music deferred until unlocked: " << path << std::endl;
        return;
    }

    if (m_ma_music) {
        ma_sound_stop(m_ma_music.get());
        ma_sound_uninit(m_ma_music.get());
    }

    m_ma_music = std::make_unique<ma_sound>();
    // USE MA_SOUND_FLAG_STREAM instead of MA_SOUND_FLAG_DECODE to avoid freezing the main thread!
    ma_result result = ma_sound_init_from_file(&m_engine, path.c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, m_ma_music.get());
    if (result != MA_SUCCESS) {
        std::cerr << "Audio: Failed to open music from " << path << " (" << result << ")\n";
        m_ma_music.reset();
        return;
    }

    ma_sound_set_looping(m_ma_music.get(), loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(m_ma_music.get(), volume / 100.0f);
    ma_sound_start(m_ma_music.get());
    std::cout << "Audio: Music started: " << path << std::endl;
}

void Audio::StopMusic() {
    m_pending_music_path.clear();
    if (m_ma_music)
        ma_sound_stop(m_ma_music.get());
}

void Audio::SetMusicVolume(float volume) {
    if (m_ma_music)
        ma_sound_set_volume(m_ma_music.get(), volume / 100.0f);
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE void Audio_Resume() {
        Audio::Resume();
    }
}

#endif
