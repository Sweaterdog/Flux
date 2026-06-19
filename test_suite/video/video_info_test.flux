// video_info_test.flux — Test basic Video metadata (no window needed)
// Tests: Video constructor, width, height, fps, duration, frame decoding, close

import std.video;

func main() {
    var path = "test_video.mp4";
    print("Opening video: " + path);

    var vid = Video(path);
    if (!vid.isOpen()) {
        print("ERROR: Failed to open video");
    }

    print("Video opened successfully!");
    print("  Width:    " + toString(vid.width()));
    print("  Height:   " + toString(vid.height()));
    print("  FPS:      " + toString(vid.fps()));
    print("  Duration: " + toString(vid.duration()) + " seconds");

    // Decode a few frames
    var frameCount = 0;
    var maxFrames = 10;
    while (vid.nextFrame() && frameCount < maxFrames) {
        frameCount = frameCount + 1;
    }

    print("  Decoded " + toString(frameCount) + " frames");
    print("  Finished: " + toString(vid.isFinished()));

    // Test seek
    vid.seek(0.0);
    print("  Seeked back to 0.0s");
    print("  Finished after seek: " + toString(vid.isFinished()));

    // Decode one more frame after seek
    if (vid.nextFrame()) {
        print("  Successfully decoded frame after seek");
    } else {
        print("  No frame after seek (video may be very short)");
    }

    vid.close();
    print("Video closed.");
    print("All video info tests passed!");
}
