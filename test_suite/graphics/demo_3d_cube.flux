# 3D First-Person Demo: Textured Cube Exploration
# 
# IMPORTANT: This demo requires Flux to be compiled with GLFW support for 3D rendering.
# If you're using SDL2, the 3D methods are stubs and won't render anything.
# The GLFW backend provides full OpenGL support for 3D via GL immediate mode.
#
# To rebuild Flux with GLFW:
#   1. Ensure GLFW is installed: sudo apt install libglfw3-dev
#   2. The Makefile will auto-detect and enable GLFW if FLUX_HAS_SDL2 isn't defined first
#   3. SDL2 and GLFW backends use the same Window API, so this code works with both
#
# Controls:
#   WASD - Move camera
#   Mouse - Look around
#   E - Pick up / Drop cube
#   Ctrl + Mouse Drag - Rotate picked-up cube
#   ESC - Exit

import std.net;
import std.graphics;
import std.math;
import std.io;

func main() {
    print("===== Flux 3D First-Person Demo =====");
    print("NOTE: For 3D rendering, compile Flux with GLFW.");
    print("      SDL2 backend only provides 2D rendering.");
    print("======================================");
    print("");
    # Download image for cube texture
    string imageUrl = "https://cdn.mos.cms.futurecdn.net/39CUYMP8vJqHAYGVzUghBX-1200-80.jpg";
    string imagePath = "/tmp/flux_cube_texture.jpg";
    
    print("Downloading cube texture...");
    object client = HttpClient();
    bool success = client.download(imageUrl, imagePath);
    
    if (!success) {
        print("Failed to download texture! Using untextured cube.");
    }
    
    print("Creating 3D window...");
    object win = Window("Flux 3D - First Person Cube Explorer", 1280, 720);
    
    # Configure window for 3D
    win.enable3D();
    win.setCursorMode("disabled");  # Lock cursor to window
    
    # Load texture
    int cubeTexture = win.loadTexture(imagePath);
    print("Texture loaded: ID $cubeTexture");
    
    # Camera state
    float camX = 0.0;
    float camY = 1.5;
    float camZ = 5.0;
    float camYaw = 0.0;    # Horizontal rotation
    float camPitch = 0.0;  # Vertical rotation
    
    # Cube state
    float cubeX = 0.0;
    float cubeY = 0.0;
    float cubeZ = 0.0;
    float cubeRotX = 0.0;
    float cubeRotY = 0.0;
    float cubeRotZ = 0.0;
    float cubeSize = 1.0;
    
    # Interaction state
    bool holdingCube = false;
    float holdDistance = 3.0;
    
    # Mouse state
    int centerX = 640;
    int centerY = 360;
    bool firstMouse = true;
    
    # Movement speed
    float moveSpeed = 0.1;
    float lookSpeed = 0.1;
    float rotateSpeed = 2.0;
    
    int frameCount = 0;
    
    while (win.isOpen()) {
        win.pollEvents();
        
        frameCount = frameCount + 1;
        
        # Get mouse movement
        list mousePos = win.getMousePos();
        int mouseX = mousePos[0];
        int mouseY = mousePos[1];
        
        if (firstMouse) {
            firstMouse = false;
        } else {
            # Calculate mouse delta
            float deltaX = (float)(mouseX - centerX) * lookSpeed;
            float deltaY = (float)(mouseY - centerY) * lookSpeed;
            
            bool holding_ctrl = win.keyPressed("CTRL");
            
            if (holding_ctrl && holdingCube) {
                # Rotate cube with mouse when holding Ctrl
                cubeRotY = cubeRotY + deltaX * rotateSpeed;
                cubeRotX = cubeRotX + deltaY * rotateSpeed;
            } else {
                # Normal camera look
                camYaw = camYaw + deltaX;
                camPitch = camPitch - deltaY;
                
                # Clamp pitch to prevent gimbal lock
                if (camPitch > 89.0) {
                    camPitch = 89.0;
                }
                if (camPitch < -89.0) {
                    camPitch = -89.0;
                }
            }
        }
        
        # Reset mouse to center
        win.setMousePos(centerX, centerY);
        
        # Calculate camera forward vector
        float yawRad = camYaw * 3.14159 / 180.0;
        float pitchRad = camPitch * 3.14159 / 180.0;
        float forwardX = math.sin(yawRad) * math.cos(pitchRad);
        float forwardY = math.sin(pitchRad);
        float forwardZ = 0.0 - math.cos(yawRad) * math.cos(pitchRad);
        
        # Calculate right vector (perpendicular to forward)
        float rightX = math.cos(yawRad);
        float rightY = 0.0;
        float rightZ = 0.0 - math.sin(yawRad);
        
        # WASD movement
        if (win.keyPressed("W")) {
            camX = camX + forwardX * moveSpeed;
            camY = camY + forwardY * moveSpeed;
            camZ = camZ + forwardZ * moveSpeed;
        }
        if (win.keyPressed("S")) {
            camX = camX - forwardX * moveSpeed;
            camY = camY - forwardY * moveSpeed;
            camZ = camZ - forwardZ * moveSpeed;
        }
        if (win.keyPressed("A")) {
            camX = camX - rightX * moveSpeed;
            camZ = camZ - rightZ * moveSpeed;
        }
        if (win.keyPressed("D")) {
            camX = camX + rightX * moveSpeed;
            camZ = camZ + rightZ * moveSpeed;
        }
        
        # E to pick up/drop cube
        if (win.keyPressed("E")) {
            if (frameCount % 15 == 0) {  # Debounce
                holdingCube = !holdingCube;
                if (holdingCube) {
                    print("Picked up cube!");
                } else {
                    print("Dropped cube!");
                }
            }
        }
        
        # Update cube position if holding it
        if (holdingCube) {
            cubeX = camX + forwardX * holdDistance;
            cubeY = camY + forwardY * holdDistance;
            cubeZ = camZ + forwardZ * holdDistance;
        }
        
        # Exit on ESC
        if (win.keyPressed("ESC")) {
            win.close();
        }
        
        # Render
        win.clear(20, 30, 50);
        win.clearDepth();
        
        # Set up projection and camera
        win.setPerspective(70.0, 1280.0 / 720.0, 0.1, 100.0);
        
        float lookX = camX + forwardX;
        float lookY = camY + forwardY;
        float lookZ = camZ + forwardZ;
        win.setCamera(camX, camY, camZ, lookX, lookY, lookZ, 0.0, 1.0, 0.0);
        
        # Draw ground plane (grid)
        win.bindTexture(0);
        for (int gridX = -10; gridX <= 10; gridX = gridX + 1) {
            for (int gridZ = -10; gridZ <= 10; gridZ = gridZ + 1) {
                win.pushMatrix();
                win.translate((float)gridX * 2.0, -1.0, (float)gridZ * 2.0);
                win.rotate(90.0, 1.0, 0.0, 0.0);
                win.drawTexturedCube(1.9, 0);  # Floor tiles
                win.popMatrix();
            }
        }
        
        # Draw the textured cube
        win.pushMatrix();
        win.translate(cubeX, cubeY, cubeZ);
        win.rotate(cubeRotX, 1.0, 0.0, 0.0);
        win.rotate(cubeRotY, 0.0, 1.0, 0.0);
        win.rotate(cubeRotZ, 0.0, 0.0, 1.0);
        win.drawTexturedCube(cubeSize, cubeTexture);
        win.popMatrix();
        
        win.present();
    }
    
    win.close();
    print("Demo complete!");
}
