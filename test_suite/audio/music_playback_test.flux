# Test loading and playing MP3 music files
# This demonstrates Audio.loadMusic() and playback control

import std.audio;
import std.io;

func main() {
    print("===== Audio File Playback Test =====");
    print("");
    
    # Initialize audio
    bool ok = Audio.init();
    if (!ok) {
        print("ERROR: Failed to initialize audio subsystem");
        return;
    }
    print("Audio system initialized!");
    print("");
    
    # Load the MP3 file
    string musicPath = "choose_joy.mp3";
    print("Loading music: $musicPath");
    int musicId = Audio.loadMusic(musicPath);
    
    if (musicId < 0) {
        print("ERROR: Failed to load music file");
        Audio.quit();
        return;
    }
    
    print("Music loaded successfully! ID: $musicId");
    print("");
    
    # Set volume to 80% and play
    Audio.setMusicVolume(102);  # 80% of 128
    print("Playing music (looping forever)...");
    print("Press Ctrl+C to stop");
    print("");
    
    Audio.playMusic(musicId, -1);  # -1 = loop forever
    
    # Keep program running so music plays
    # In a real game, this would be your main loop
    int counter = 0;
    while (counter < 100) {
        if (Audio.isPlayingMusic()) {
            if (counter % 20 == 0) {
                print("Music still playing... ($counter)");
            }
        } else {
            print("Music stopped");
            break;
        }
        
        # Busy wait (in a real game you'd render frames here)
        int j = 0;
        while (j < 10000000) {
            j = j + 1;
        }
        
        counter = counter + 1;
    }
    
    print("");
    print("Stopping music...");
    Audio.stopMusic();
    
    print("Cleaning up...");
    Audio.quit();
    print("Done!");
}
