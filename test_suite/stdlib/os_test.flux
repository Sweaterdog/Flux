# File: os_test.flux
# Tests: OS module from std.os

import std.os;

func main() {
    # Get current working directory
    string cwd = OS.cwd();
    print("CWD: $cwd");

    # Check platform (property, not a function)
    string platform = OS.platform;
    print("Platform: $platform");

    # Get hostname
    string host = OS.hostname();
    print("Hostname: $host");

    # Get PID
    int pid = OS.pid();
    print("PID > 0: ${pid > 0}");

    # Get temp directory
    string tmp = OS.tempDir();
    print("Temp dir: $tmp");

    # Execute a simple command
    string result = OS.exec("echo FluxOS");
    print("Exec result: $result");
}
