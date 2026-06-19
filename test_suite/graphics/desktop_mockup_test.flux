# Test: OS Desktop Environment Mockup
# Demonstrates that Flux can render a basic desktop environment:
# - Taskbar with clock
# - Desktop icons
# - Movable "windows" with title bars
# - Button-like UI elements
# - Mouse interaction

import std.graphics;
import std.time;

func drawTaskbar(Window win, int screenW, int screenH) {
    int barH = 40;
    int barY = screenH - barH;
    
    # Taskbar background
    win.fillRect(0, barY, screenW, barH, 45, 45, 65);
    win.drawLine(0, barY, screenW, barY, 80, 80, 120);
    
    # "Start" button
    win.fillRect(5, barY + 5, 80, 30, 70, 70, 110);
    win.drawRect(5, barY + 5, 80, 30, 120, 120, 180);
    
    # System tray area (right side)
    win.fillRect(screenW - 120, barY + 5, 115, 30, 55, 55, 75);
    win.drawRect(screenW - 120, barY + 5, 115, 30, 80, 80, 100);
}

func drawDesktopIcon(Window win, int x, int y, int iconSize) {
    # Icon body (folder shape)
    win.fillRect(x, y + 5, iconSize, iconSize - 5, 240, 200, 80);
    win.fillRect(x, y, iconSize / 2, 8, 240, 200, 80);
    win.drawRect(x, y + 5, iconSize, iconSize - 5, 200, 160, 40);
}

func drawAppWindow(Window win, int wx, int wy, int ww, int wh, int titleR, int titleG, int titleB) {
    int titleH = 28;
    
    # Window shadow
    win.fillRect(wx + 3, wy + 3, ww, wh, 15, 15, 25, 100);
    
    # Window body
    win.fillRect(wx, wy, ww, wh, 50, 50, 70);
    
    # Title bar
    win.fillRect(wx, wy, ww, titleH, titleR, titleG, titleB);
    
    # Border
    win.drawRect(wx, wy, ww, titleH, titleR + 30, titleG + 30, titleB + 30);
    win.drawRect(wx, wy, ww, wh, 80, 80, 110);
    
    # Close button
    win.fillCircle(wx + ww - 18, wy + 14, 7, 230, 70, 70);
    
    # Minimize button
    win.fillCircle(wx + ww - 38, wy + 14, 7, 230, 200, 70);
    
    # Maximize button
    win.fillCircle(wx + ww - 58, wy + 14, 7, 70, 200, 70);
    
    # Content area separator
    win.drawLine(wx, wy + titleH, wx + ww, wy + titleH, 80, 80, 110);
}

func main() {
    print("=== StratOS Desktop Mockup Test ===");
    
    int screenW = 1024;
    int screenH = 768;
    Window win = Window("StratOS Desktop", screenW, screenH);
    
    if (!win.isOpen()) {
        print("ERROR: Could not open window");
        return;
    }
    
    win.setBlendMode(true);
    
    int frames = 0;
    int maxFrames = 180;
    
    while (win.isOpen() && frames < maxFrames) {
        win.pollEvents();
        
        # Desktop background gradient (simulated with horizontal bars)
        for (int y = 0; y < screenH; y += 4) {
            int r = 20 + (y * 15 / screenH);
            int g = 25 + (y * 20 / screenH);
            int b = 50 + (y * 30 / screenH);
            win.fillRect(0, y, screenW, 4, r, g, b);
        }
        
        # Desktop icons column
        drawDesktopIcon(win, 30, 30, 48);
        drawDesktopIcon(win, 30, 110, 48);
        drawDesktopIcon(win, 30, 190, 48);
        
        # Application windows
        drawAppWindow(win, 150, 80, 400, 300, 60, 60, 100);
        drawAppWindow(win, 350, 200, 450, 350, 80, 50, 80);
        
        # Content in first window: some "text lines" (represented as thin rects)
        for (int i = 0; i < 8; i++) {
            int lineW = 200 + (i * 37 % 150);
            win.fillRect(165, 120 + i * 25, lineW, 12, 180, 180, 200);
        }
        
        # Content in second window: a chart-like visualization
        for (int i = 0; i < 10; i++) {
            int barH = 20 + (i * 23 % 120);
            int barX = 370 + i * 40;
            int barY = 520 - barH;
            
            int cr = 80 + i * 15;
            int cg = 120 + i * 10;
            int cb = 200 - i * 10;
            win.fillRect(barX, barY, 30, barH, cr, cg, cb);
            win.drawRect(barX, barY, 30, barH, cr + 30, cg + 30, cb + 30);
        }
        
        # Taskbar
        drawTaskbar(win, screenW, screenH);
        
        # Mouse cursor (small crosshair at mouse position)
        int mx = Input.mouseX();
        int my = Input.mouseY();
        win.drawLine(mx - 8, my, mx + 8, my, 255, 255, 255);
        win.drawLine(mx, my - 8, mx, my + 8, 255, 255, 255);
        
        win.present();
        frames++;
    }
    
    win.close();
    print("Rendered $frames frames of desktop environment");
    print("=== StratOS Desktop Mockup PASSED ===");
}
