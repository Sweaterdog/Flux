// ============================================================================
// std.audio — Audio playback for Flux
//
// Backend: SDL2_mixer (compiled conditionally with -DFLUX_HAS_SDL2_MIXER)
//
// Supports:
//   - WAV, OGG, MP3 sound effects and music
//   - Programmatic tone generation (sine waves)
//   - Volume control per-sound, per-music, and per-channel
//   - Multiple simultaneous sound effects (up to 16 channels)
//
// If SDL2_mixer is not available, all operations are no-ops with warnings.
// ============================================================================

#include "std_audio.h"
#include "../src/interpreter.h"
#include <iostream>
#include <vector>
#include <map>
#include <cmath>

#ifdef FLUX_HAS_SDL2_MIXER
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

// ============================================================================
// Audio manager singleton — manages all loaded sounds and music
// ============================================================================

struct FluxAudioManager {
    bool initialized = false;

#ifdef FLUX_HAS_SDL2_MIXER
    std::map<int, Mix_Chunk*> sounds;
    std::map<int, Mix_Music*> musics;
    int nextSoundId = 1;
    int nextMusicId = 1;
#endif

    bool init() {
#ifdef FLUX_HAS_SDL2_MIXER
        if (initialized) return true;

        // Initialize SDL audio subsystem if not already done
        if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
                std::cerr << "[Flux Audio] SDL audio init failed: " << SDL_GetError() << std::endl;
                return false;
            }
        }

        // Open audio device: 44100 Hz, default format, stereo, 2048 byte chunks
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            std::cerr << "[Flux Audio] Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
            return false;
        }

        // Allocate 16 mixing channels for simultaneous sound effects
        Mix_AllocateChannels(16);
        initialized = true;
        return true;
#else
        std::cerr << "[Flux] Warning: std.audio requires SDL2_mixer (compile with -DFLUX_HAS_SDL2_MIXER)" << std::endl;
        return false;
#endif
    }

    void quit() {
#ifdef FLUX_HAS_SDL2_MIXER
        if (!initialized) return;
        // Free all loaded sounds
        for (auto& [id, chunk] : sounds) {
            Mix_FreeChunk(chunk);
        }
        sounds.clear();
        // Free all loaded music
        for (auto& [id, music] : musics) {
            Mix_FreeMusic(music);
        }
        musics.clear();
        Mix_CloseAudio();
        initialized = false;
#endif
    }

    int loadSound(const std::string& path) {
#ifdef FLUX_HAS_SDL2_MIXER
        if (!initialized) { init(); }
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (!chunk) {
            std::cerr << "[Flux Audio] Failed to load sound '" << path << "': " << Mix_GetError() << std::endl;
            return 0;
        }
        int id = nextSoundId++;
        sounds[id] = chunk;
        return id;
#else
        (void)path;
        return 0;
#endif
    }

    int loadMusic(const std::string& path) {
#ifdef FLUX_HAS_SDL2_MIXER
        if (!initialized) { init(); }
        Mix_Music* music = Mix_LoadMUS(path.c_str());
        if (!music) {
            std::cerr << "[Flux Audio] Failed to load music '" << path << "': " << Mix_GetError() << std::endl;
            return 0;
        }
        int id = nextMusicId++;
        musics[id] = music;
        return id;
#else
        (void)path;
        return 0;
#endif
    }

    int playSound(int id, int loops = 0) {
#ifdef FLUX_HAS_SDL2_MIXER
        auto it = sounds.find(id);
        if (it == sounds.end()) return -1;
        return Mix_PlayChannel(-1, it->second, loops);
#else
        (void)id; (void)loops;
        return -1;
#endif
    }

    void playMusic(int id, int loops = -1) {
#ifdef FLUX_HAS_SDL2_MIXER
        auto it = musics.find(id);
        if (it == musics.end()) return;
        Mix_PlayMusic(it->second, loops);
#else
        (void)id; (void)loops;
#endif
    }

    void stopMusic() {
#ifdef FLUX_HAS_SDL2_MIXER
        Mix_HaltMusic();
#endif
    }

    void pauseMusic() {
#ifdef FLUX_HAS_SDL2_MIXER
        Mix_PauseMusic();
#endif
    }

    void resumeMusic() {
#ifdef FLUX_HAS_SDL2_MIXER
        Mix_ResumeMusic();
#endif
    }

    void setSoundVolume(int id, int vol) {
#ifdef FLUX_HAS_SDL2_MIXER
        auto it = sounds.find(id);
        if (it != sounds.end()) {
            Mix_VolumeChunk(it->second, vol);
        }
#else
        (void)id; (void)vol;
#endif
    }

    void setMusicVolume(int vol) {
#ifdef FLUX_HAS_SDL2_MIXER
        Mix_VolumeMusic(vol);
#else
        (void)vol;
#endif
    }

    void stopChannel(int channel) {
#ifdef FLUX_HAS_SDL2_MIXER
        Mix_HaltChannel(channel);
#else
        (void)channel;
#endif
    }

    bool isPlayingMusic() {
#ifdef FLUX_HAS_SDL2_MIXER
        return Mix_PlayingMusic() != 0;
#else
        return false;
#endif
    }

    // Generate a sine wave tone at the given frequency and duration
    // Returns a sound ID that can be played with playSound()
    int generateTone(float frequency, int durationMs) {
#ifdef FLUX_HAS_SDL2_MIXER
        if (!initialized) { init(); }
        // Generate PCM data: 44100 Hz, 16-bit signed, mono
        int sampleRate = 44100;
        int numSamples = (sampleRate * durationMs) / 1000;
        int dataLen = numSamples * 2; // 16-bit = 2 bytes per sample

        // Allocate buffer: WAV header isn't needed for Mix_QuickLoad_RAW,
        // but we need the raw PCM data
        std::vector<int16_t> samples(numSamples);
        for (int i = 0; i < numSamples; i++) {
            double t = (double)i / (double)sampleRate;
            // Apply a simple envelope (fade in/out) to avoid clicks
            double envelope = 1.0;
            int fadeLen = sampleRate / 50; // 20ms fade
            if (i < fadeLen) envelope = (double)i / fadeLen;
            if (i > numSamples - fadeLen) envelope = (double)(numSamples - i) / fadeLen;
            samples[i] = (int16_t)(sin(2.0 * M_PI * frequency * t) * 16000.0 * envelope);
        }

        // Create SDL audio chunk from raw PCM
        // We need to allocate a persistent buffer since Mix_QuickLoad_RAW doesn't copy
        Uint8* buf = (Uint8*)SDL_malloc(dataLen);
        if (!buf) return 0;
        memcpy(buf, samples.data(), dataLen);

        Mix_Chunk* chunk = Mix_QuickLoad_RAW(buf, dataLen);
        if (!chunk) {
            SDL_free(buf);
            return 0;
        }
        // Mark the chunk so SDL_mixer frees the buffer when the chunk is freed
        chunk->allocated = 1;

        int id = nextSoundId++;
        sounds[id] = chunk;
        return id;
#else
        (void)frequency; (void)durationMs;
        return 0;
#endif
    }

    ~FluxAudioManager() {
        quit();
    }
};

