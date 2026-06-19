# Test window aspect ratio snapping and resolution getters
import std.graphics;

func main() {
    # Create a window
    object win = Window("Aspect Ratio Test", 800, 600);
    
    print("Initial size: ${toString(win.getWidth())} x ${toString(win.getHeight())}");
    
    # Enable 16:9 aspect ratio snapping
    win.snapAspectRatio(16.0 / 9.0);
    
    print("After snapping to 16:9: ${toString(win.getWidth())} x ${toString(win.getHeight())}");
    print("Try resizing the window - it will maintain 16:9 aspect ratio!");
    print("Press ESC to exit.");
    
    while (win.isOpen()) {
        win.pollEvents();
        if (win.keyPressed("ESC")) { break; }
        
        # Display current resolution
        win.clear(30, 30, 50);
        var w = win.getWidth();
        var h = win.getHeight();
        var aspectRatio = (float) w / (float) h;
        
        # Draw info text
        win.drawText("Width: ${toString(w)}", 10, 10, "sans", 16, 255, 255, 255);
        win.drawText("Height: ${toString(h)}", 10, 30, "sans", 16, 255, 255, 255);
        win.drawText("Aspect: ${toString(aspectRatio)}", 10, 50, "sans", 16, 255, 255, 0);
        
        win.present();
    }
    
    win.close();
    print("Test complete.");
}
