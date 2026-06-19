# File: collections_map.flux
# Tests: Map from std.collections

import std.collections;

func main() {
    auto m = Map();

    m.put("name", "Flux");
    m.put("version", "1.0");
    m.put("type", "language");

    print("Size: ${m.size()}");

    string name = m.get("name");
    print("Name: $name");

    bool hasVersion = m.hasKey("version");
    print("Has version: $hasVersion");

    bool hasAuthor = m.hasKey("author");
    print("Has author: $hasAuthor");

    m.remove("type");
    print("Size after remove: ${m.size()}");
}
