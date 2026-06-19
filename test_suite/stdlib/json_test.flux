# File: json_test.flux
# Tests: JSON parse and stringify from std.json

import std.json;

func main() {
    # Stringify a simple object — test basic functionality
    string jsonStr = "{\"name\":\"Flux\",\"version\":1}";
    auto obj = JSON.parse(jsonStr);
    print("Parsed successfully");

    # Stringify
    string result = JSON.stringify(obj);
    print("Stringified: $result");
}
