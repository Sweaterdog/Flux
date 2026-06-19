# File: sys_test.flux
# Tests: sys module from std.sys

import std.sys;

func main() {
    # Get current time from sys
    float t = sys.time();
    print("sys.time > 0: ${t > 0}");

    # Get platform (property, not a function)
    string platform = sys.platform;
    print("Platform: $platform");

    # Get architecture (property, not a function)
    string arch = sys.arch;
    print("Architecture: $arch");

    # Thread sleep (very short - 1ms)
    thread.sleep(1);
    print("Sleep completed");

    # Environment variable
    string path = sys.env("PATH");
    print("PATH exists: ${path.length > 0}");
}
