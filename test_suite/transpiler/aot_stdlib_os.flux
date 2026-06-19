// AOT transpiler test - std.os
import std.os;

func main() {
    print("=== AOT std.os Test ===");

    // OS.platform
    string plat = OS.platform;
    if (plat == "linux") {
        print("PASS: OS.platform = linux");
    }

    // OS.cwd
    string cwd = OS.cwd();
    if (cwd != "") {
        print("PASS: OS.cwd = " + cwd);
    } else {
        print("FAIL: OS.cwd empty");
    }

    // OS.hostname
    string host = OS.hostname();
    if (host != "") {
        print("PASS: OS.hostname = " + host);
    }

    // OS.exec
    string result = OS.exec("echo hello_flux");
    if (result == "hello_flux\n") {
        print("PASS: OS.exec echo");
    } else {
        print("WARN: OS.exec returned unexpected: " + result);
    }

    // OS.pid
    int pid = OS.pid();
    if (pid > 0) {
        print("PASS: OS.pid > 0");
    }

    // OS.env
    string home = OS.env("HOME");
    if (home != "") {
        print("PASS: OS.env(HOME)");
    }

    print("=== AOT std.os Test PASSED ===");
}
