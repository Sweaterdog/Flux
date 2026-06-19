# Test: 2D Drawing Primitives
# Tests all drawing functions: pixel, line, rect, circle, triangle

import std.graphics;

func main() {
    print("=== 2D Drawing Test ===");
    
    Window win = Window("Flux 2D Drawing Test", 800, 600);
    print("Backend: ${win.backend}");
    
    if (!win.isOpen()) {
        print("ERROR: Could not open window");
        return;
    }
    
    # Enable alpha blending
    win.setBlendMode(true);
    
    int frames = 0;
    int maxFrames = 120;
    
    while (win.isOpen() && frames < maxFrames) {
        win.pollEvents();
        
        # Clear to dark background
        win.clear(30, 30, 50);
        
        # Draw pixels - a dotted pattern
        for (int i = 0; i < 20; i++) {
            win.drawPixel(50 + i * 5, 50, 255, 255, 0);
        }
        
        # Draw lines - a star pattern
        win.drawLine(200, 50, 250, 150, 0, 255, 0);
        win.drawLine(250, 150, 150, 100, 0, 255, 0);
        win.drawLine(150, 100, 250, 100, 0, 255, 0);
        win.drawLine(250, 100, 200, 150, 0, 255, 0);
        win.drawLine(200, 150, 200, 50, 0, 255, 0);
        
        # Draw outlined rectangle
        win.drawRect(350, 50, 120, 80, 255, 0, 0);
        
        # Draw filled rectangle
        win.fillRect(350, 160, 120, 80, 0, 100, 255);
        
        # Draw semi-transparent overlay
        win.fillRect(380, 100, 80, 100, 255, 255, 0, 128);
        
        # Draw outlined circle
        win.drawCircle(600, 100, 60, 255, 128, 0);
        
        # Draw filled circle
        win.fillCircle(600, 250, 50, 128, 0, 255);
        
        # Draw triangle
        win.drawTriangle(100, 300, 200, 200, 300, 300, 255, 0, 255);
        
        # Draw a "button" - filled rect with outline
        win.fillRect(100, 400, 200, 50, 60, 60, 100);
        win.drawRect(100, 400, 200, 50, 150, 150, 200);
        
        win.present();
        frames++;
    }
    
    win.close();
    print("Rendered ${frames} frames with 2D primitives");
    print("=== 2D Drawing Test PASSED ===");
}
