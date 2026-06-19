# video_3d_playback.flux — Play a video on a 3D quad in an OpenGL window
# Renders video frames as an OpenGL texture on a flat quad in 3D space.
# Press ESC to close.

import std.graphics;
import std.video;
import std.time;

func main() {
    var path = "test_video2.mp4";
    print("Opening video: $path");

    var vid = Video(path);
    if (!vid.isOpen()) {
        print("ERROR: Failed to open video");
        return;
    }

    var vw = vid.width();
    var vh = vid.height();
    print("Video: ${toString(vw)}x${toString(vh)} @ ${toString(vid.fps())} fps");

    # Create a 3D window
    object win = Window("Video Player", 800, 600);
    win.enable3D();

    # Set up orthographic-like view so the quad fills the screen
    # Use identity matrices (default) — quad goes from -1 to 1 which is NDC

    # Timing for frame pacing (milliseconds)
    var frameIntervalMs = 1000.0 / vid.fps();
    var lastFrameMs = (float) Time.nowMs();

    # Decode the first frame BEFORE entering the loop
    vid.nextFrame();

    print("Playing video... Press ESC to stop.");

    while (win.isOpen()) {
        win.pollEvents();
        if (win.keyPressed("ESC")) { break; }

        var nowMs = (float) Time.nowMs();

        # Decode next frame at the right pace
        if (nowMs - lastFrameMs >= frameIntervalMs) {
            if (vid.nextFrame()) {
                lastFrameMs = nowMs;
            } else {
                print("Video playback complete.");
                break;
            }
        }

        win.clear(0, 0, 0);
        win.clearDepth();

        # Get the OpenGL texture from the current frame
        var texId = vid.getTextureId();
        if (texId > 0) {
            win.bindTexture(texId);

            win.drawQuad(
                -1.0,  1.0, 0.0,
                 1.0,  1.0, 0.0,
                 1.0, -1.0, 0.0,
                -1.0, -1.0, 0.0
            );
        }

        win.present();
    }

    vid.close();
    win.close();
    print("Done.");
}
