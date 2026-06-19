// ============================================================================
// std.video — Video playback for Flux
//
// Backend: FFmpeg (libavcodec, libavformat, libswscale, libswresample)
//          + SDL2 (for audio output) + OpenGL (for texture upload)
//
// Supports:
//   - MP4, AVI, MKV, WebM, MOV, and other container formats
//   - H.264, H.265, VP8, VP9, AV1 and other codecs
//   - Frame-by-frame decoding to RGB pixel data
//   - OpenGL texture upload for 3D rendering
//   - Audio track playback via SDL2_mixer
//   - Seeking and duration queries
//
// If FFmpeg is not available, all operations are no-ops with warnings.
// ============================================================================

#include "std_video.h"
#include "../src/interpreter.h"
#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <cstring>
#include <mutex>

#ifdef FLUX_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}
#endif

// Use OpenGL if available (for texture upload)
#ifdef FLUX_HAS_GLFW
#include <GL/gl.h>
#endif

// Use SDL2_mixer for audio playback from video
#ifdef FLUX_HAS_SDL2_MIXER
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

// ============================================================================
// FluxVideoPlayer — Manages a single video file's decode state
// ============================================================================

struct FluxVideoPlayer {
    bool opened = false;
    bool finished = false;
    std::string filePath;

    // Video dimensions and timing
    int videoWidth = 0;
    int videoHeight = 0;
    double framerate = 0.0;
    double durationSeconds = 0.0;

    // Current decoded frame pixel data (RGB24)
    std::vector<uint8_t> frameData;
    bool hasFrame = false;

    // OpenGL texture ID for the current frame
    unsigned int glTextureId = 0;
    bool textureAllocated = false;

    // Audio state
    bool audioPlaying = false;
    bool audioPreloaded = false;
    bool audioAutoStarted = false;
    int audioVolume = 128;

#ifdef FLUX_HAS_FFMPEG
    AVFormatContext* fmtCtx = nullptr;
    AVCodecContext* videoCodecCtx = nullptr;
    AVCodecContext* audioCodecCtx = nullptr;
    SwsContext* swsCtx = nullptr;
    SwrContext* swrCtx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgbFrame = nullptr;
    AVFrame* audioFrame = nullptr;
    AVPacket* packet = nullptr;
    int videoStreamIdx = -1;
    int audioStreamIdx = -1;
    uint8_t* rgbBuffer = nullptr;

    // Audio decode buffer
    std::vector<uint8_t> audioBuffer;
    size_t audioBufferPos = 0;
    std::mutex audioMutex;
    Mix_Chunk* audioChunk = nullptr;
    int audioChannel = -1;
#endif

