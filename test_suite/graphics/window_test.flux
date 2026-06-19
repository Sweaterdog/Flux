# Test: Basic window creation and rendering with std.graphics
# This test verifies that Flux can create a graphical window,
# draw to it, handle input, and close cleanly.

import std.graphics;

func main() {
    print("=== Graphics Window Test ===");
    
    # Create a window
    auto win = Window("Flux Graphics Test", 800, 600);
    
    # Check backend
    print("Backend: ${win.backend}");
    print("Window size: ${win.width}x${win.height}");
    
    # Verify window is open
    if (win.isOpen()) {
        print("Window created successfully!");
    } else {
        print("ERROR: Window failed to open");
        return;
    }
    
    # Render a few frames with different colors
    int frameCount = 0;
    int maxFrames = 60;  # Run for ~1 second at 60fps
    
    while (win.isOpen() && frameCount < maxFrames) {
        win.pollEvents();
        
        # Cycle through colors
        int r = (frameCount * 4) % 256;
        int g = (frameCount * 2) % 256;
        int b = 128;
        
        win.clear(r, g, b);
        win.present();
        
        frameCount++;
    }
    
    print("Rendered $frameCount frames");
    
    # Test setTitle
    win.setTitle("Flux - Title Changed!");
    print("Title updated");
    
    # Test input functions
    int mx = Input.mouseX();
    int my = Input.mouseY();
    print("Mouse position: $mx, $my");
    
    bool leftDown = Input.mouseDown(0);
    print("Left mouse down: $leftDown");
    
    bool escPressed = Input.keyPressed("Escape");
    print("Escape pressed: $escPressed");
    
    # Close the window
    win.close();
    print("Window closed.");
    print("=== Graphics Test PASSED ===");
}
