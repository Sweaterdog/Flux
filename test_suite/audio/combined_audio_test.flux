# Combined audio test: file loading + procedural tone generation
# Shows both loadSound(path) and generateTone(freq, duration)

import std.audio;
import std.io;

func main() {
    print("===== Audio Test: Files + Procedural Tones =====");
    print("");
    
    Audio.init();
    
    # Generate procedural tones
    print("Generating procedural tones...");
    int beep = Audio.generateTone(880, 100);    # High beep
    int boop = Audio.generateTone(440, 150);    # Mid boop  
    int buzz = Audio.generateTone(220, 200);    # Low buzz
    print("Generated 3 tones (beep, boop, buzz)");
    print("");
    
    # Try to load WAV files if they exist
    # (These are just examples - create .wav files to test)
    print("Attempting to load WAV files...");
    int explosion = Audio.loadSound("test_suite/audio/explosion.wav");
    int laser = Audio.loadSound("test_suite/audio/laser.wav");
    
    if (explosion >= 0) {
        print("Loaded explosion.wav (ID: $explosion)");
    } else {
        print("explosion.wav not found (that's ok, using tones instead)");
    }
    
    if (laser >= 0) {
        print("Loaded laser.wav (ID: $laser)");
    } else {
        print("laser.wav not found (that's ok, using tones instead)");
    }
    print("");
    
    # Play sequence
    print("Playing sound sequence...");
    print("1. Beep");
    Audio.playSound(beep, 0);
    
    int i = 0;
    while (i < 5000000) { i = i + 1; }
    
    print("2. Boop");
    Audio.playSound(boop, 0);
    
    i = 0;
    while (i < 5000000) { i = i + 1; }
    
    print("3. Buzz");
    Audio.playSound(buzz, 0);
    
    i = 0;
    while (i < 7000000) { i = i + 1; }
    
    # Play loaded sounds if available
    if (explosion >= 0) {
        print("4. Explosion!");
        Audio.playSound(explosion, 0);
        i = 0;
        while (i < 10000000) { i = i + 1; }
    }
    
    if (laser >= 0) {
        print("5. Laser!");
        Audio.playSound(laser, 0);
        i = 0;
        while (i < 10000000) { i = i + 1; }
    }
    
    print("");
    print("Audio test complete!");
    Audio.quit();
}
