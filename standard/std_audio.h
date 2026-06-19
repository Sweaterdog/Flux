#pragma once

#include "../src/value.h"
#include "../src/environment.h"
#include <memory>

// ============================================================================
// std.audio — Audio playback using SDL2_mixer
//
// Provides:
//   Audio.init()                          -> bool  (initialize audio system)
//   Audio.quit()                          -> nil   (shut down audio system)
//   Audio.loadSound(path)                 -> int   (load WAV/OGG, returns sound ID)
//   Audio.loadMusic(path)                 -> int   (load music file, returns music ID)
//   Audio.playSound(id, loops=0)          -> int   (play sound, returns channel)
//   Audio.playMusic(id, loops=-1)         -> nil   (play music, -1 = loop forever)
//   Audio.stopMusic()                     -> nil   (stop currently playing music)
//   Audio.pauseMusic()                    -> nil
//   Audio.resumeMusic()                   -> nil
//   Audio.setSoundVolume(id, vol)         -> nil   (0-128)
//   Audio.setMusicVolume(vol)             -> nil   (0-128)
//   Audio.stopChannel(channel)            -> nil
//   Audio.isPlayingMusic()                -> bool
//   Audio.generateTone(freq, duration_ms) -> int   (generate sine wave, returns sound ID)
//
// All SDL2_mixer functionality is conditionally compiled behind FLUX_HAS_SDL2_MIXER.
// If SDL2_mixer is not available, all functions are stubs that print warnings.
// ============================================================================

class Interpreter;

void registerStdAudio(std::shared_ptr<Environment> env, Interpreter& interp);
