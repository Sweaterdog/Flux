// AOT transpiler test - std.io filesystem operations
import std.io;

func main() {
    print("=== AOT std.io Test ===");

    // Write a file
    fs.write("/tmp/flux_aot_test.txt", "hello from AOT");

    // Read it back
    string content = fs.read("/tmp/flux_aot_test.txt");
    if (content == "hello from AOT") {
        print("PASS: fs.write + fs.read");
    } else {
        print("FAIL: fs.read returned unexpected content");
    }

    // Check exists
    if (fs.exists("/tmp/flux_aot_test.txt")) {
        print("PASS: fs.exists");
    } else {
        print("FAIL: fs.exists");
    }

    // Append
    fs.append("/tmp/flux_aot_test.txt", " appended");
    string content2 = fs.read("/tmp/flux_aot_test.txt");
    if (content2 == "hello from AOT appended") {
        print("PASS: fs.append");
    } else {
        print("FAIL: fs.append");
    }

    // Cleanup
    fs.delete("/tmp/flux_aot_test.txt");
    if (fs.exists("/tmp/flux_aot_test.txt") == false) {
        print("PASS: fs.delete");
    } else {
        print("FAIL: fs.delete");
    }

    print("=== AOT std.io Test PASSED ===");
}
