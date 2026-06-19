# AOT Test: std.json — real JSON parsing and stringification
# Verifies that AOT-compiled JSON functions actually parse and validate.

import std.json;

func main() {
    print("=== AOT JSON Test ===");

    # Parse a simple object
    string parsed = JSON.parse("{\"name\": \"Flux\", \"version\": 1}");
    print("Parsed: $parsed");

    # Parse an array
    string arr = JSON.parse("[1, 2, 3, \"hello\", true, null]");
    print("Array: $arr");

    # Parse nested structures
    string nested = JSON.parse("{\"app\": {\"name\": \"StratOS\", \"modules\": [\"kernel\", \"gui\"]}}");
    print("Nested: $nested");

    # Stringify with indent
    string pretty = JSON.stringify("{\"a\":1,\"b\":2}", 2);
    print("Pretty:");
    print(pretty);

    # Invalid JSON should throw
    bool caught = false;
    try {
        JSON.parse("{invalid json}");
    } catch (error e) {
        caught = true;
        print("Caught invalid JSON error (expected)");
    }
    if (!caught) {
        print("WARNING: Invalid JSON did not throw");
    }

    # Parse booleans and null
    string bools = JSON.parse("{\"yes\": true, \"no\": false, \"nothing\": null}");
    print("Booleans: $bools");

    # Parse numbers (including negative and float)
    string nums = JSON.parse("{\"int\": 42, \"neg\": -7, \"float\": 3.14}");
    print("Numbers: $nums");

    # Stringify compact
    string compact = JSON.stringify("{\"a\": 1, \"b\": 2}");
    print("Compact: $compact");

    print("=== AOT JSON Test PASSED ===");
}
