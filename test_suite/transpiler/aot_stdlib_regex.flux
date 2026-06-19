// AOT transpiler test - std.regex
import std.regex;

func main() {
    print("=== AOT std.regex Test ===");

    // Create regex
    Regex re = Regex("[0-9]+");

    // Match
    if (re.match("12345")) {
        print("PASS: match full numeric");
    } else {
        print("FAIL: match");
    }

    // Search
    string found = re.search("abc 42 def");
    if (found == "42") {
        print("PASS: search found 42");
    } else {
        print("FAIL: search got: " + found);
    }

    // Replace
    string replaced = re.replace("abc 42 def 99", "NUM");
    print("replace result: " + replaced);

    // Split
    Regex splitRe = Regex("[,;]");
    list<string> parts = splitRe.split("a,b;c");
    if (len(parts) == 3) {
        print("PASS: split into 3 parts");
    } else {
        print("FAIL: split");
    }

    print("=== AOT std.regex Test PASSED ===");
}
