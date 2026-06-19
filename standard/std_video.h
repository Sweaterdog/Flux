#pragma once

#include "../src/value.h"
#include "../src/environment.h"
#include <memory>

// ============================================================================
// std.video — Video playback using FFmpeg + SDL2/OpenGL
//
// Decodes video files (MP4, AVI, MKV, WebM, etc.) frame-by-frame using
// FFmpeg (libavcodec/libavformat/libswscale). Frames are converted to
// RGB24 pixel data, which can be:
//   - Uploaded as an OpenGL texture for 3D rendering on quads/cubes
//   - Rendered to an SDL2 texture for 2D window display
//
// Audio from video files is decoded and played via SDL2_mixer when available.
//
// Provides:
//   Video(path)                 -> object  (open a video file)
//   .isOpen() -> bool           (true if video was opened successfully)
//   .width() -> int             (video width in pixels)
//   .height() -> int            (video height in pixels)
//   .fps() -> float             (framerate)
//   .duration() -> float        (duration in seconds)
//   .nextFrame() -> bool        (decode next frame, returns false at EOF)
//   .getTextureId() -> int      (upload current frame as OpenGL texture, returns ID)
//   .seek(float seconds)        (seek to a position in seconds)
//   .restart()                  (seek to beginning)
//   .isFinished() -> bool       (true if reached end of video)
//   .close()                    (close video and free resources)
//   .playAudio()                (start audio track playback)
//   .stopAudio()                (stop audio track playback)
//   .setAudioVolume(int vol)    (set audio volume, 0-128)
//
// Conditionally compiled behind FLUX_HAS_FFMPEG.
// If FFmpeg is not available, all operations print warnings and return defaults.
// ============================================================================

class Interpreter;

void registerStdVideo(std::shared_ptr<Environment> env, Interpreter& interp);