    bool open(const std::string& path) {
#ifdef FLUX_HAS_FFMPEG
        filePath = path;

        // Open input file
        fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, path.c_str(), nullptr, nullptr) < 0) {
            std::cerr << "[Flux Video] Failed to open: " << path << std::endl;
            return false;
        }

        // Read stream info
        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            std::cerr << "[Flux Video] Failed to find stream info" << std::endl;
            avformat_close_input(&fmtCtx);
            return false;
        }

        // Find video stream
        videoStreamIdx = -1;
        audioStreamIdx = -1;
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIdx < 0) {
                videoStreamIdx = (int)i;
            }
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIdx < 0) {
                audioStreamIdx = (int)i;
            }
        }

        if (videoStreamIdx < 0) {
            std::cerr << "[Flux Video] No video stream found in: " << path << std::endl;
            avformat_close_input(&fmtCtx);
            return false;
        }

        // Open video decoder
        AVCodecParameters* videoParams = fmtCtx->streams[videoStreamIdx]->codecpar;
        const AVCodec* videoCodec = avcodec_find_decoder(videoParams->codec_id);
        if (!videoCodec) {
            std::cerr << "[Flux Video] Unsupported video codec" << std::endl;
            avformat_close_input(&fmtCtx);
            return false;
        }

        videoCodecCtx = avcodec_alloc_context3(videoCodec);
        avcodec_parameters_to_context(videoCodecCtx, videoParams);
        if (avcodec_open2(videoCodecCtx, videoCodec, nullptr) < 0) {
            std::cerr << "[Flux Video] Failed to open video codec" << std::endl;
            avcodec_free_context(&videoCodecCtx);
            avformat_close_input(&fmtCtx);
            return false;
        }

        videoWidth = videoCodecCtx->width;
        videoHeight = videoCodecCtx->height;

        // Calculate framerate
        AVRational fr = fmtCtx->streams[videoStreamIdx]->avg_frame_rate;
        if (fr.den > 0 && fr.num > 0) {
            framerate = (double)fr.num / (double)fr.den;
        } else {
            framerate = 30.0; // fallback
        }

        // Duration
        if (fmtCtx->duration > 0) {
            durationSeconds = (double)fmtCtx->duration / AV_TIME_BASE;
        }

        // Set up SWS context for pixel format conversion to RGB24
        swsCtx = sws_getContext(
            videoWidth, videoHeight, videoCodecCtx->pix_fmt,
            videoWidth, videoHeight, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        // Allocate frames
        frame = av_frame_alloc();
        rgbFrame = av_frame_alloc();
        packet = av_packet_alloc();

        // Allocate RGB buffer
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, videoWidth, videoHeight, 1);
        rgbBuffer = (uint8_t*)av_malloc(numBytes);
        av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize,
                            rgbBuffer, AV_PIX_FMT_RGB24,
                            videoWidth, videoHeight, 1);

        // Allocate frame data for external access
        frameData.resize(videoWidth * videoHeight * 3);

        // Open audio decoder if audio stream exists
        if (audioStreamIdx >= 0) {
            AVCodecParameters* audioParams = fmtCtx->streams[audioStreamIdx]->codecpar;
            const AVCodec* audioCodec = avcodec_find_decoder(audioParams->codec_id);
            if (audioCodec) {
                audioCodecCtx = avcodec_alloc_context3(audioCodec);
                avcodec_parameters_to_context(audioCodecCtx, audioParams);
                if (avcodec_open2(audioCodecCtx, audioCodec, nullptr) == 0) {
                    audioFrame = av_frame_alloc();

                    // Set up resampler to convert to S16 stereo 44100Hz
                    swrCtx = swr_alloc();
                    if (swrCtx) {
                        AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
                        AVChannelLayout inLayout;
                        if (audioCodecCtx->ch_layout.nb_channels > 0) {
                            inLayout = audioCodecCtx->ch_layout;
                        } else {
                            inLayout = AV_CHANNEL_LAYOUT_STEREO;
                        }
                        swr_alloc_set_opts2(&swrCtx,
                            &outLayout, AV_SAMPLE_FMT_S16, 44100,
                            &inLayout, audioCodecCtx->sample_fmt, audioCodecCtx->sample_rate,
                            0, nullptr);
                        swr_init(swrCtx);
                    }
                } else {
                    avcodec_free_context(&audioCodecCtx);
                    audioCodecCtx = nullptr;
                    audioStreamIdx = -1;
                }
            } else {
                audioStreamIdx = -1;
            }
        }

        opened = true;
        finished = false;

        // Pre-load all audio from the file so playAudio() works immediately
        preloadAudio();

        return true;
#else
        (void)path;
        std::cerr << "[Flux Video] FFmpeg not available — video playback disabled" << std::endl;
        return false;
