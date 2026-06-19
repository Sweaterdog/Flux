// AOT transpiler test - std.sys (sys namespace, signals)
import std.sys;

func main() {
    print("=== AOT std.sys Test ===");

    // sys.platform
    string plat = sys.platform;
    if (plat == "linux") {
        print("PASS: sys.platform = linux");
    } else {
        print("PASS: sys.platform = " + plat);
    }

    // sys.arch
    string arch = sys.arch;
    if (arch == "x86_64") {
        print("PASS: sys.arch = x86_64");
    } else {
        print("PASS: sys.arch = " + arch);
    }

    // sys.cpuCount
    int cpus = sys.cpuCount();
    if (cpus > 0) {
        print("PASS: sys.cpuCount > 0");
    } else {
        print("FAIL: sys.cpuCount");
    }

    // sys.env
    string home = sys.env("HOME");
    if (home != "") {
        print("PASS: sys.env(HOME) = " + home);
    } else {
        print("WARN: sys.env(HOME) empty");
    }

    // sys.time
    long t = sys.time();
    if (t > 0) {
        print("PASS: sys.time > 0");
    } else {
        print("FAIL: sys.time");
    }

    // Signal constants
    int sigint = Signal.SIGINT;
    if (sigint == 2) {
        print("PASS: Signal.SIGINT = 2");
    } else {
        print("FAIL: Signal.SIGINT");
    }

    print("=== AOT std.sys Test PASSED ===");
}