// Global audio manager
static FluxAudioManager g_audioManager;

// ============================================================================
// Register std.audio into the Flux environment
// ============================================================================

void registerStdAudio(std::shared_ptr<Environment> env, Interpreter& interp) {
    (void)interp;

    auto audioObj = std::make_shared<FluxObject>();

    // Audio.init() -> bool
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            return Value::fromBool(g_audioManager.init());
        };
        audioObj->fields["init"] = fn;
    }

    // Audio.quit()
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            g_audioManager.quit();
            return Value::nil();
        };
        audioObj->fields["quit"] = fn;
    }

    // Audio.loadSound(path) -> int
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            std::string path = args.size() > 0 ? args[0].toString() : "";
            return Value::fromInt(g_audioManager.loadSound(path));
        };
        audioObj->fields["loadSound"] = fn;
    }

    // Audio.loadMusic(path) -> int
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            std::string path = args.size() > 0 ? args[0].toString() : "";
            return Value::fromInt(g_audioManager.loadMusic(path));
        };
        audioObj->fields["loadMusic"] = fn;
    }

    // Audio.playSound(id, loops=0) -> channel
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            int id = args.size() > 0 ? (int)args[0].toNumber() : 0;
            int loops = args.size() > 1 ? (int)args[1].toNumber() : 0;
            return Value::fromInt(g_audioManager.playSound(id, loops));
        };
        audioObj->fields["playSound"] = fn;
    }

    // Audio.playMusic(id, loops=-1)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            int id = args.size() > 0 ? (int)args[0].toNumber() : 0;
            int loops = args.size() > 1 ? (int)args[1].toNumber() : -1;
            g_audioManager.playMusic(id, loops);
            return Value::nil();
        };
        audioObj->fields["playMusic"] = fn;
    }

    // Audio.stopMusic()
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            g_audioManager.stopMusic();
            return Value::nil();
        };
        audioObj->fields["stopMusic"] = fn;
    }

    // Audio.pauseMusic()
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            g_audioManager.pauseMusic();
            return Value::nil();
        };
        audioObj->fields["pauseMusic"] = fn;
    }

    // Audio.resumeMusic()
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            g_audioManager.resumeMusic();
            return Value::nil();
        };
        audioObj->fields["resumeMusic"] = fn;
    }

    // Audio.setSoundVolume(id, vol)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            int id = args.size() > 0 ? (int)args[0].toNumber() : 0;
            int vol = args.size() > 1 ? (int)args[1].toNumber() : 128;
            g_audioManager.setSoundVolume(id, vol);
            return Value::nil();
        };
        audioObj->fields["setSoundVolume"] = fn;
    }

    // Audio.setMusicVolume(vol)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            int vol = args.size() > 0 ? (int)args[0].toNumber() : 128;
            g_audioManager.setMusicVolume(vol);
            return Value::nil();
        };
        audioObj->fields["setMusicVolume"] = fn;
    }

    // Audio.stopChannel(channel)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            int ch = args.size() > 0 ? (int)args[0].toNumber() : -1;
            g_audioManager.stopChannel(ch);
            return Value::nil();
        };
        audioObj->fields["stopChannel"] = fn;
    }

    // Audio.isPlayingMusic() -> bool
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
            return Value::fromBool(g_audioManager.isPlayingMusic());
        };
        audioObj->fields["isPlayingMusic"] = fn;
    }

    // Audio.generateTone(frequency, durationMs) -> int
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            float freq = args.size() > 0 ? (float)args[0].toNumber() : 440.0f;
            int dur = args.size() > 1 ? (int)args[1].toNumber() : 200;
            return Value::fromInt(g_audioManager.generateTone(freq, dur));
        };
        audioObj->fields["generateTone"] = fn;
    }

    // Register Audio as a global object
    Value audioVal;
    audioVal.type = ValueType::OBJECT;
    audioVal.objectVal = audioObj;
    env->define("Audio", audioVal, "object");
}