#endif
    }

    // Scan entire file for audio packets, decode them all, then seek back to 0.
    // This pre-buffers the complete audio track so playAudio() can play it in sync.
    void preloadAudio() {
#ifdef FLUX_HAS_FFMPEG
        if (audioPreloaded || audioStreamIdx < 0 || !audioCodecCtx) return;

        AVPacket* audioPkt = av_packet_alloc();
        while (av_read_frame(fmtCtx, audioPkt) >= 0) {
            if (audioPkt->stream_index == audioStreamIdx) {
                // Decode audio packet
                int ret = avcodec_send_packet(audioCodecCtx, audioPkt);
                if (ret >= 0) {
                    while (true) {
                        ret = avcodec_receive_frame(audioCodecCtx, audioFrame);
                        if (ret < 0) break;
                        int outSamples = swr_get_out_samples(swrCtx, audioFrame->nb_samples);
                        if (outSamples <= 0) continue;
                        std::vector<uint8_t> tempBuf(outSamples * 2 * 2);
                        uint8_t* outBufs[1] = { tempBuf.data() };
                        int converted = swr_convert(swrCtx, outBufs, outSamples,
                            (const uint8_t**)audioFrame->data, audioFrame->nb_samples);
                        if (converted > 0) {
                            int bytes = converted * 2 * 2;
                            std::lock_guard<std::mutex> lock(audioMutex);
                            audioBuffer.insert(audioBuffer.end(), tempBuf.data(), tempBuf.data() + bytes);
                        }
                    }
                }
            }
            av_packet_unref(audioPkt);
        }
        av_packet_free(&audioPkt);

        // Seek back to beginning for video playback
        av_seek_frame(fmtCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
        if (videoCodecCtx) avcodec_flush_buffers(videoCodecCtx);
        if (audioCodecCtx) avcodec_flush_buffers(audioCodecCtx);
        finished = false;

        audioPreloaded = true;
#endif
    }

    bool nextFrame() {
#ifdef FLUX_HAS_FFMPEG
        if (!opened || finished) return false;

        while (true) {
            int ret = av_read_frame(fmtCtx, packet);
            if (ret < 0) {
                finished = true;
                hasFrame = false;
                return false;
            }

            if (packet->stream_index == videoStreamIdx) {
                // Send packet to video decoder
                ret = avcodec_send_packet(videoCodecCtx, packet);
                av_packet_unref(packet);
                if (ret < 0) continue;

                ret = avcodec_receive_frame(videoCodecCtx, frame);
                if (ret < 0) continue;

                // Convert to RGB24
                sws_scale(swsCtx,
                         frame->data, frame->linesize, 0, videoHeight,
                         rgbFrame->data, rgbFrame->linesize);

                // Copy to our frameData buffer (row by row because linesize may differ)
                for (int y = 0; y < videoHeight; y++) {
                    memcpy(frameData.data() + y * videoWidth * 3,
                           rgbFrame->data[0] + y * rgbFrame->linesize[0],
                           videoWidth * 3);
                }

                hasFrame = true;

                // Auto-start audio on first video frame
                if (!audioAutoStarted && audioPreloaded && !audioBuffer.empty()) {
                    playAudio();
                    audioAutoStarted = true;
                }

                return true;
            } else if (packet->stream_index == audioStreamIdx && audioCodecCtx) {
                // Skip audio packets during playback (already pre-loaded)
                av_packet_unref(packet);
                continue; // keep reading until we get a video frame
            } else {
                av_packet_unref(packet);
                continue;
            }
        }
#else
        return false;
#endif
    }

#ifdef FLUX_HAS_FFMPEG
    void decodeAudioPacket() {
        if (!audioCodecCtx || !swrCtx) return;

        int ret = avcodec_send_packet(audioCodecCtx, packet);
        if (ret < 0) return;

        while (true) {
            ret = avcodec_receive_frame(audioCodecCtx, audioFrame);
            if (ret < 0) break;

            // Resample to S16 stereo 44100Hz
            int outSamples = swr_get_out_samples(swrCtx, audioFrame->nb_samples);
            if (outSamples <= 0) continue;

            std::vector<uint8_t> tempBuf(outSamples * 2 * 2); // 2 channels, 2 bytes per sample (S16)
            uint8_t* outBufs[1] = { tempBuf.data() };

            int converted = swr_convert(swrCtx,
                outBufs, outSamples,
                (const uint8_t**)audioFrame->data, audioFrame->nb_samples);

            if (converted > 0) {
                int bytes = converted * 2 * 2; // 2 channels * 2 bytes
                std::lock_guard<std::mutex> lock(audioMutex);
                audioBuffer.insert(audioBuffer.end(), tempBuf.data(), tempBuf.data() + bytes);
            }
        }
    }
#endif

    int getTextureId() {
#if defined(FLUX_HAS_FFMPEG) && defined(FLUX_HAS_GLFW)
        if (!hasFrame || videoWidth <= 0 || videoHeight <= 0) return 0;

        // RGB24: 3 bytes per pixel — must set alignment to 1
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        if (!textureAllocated) {
            glGenTextures(1, &glTextureId);
            glBindTexture(GL_TEXTURE_2D, glTextureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, videoWidth, videoHeight,
                        0, GL_RGB, GL_UNSIGNED_BYTE, frameData.data());
            textureAllocated = true;
        } else {
            glBindTexture(GL_TEXTURE_2D, glTextureId);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth, videoHeight,
                           GL_RGB, GL_UNSIGNED_BYTE, frameData.data());
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore default

        return (int)glTextureId;
#else
        return 0;
#endif
    }

    void playAudio() {
#if defined(FLUX_HAS_FFMPEG) && defined(FLUX_HAS_SDL2_MIXER)
        if (audioStreamIdx < 0) {
            std::cerr << "[Flux Video] No audio stream in video" << std::endl;
            return;
        }

        if (audioBuffer.empty()) {
            std::cerr << "[Flux Video] No audio data buffered yet." << std::endl;
            return;
        }

        // Ensure SDL2 audio subsystem and mixer are initialized
        if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
                std::cerr << "[Flux Video] SDL audio init failed: " << SDL_GetError() << std::endl;
                return;
            }
        }
        // Open mixer if not already open (check by attempting to query)
        {
            int freq = 0, channels = 0;
            Uint16 fmt = 0;
            if (Mix_QuerySpec(&freq, &fmt, &channels) == 0) {
                // Not yet opened
                if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
                    std::cerr << "[Flux Video] Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
                    return;
                }
            }
        }

        // Create a WAV in memory from the buffered audio
        std::lock_guard<std::mutex> lock(audioMutex);
        int dataSize = (int)audioBuffer.size();
        int wavSize = 44 + dataSize;
        std::vector<uint8_t> wav(wavSize);

        auto w16 = [](uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; };
        auto w32 = [](uint8_t* p, uint32_t v) {
            p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
            p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
        };

        memcpy(wav.data(), "RIFF", 4); w32(wav.data() + 4, wavSize - 8);
        memcpy(wav.data() + 8, "WAVE", 4);
        memcpy(wav.data() + 12, "fmt ", 4); w32(wav.data() + 16, 16);
        w16(wav.data() + 20, 1);      // PCM
        w16(wav.data() + 22, 2);      // Stereo
        w32(wav.data() + 24, 44100);  // Sample rate
        w32(wav.data() + 28, 44100 * 2 * 2); // Byte rate
        w16(wav.data() + 32, 4);      // Block align
        w16(wav.data() + 34, 16);     // Bits per sample
        memcpy(wav.data() + 36, "data", 4);
        w32(wav.data() + 40, dataSize);
        memcpy(wav.data() + 44, audioBuffer.data(), dataSize);

        // Load as SDL2_mixer chunk
        if (audioChunk) {
            Mix_FreeChunk(audioChunk);
            audioChunk = nullptr;
        }

        SDL_RWops* rw = SDL_RWFromMem(wav.data(), wavSize);
        audioChunk = Mix_LoadWAV_RW(rw, 1);

        if (audioChunk) {
            Mix_VolumeChunk(audioChunk, audioVolume);
            audioChannel = Mix_PlayChannel(-1, audioChunk, 0);
            audioPlaying = true;
        } else {
            std::cerr << "[Flux Video] Failed to create audio chunk: " << Mix_GetError() << std::endl;
        }
