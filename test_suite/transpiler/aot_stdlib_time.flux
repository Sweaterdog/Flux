// AOT transpiler test - std.time
import std.time;

func main() {
    print("=== AOT std.time Test ===");

    // Time.now
    float now = Time.now();
    if (now > 1000000000.0) {
        print("PASS: Time.now > 1 billion");
    } else {
        print("FAIL: Time.now too small");
    }

    // Time.nowMs
    long nowMs = Time.nowMs();
    if (nowMs > 0) {
        print("PASS: Time.nowMs > 0");
    } else {
        print("FAIL: Time.nowMs");
    }

    // Time.year
    int yr = Time.year(now);
    if (yr >= 2024) {
        print("PASS: Time.year >= 2024");
    } else {
        print("FAIL: Time.year");
    }

    // Timer
    Timer timer = Timer();
    timer.start();
    // do something
    int sum = 0;
    int i = 0;
    while (i < 100000) {
        sum = sum + i;
        i = i + 1;
    }
    timer.stop();
    float elapsed = timer.elapsed();
    if (elapsed >= 0.0) {
        print("PASS: Timer elapsed >= 0");
    } else {
        print("FAIL: Timer elapsed");
    }

    print("=== AOT std.time Test PASSED ===");
}
