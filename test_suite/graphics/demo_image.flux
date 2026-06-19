# Demo: Download and display an image from the internet
# Showcases HttpClient.download() and Window.drawImage()

import std.net;
import std.graphics;
import std.io;

func main() {
    string imageUrl = "https://cdn.mos.cms.futurecdn.net/39CUYMP8vJqHAYGVzUghBX-1200-80.jpg";
    string imagePath = "/tmp/flux_demo_image.jpg";
    
    print("Downloading image...");
    
    # Download the image (HttpClient() is a constructor function in JIT, new HttpClient() in AOT)
    object client = HttpClient();
    bool success = client.download(imageUrl, imagePath);
    
    if (!success) {
        print("Failed to download image!");
        return;
    }
    
    print("Download complete! Opening window...");
    
    # Get image dimensions
    object win = Window("Flux Image Demo", 1280, 720);
    list imgSize = win.getImageSize(imagePath);
    int imgW = imgSize[0];
    int imgH = imgSize[1];
    
    print("Image size: ${imgW}x${imgH}");
    
    # Resize window to fit image (or scale image to fit window)
    # Let's keep window at 1280x720 and center the scaled image
    
    while (win.isOpen()) {
        win.pollEvents();
        
        # Clear to dark gray
        win.clear(40, 40, 40);
        
        # Draw the image scaled to fit the window with padding
        int padding = 20;
        int targetW = 1240;  # 1280 - 2*padding
        int targetH = 680;   # 720 - 2*padding
        
        # Maintain aspect ratio
        float aspect = (float)imgW / (float)imgH;
        float targetAspect = (float)targetW / (float)targetH;
        
        int drawW;
        int drawH;
        
        if (aspect > targetAspect) {
            # Image is wider, fit to width
            drawW = targetW;
            drawH = (int)((float)targetW / aspect);
        } else {
            # Image is taller, fit to height
            drawH = targetH;
            drawW = (int)((float)targetH * aspect);
        }
        
        # Center the image
        int x = (1280 - drawW) / 2;
        int y = (720 - drawH) / 2;
        
        win.drawImageScaled(imagePath, x, y, drawW, drawH);
        
        # Draw a border around the image
        win.drawRect(x - 2, y - 2, drawW + 4, drawH + 4, 255, 255, 255);
        
        # Draw title text at top
        win.drawText("Flux Image Display Demo", 20, 20, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24, 255, 255, 255, 255);
        
        win.present();
    }
    
    win.close();
    print("Demo complete!");
}