#else
        std::cerr << "[Flux Video] Audio playback requires FFmpeg + SDL2_mixer" << std::endl;
#endif
    }

    void stopAudio() {
#ifdef FLUX_HAS_SDL2_MIXER
        if (audioChannel >= 0) {
            Mix_HaltChannel(audioChannel);
            audioChannel = -1;
        }
        audioPlaying = false;
#endif
    }

    void setAudioVolume(int vol) {
        audioVolume = vol;
#ifdef FLUX_HAS_SDL2_MIXER
        if (audioChunk) Mix_VolumeChunk(audioChunk, vol);
#endif
    }

    void seek(double seconds) {
#ifdef FLUX_HAS_FFMPEG
        if (!opened) return;
        int64_t ts = (int64_t)(seconds * AV_TIME_BASE);
        av_seek_frame(fmtCtx, -1, ts, AVSEEK_FLAG_BACKWARD);
        if (videoCodecCtx) avcodec_flush_buffers(videoCodecCtx);
        if (audioCodecCtx) avcodec_flush_buffers(audioCodecCtx);
        finished = false;
        hasFrame = false;
#else
        (void)seconds;
#endif
    }

    void restart() {
        seek(0.0);
    }

    void close() {
#ifdef FLUX_HAS_FFMPEG
        stopAudio();

#ifdef FLUX_HAS_SDL2_MIXER
        if (audioChunk) { Mix_FreeChunk(audioChunk); audioChunk = nullptr; }
#endif

#ifdef FLUX_HAS_GLFW
        if (textureAllocated && glTextureId > 0) {
            glDeleteTextures(1, &glTextureId);
            glTextureId = 0;
            textureAllocated = false;
        }
#endif

        if (rgbBuffer) { av_free(rgbBuffer); rgbBuffer = nullptr; }
        if (frame) { av_frame_free(&frame); }
        if (rgbFrame) { av_frame_free(&rgbFrame); }
        if (audioFrame) { av_frame_free(&audioFrame); }
        if (packet) { av_packet_free(&packet); }
        if (swsCtx) { sws_freeContext(swsCtx); swsCtx = nullptr; }
        if (swrCtx) { swr_free(&swrCtx); swrCtx = nullptr; }
        if (videoCodecCtx) { avcodec_free_context(&videoCodecCtx); }
        if (audioCodecCtx) { avcodec_free_context(&audioCodecCtx); }
        if (fmtCtx) { avformat_close_input(&fmtCtx); }

        opened = false;
        finished = true;
        hasFrame = false;
        audioBuffer.clear();
#endif
    }

    ~FluxVideoPlayer() {
        close();
    }
};

