# File: regex_test.flux
# Tests: Regex module from std.regex

import std.regex;

func main() {
    auto r = Regex("[0-9]+");

    # match
    bool m1 = r.match("12345");
    print("match 12345: $m1");

    bool m2 = r.match("hello");
    print("match hello: $m2");

    # search — find first match in a string
    string found = r.search("abc 42 def");
    print("search found: $found");

    # replace
    string replaced = r.replace("call 555-1234", "***");
    print("replace: $replaced");
}
