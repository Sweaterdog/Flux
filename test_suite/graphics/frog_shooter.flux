# Frog Shooter - 3D Target Practice Game
# 
# Shoot the moving frog targets with gravity and sound!
# 
# Controls:
#   Mouse - Aim
#   Left Click - Shoot
#   WASD - Move camera
#   SPACE - Jump
#   ESC - Exit
#
# Score points by hitting the frogs. Miss and lose points!

import std.net;
import std.graphics;
import std.math;
import std.io;
import std.audio;

func main() {
    print("===== FROG SHOOTER =====");
    print("Shoot the moving frogs!");
    print("========================");
    print("");
    
    # Initialize audio
    Audio.init();
    
    # Generate sound effects
    int shootSound = Audio.generateTone(880, 80);
    int hitSound = Audio.generateTone(523, 200);
    int missSound = Audio.generateTone(220, 300);
    int bounceSound = Audio.generateTone(330, 60);
    
    # Download frog texture
    string imageUrl = "https://cdn.mos.cms.futurecdn.net/39CUYMP8vJqHAYGVzUghBX-1200-80.jpg";
    string imagePath = "/tmp/flux_frog_texture.jpg";
    
    print("Downloading frog texture...");
    object client = HttpClient();
    bool success = client.download(imageUrl, imagePath);
    
    if (!success) {
        print("Failed to download texture!");
        return;
    }
    
    print("Creating game window...");
    object win = Window("Frog Shooter", 1280, 720);
    
    # Configure for 3D
    win.enable3D();
    win.setCursorMode("disabled");
    
    # Load texture
    int frogTexture = win.loadTexture(imagePath);
    print("Texture loaded!");
    print("");
    print("READY! Click to shoot the frogs!");
    print("");
    
    # Camera state
    float camX = 0.0;
    float camY = 2.0;
    float camZ = 8.0;
    float camYaw = 0.0;
    float camPitch = 0.0;
    float camVY = 0.0;
    bool onGround = true;
    
    # Mouse state
    int centerX = 640;
    int centerY = 360;
    bool firstMouse = true;
    
    # Movement/look speed
    float moveSpeed = 0.08;
    float lookSpeed = 0.15;
    float gravity = -0.006;
    float jumpForce = 0.12;
    float groundY = 2.0;
    
    # Game state
    int score = 0;
    int hits = 0;
    int misses = 0;
    int frameCount = 0;
    
    # Frog targets - parallel arrays (avoids nested lists for AOT)
    int maxFrogs = 8;
    list<float> frogX = [];
    list<float> frogY = [];
    list<float> frogZ = [];
    list<float> frogVX = [];
    list<float> frogVY = [];
    list<float> frogVZ = [];
    list<float> frogRotY = [];
    list<int> frogAlive = [];
    
    # Initialize frogs
    for (int i = 0; i < maxFrogs; i = i + 1) {
        float rx = (float)((i % 4) - 2) * 4.0 + math.sin((float)i) * 2.0;
        float ry = math.cos((float)i * 0.7) * 2.0 + 2.0;
        float rz = (float)((i / 4) - 1) * 6.0 - 10.0;
        float vx = (math.sin((float)i * 1.3) - 0.5) * 0.03;
        float vy = 0.0;
        float vz = (math.sin((float)i * 1.7) - 0.5) * 0.03;
        frogX.add(rx);
        frogY.add(ry);
        frogZ.add(rz);
        frogVX.add(vx);
        frogVY.add(vy);
        frogVZ.add(vz);
        frogRotY.add(0.0);
        frogAlive.add(1);
    }
    
    # Shooting state
    bool wasMouseDown = false;
    float bulletX = 0.0;
    float bulletY = 0.0;
    float bulletZ = 0.0;
    float bulletVX = 0.0;
    float bulletVY = 0.0;
    float bulletVZ = 0.0;
    bool bulletActive = false;
    int bulletLife = 0;
    
    while (win.isOpen()) {
        win.pollEvents();
        frameCount = frameCount + 1;
        
        # Mouse look
        list mousePos = win.getMousePos();
        int mouseX = mousePos[0];
        int mouseY = mousePos[1];
        
        if (firstMouse) {
            firstMouse = false;
        } else {
            float deltaX = (float)(mouseX - centerX) * lookSpeed;
            float deltaY = (float)(mouseY - centerY) * lookSpeed;
            
            camYaw = camYaw + deltaX;
            camPitch = camPitch - deltaY;
            
            # Clamp pitch
            if (camPitch > 89.0) {
                camPitch = 89.0;
            }
            if (camPitch < -89.0) {
                camPitch = -89.0;
            }
        }
        
        win.setMousePos(centerX, centerY);
        
        # Calculate camera vectors
        float yawRad = camYaw * 3.14159 / 180.0;
        float pitchRad = camPitch * 3.14159 / 180.0;
        float forwardX = math.sin(yawRad) * math.cos(pitchRad);
        float forwardY = math.sin(pitchRad);
        float forwardZ = 0.0 - math.cos(yawRad) * math.cos(pitchRad);
        
        float rightX = math.cos(yawRad);
        float rightZ = 0.0 - math.sin(yawRad);
        
        # WASD movement (horizontal only)
        if (win.keyPressed("W")) {
            camX = camX + math.sin(yawRad) * moveSpeed;
            camZ = camZ - math.cos(yawRad) * moveSpeed;
        }
        if (win.keyPressed("S")) {
            camX = camX - math.sin(yawRad) * moveSpeed;
            camZ = camZ + math.cos(yawRad) * moveSpeed;
        }
        if (win.keyPressed("D")) {
            camX = camX - rightX * moveSpeed;
            camZ = camZ - rightZ * moveSpeed;
        }
        if (win.keyPressed("A")) {
            camX = camX + rightX * moveSpeed;
            camZ = camZ + rightZ * moveSpeed;
        }
        
        # Jumping
        if (win.keyPressed("SPACE") && onGround) {
            camVY = jumpForce;
            onGround = false;
        }
        
        # Apply gravity to camera
        camVY = camVY + gravity;
        camY = camY + camVY;
        if (camY <= groundY) {
            camY = groundY;
            camVY = 0.0;
            onGround = true;
        }
        
        # Shooting
        bool mouseDown = win.mouseButtonPressed(0);
        if (mouseDown && !wasMouseDown && !bulletActive) {
            # Fire bullet
            bulletX = camX;
            bulletY = camY;
            bulletZ = camZ;
            bulletVX = forwardX * 0.5;
            bulletVY = forwardY * 0.5;
            bulletVZ = forwardZ * 0.5;
            bulletActive = true;
            bulletLife = 0;
            Audio.playSound(shootSound, 0);
        }
        wasMouseDown = mouseDown;
        
        # Update bullet (with gravity)
        if (bulletActive) {
            bulletX = bulletX + bulletVX;
            bulletY = bulletY + bulletVY;
            bulletZ = bulletZ + bulletVZ;
            bulletVY = bulletVY + gravity * 0.5;
            bulletLife = bulletLife + 1;
            
            # Check bullet collisions with frogs
            bool hit = false;
            for (int i = 0; i < maxFrogs; i = i + 1) {
                if (frogAlive[i] == 1) {
                    float dx = bulletX - frogX[i];
                    float dy = bulletY - frogY[i];
                    float dz = bulletZ - frogZ[i];
                    float dist = math.sqrt(dx*dx + dy*dy + dz*dz);
                    
                    if (dist < 0.8) {
                        # Hit!
                        frogAlive[i] = 0;
                        hit = true;
                        hits = hits + 1;
                        score = score + 10;
                        bulletActive = false;
                        Audio.playSound(hitSound, 0);
                        break;
                    }
                }
            }
            
            # Bullet timeout, hit ground, or miss
            if (bulletLife > 120 && !hit) {
                bulletActive = false;
                misses = misses + 1;
                score = score - 2;
                if (score < 0) {
                    score = 0;
                }
                Audio.playSound(missSound, 0);
            }
            if (bulletY < 0.0 && !hit) {
                bulletActive = false;
                misses = misses + 1;
                score = score - 2;
                if (score < 0) {
                    score = 0;
                }
            }
        }
        
        # Update frogs (with gravity)
        for (int i = 0; i < maxFrogs; i = i + 1) {
            if (frogAlive[i] == 1) {
                # Apply gravity
                frogVY[i] = frogVY[i] + gravity;
                
                # Move frog
                frogX[i] = frogX[i] + frogVX[i];
                frogY[i] = frogY[i] + frogVY[i];
                frogZ[i] = frogZ[i] + frogVZ[i];
                frogRotY[i] = frogRotY[i] + 1.0;
                
                # Ground collision
                if (frogY[i] < 0.6) {
                    frogY[i] = 0.6;
                    frogVY[i] = math.abs(frogVY[i]) * 0.7;
                    if (frogVY[i] < 0.01) {
                        # Give a random hop
                        frogVY[i] = 0.04 + math.abs(math.sin((float)frameCount * 0.1 + (float)i)) * 0.06;
                    }
                    Audio.playSound(bounceSound, 0);
                }
                
                # Bounce off side boundaries
                if (frogX[i] > 15.0 || frogX[i] < -15.0) {
                    frogVX[i] = 0.0 - frogVX[i];
                }
                if (frogZ[i] > -3.0 || frogZ[i] < -25.0) {
                    frogVZ[i] = 0.0 - frogVZ[i];
                }
            } else {
                # Respawn after delay
                if (frameCount % 180 == i * 20) {
                    frogX[i] = (float)((i % 4) - 2) * 4.0 + math.sin((float)frameCount * 0.01) * 2.0;
                    frogY[i] = 3.0 + math.cos((float)frameCount * 0.007) * 2.0;
                    frogZ[i] = (float)((i / 4) - 1) * 6.0 - 10.0;
                    frogVX[i] = (math.sin((float)frameCount * 0.013) - 0.5) * 0.03;
                    frogVY[i] = 0.0;
                    frogVZ[i] = (math.sin((float)frameCount * 0.017) - 0.5) * 0.03;
                    frogRotY[i] = 0.0;
                    frogAlive[i] = 1;
                }
            }
        }
        
        # ESC to exit
        if (win.keyPressed("ESC")) {
            win.close();
        }
        
        # Render
        win.clear(10, 15, 30);
        win.clearDepth();
        
        win.setPerspective(70.0, 1280.0 / 720.0, 0.1, 100.0);
        
        float lookX = camX + forwardX;
        float lookY = camY + forwardY;
        float lookZ = camZ + forwardZ;
        win.setCamera(camX, camY, camZ, lookX, lookY, lookZ, 0.0, 1.0, 0.0);
        
        # Draw procedural ground (checkerboard pattern)
        win.bindTexture(0);
        for (int gx = -10; gx < 10; gx = gx + 1) {
            for (int gz = -15; gz < 5; gz = gz + 1) {
                # Checkerboard color
                int checker = (gx + gz) % 2;
                if (checker < 0) {
                    checker = 0 - checker;
                }
                if (checker == 0) {
                    win.setColor(0.15, 0.55, 0.15, 1.0);
                } else {
                    win.setColor(0.1, 0.4, 0.1, 1.0);
                }
                float qx = (float)gx * 2.0;
                float qz = (float)gz * 2.0;
                win.drawQuad(
                    qx, 0.0, qz,
                    qx + 2.0, 0.0, qz,
                    qx + 2.0, 0.0, qz + 2.0,
                    qx, 0.0, qz + 2.0
                );
            }
        }
        # Reset color to white for textured objects
        win.setColor(1.0, 1.0, 1.0, 1.0);
        
        # Draw frogs
        for (int i = 0; i < maxFrogs; i = i + 1) {
            if (frogAlive[i] == 1) {
                win.pushMatrix();
                win.translate(frogX[i], frogY[i], frogZ[i]);
                win.rotate(frogRotY[i], 0.0, 1.0, 0.0);
                win.drawTexturedCube(1.0, frogTexture);
                win.popMatrix();
            }
        }
        
        # Draw bullet
        if (bulletActive) {
            win.setColor(1.0, 1.0, 0.0, 1.0);
            win.bindTexture(0);
            win.pushMatrix();
            win.translate(bulletX, bulletY, bulletZ);
            win.drawTexturedCube(0.15, 0);
            win.popMatrix();
            win.setColor(1.0, 1.0, 1.0, 1.0);
        }
        
        # Draw crosshair
        win.setColor(1.0, 0.2, 0.2, 1.0);
        win.bindTexture(0);
        float crosshairDist = 0.5;
        float chX = camX + forwardX * crosshairDist;
        float chY = camY + forwardY * crosshairDist;
        float chZ = camZ + forwardZ * crosshairDist;
        
        # Horizontal line
        win.pushMatrix();
        win.translate(chX, chY, chZ);
        win.scale(0.04, 0.002, 0.002);
        win.drawTexturedCube(1.0, 0);
        win.popMatrix();
        
        # Vertical line
        win.pushMatrix();
        win.translate(chX, chY, chZ);
        win.scale(0.002, 0.04, 0.002);
        win.drawTexturedCube(1.0, 0);
        win.popMatrix();
        
        win.setColor(1.0, 1.0, 1.0, 1.0);
        
        win.present();
    }
    
    Audio.quit();
    win.close();
    print("");
    print("===== GAME OVER =====");
    print("Final Score: $score");
    print("Hits: $hits");
    print("Misses: $misses");
    if (hits > 0) {
        float accuracy = ((float)hits / (float)(hits + misses)) * 100.0;
        print("Accuracy: $accuracy%");
    }
    print("=====================");
}