// Global video player storage (by ID)
static std::map<int, std::shared_ptr<FluxVideoPlayer>> g_videoPlayers;
static int g_nextVideoId = 1;

// ============================================================================
// Register std.video into the Flux environment
// ============================================================================

void registerStdVideo(std::shared_ptr<Environment> env, Interpreter& interp) {
    (void)interp;

    // Video(path) -> object constructor
    Value videoCtor;
    videoCtor.type = ValueType::NATIVE_FUNCTION;
    videoCtor.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
        if (args.empty()) {
            std::cerr << "[Flux Video] Video() requires a file path argument" << std::endl;
            return Value::nil();
        }
        std::string path = args[0].toString();

        auto player = std::make_shared<FluxVideoPlayer>();
        bool ok = player->open(path);
        if (!ok) {
            std::cerr << "[Flux Video] Failed to open: " << path << std::endl;
            return Value::nil();
        }

        int id = g_nextVideoId++;
        g_videoPlayers[id] = player;

        // Build a FluxObject with methods that reference this player
        auto vidObj = std::make_shared<FluxObject>();

        // Store the player ID so methods can look it up
        vidObj->fields["_id"] = Value::fromInt(id);

        // .isOpen() -> bool
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromBool(false);
                return Value::fromBool(it->second->opened);
            };
            vidObj->fields["isOpen"] = fn;
        }

        // .width() -> int
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromInt(0);
                return Value::fromInt(it->second->videoWidth);
            };
            vidObj->fields["width"] = fn;
        }

        // .height() -> int
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromInt(0);
                return Value::fromInt(it->second->videoHeight);
            };
            vidObj->fields["height"] = fn;
        }

        // .fps() -> float
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromFloat(0.0);
                return Value::fromFloat(it->second->framerate);
            };
            vidObj->fields["fps"] = fn;
        }

        // .duration() -> float
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromFloat(0.0);
                return Value::fromFloat(it->second->durationSeconds);
            };
            vidObj->fields["duration"] = fn;
        }

        // .nextFrame() -> bool
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromBool(false);
                return Value::fromBool(it->second->nextFrame());
            };
            vidObj->fields["nextFrame"] = fn;
        }

        // .getTextureId() -> int
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromInt(0);
                return Value::fromInt(it->second->getTextureId());
            };
            vidObj->fields["getTextureId"] = fn;
        }

        // .seek(seconds)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value> args) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::nil();
                double sec = args.size() > 0 ? args[0].toNumber() : 0.0;
                it->second->seek(sec);
                return Value::nil();
            };
            vidObj->fields["seek"] = fn;
        }

        // .restart()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::nil();
                it->second->restart();
                return Value::nil();
            };
            vidObj->fields["restart"] = fn;
        }

        // .isFinished() -> bool
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::fromBool(true);
                return Value::fromBool(it->second->finished);
            };
            vidObj->fields["isFinished"] = fn;
        }

        // .close()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it != g_videoPlayers.end()) {
                    it->second->close();
                    g_videoPlayers.erase(it);
                }
                return Value::nil();
            };
            vidObj->fields["close"] = fn;
        }

        // .playAudio()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it != g_videoPlayers.end()) it->second->playAudio();
                return Value::nil();
            };
            vidObj->fields["playAudio"] = fn;
        }

        // .stopAudio()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value>) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it != g_videoPlayers.end()) it->second->stopAudio();
                return Value::nil();
            };
            vidObj->fields["stopAudio"] = fn;
        }

        // .setAudioVolume(vol)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [id](Interpreter&, std::vector<Value> args) -> Value {
                auto it = g_videoPlayers.find(id);
                if (it == g_videoPlayers.end()) return Value::nil();
                int vol = args.size() > 0 ? (int)args[0].toNumber() : 128;
                it->second->setAudioVolume(vol);
                return Value::nil();
            };
            vidObj->fields["setAudioVolume"] = fn;
        }

        // Return the video object
        Value result;
        result.type = ValueType::OBJECT;
        result.objectVal = vidObj;
        return result;
    };

    env->define("Video", videoCtor, "native_function");
}
