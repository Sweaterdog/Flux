# File: io_test.flux
# Tests: File I/O from std.io

import std.io;

func main() {
    string testFile = "/tmp/flux_io_test.txt";

    # Write
    fs.write(testFile, "Hello from Flux IO!");
    print("Write completed");

    # Exists
    bool exists = fs.exists(testFile);
    print("File exists: $exists");

    # Read
    string content = fs.read(testFile);
    print("Content: $content");

    # Append
    fs.append(testFile, "\nSecond line");
    print("Append completed");

    # Size
    int size = fs.size(testFile);
    print("Size > 0: ${size > 0}");

    # Clean up
    fs.remove(testFile);
    print("Cleanup done");
    print("Original removed: ${!fs.exists(testFile)}");
}
