# File: aot_string_methods.flux
# Tests: AOT compilation of string methods — substring, indexOf, contains,
#        startsWith, endsWith, toUpper, toLower, replace, charAt, reverse

func main() {
    string text = "Hello, World!";

    # substring
    string sub = text.substring(7, 5);
    print("substring: $sub");

    # indexOf
    int idx = text.indexOf("World");
    print("indexOf: $idx");

    # contains
    bool has = text.contains("Hello");
    print("contains: $has");

    # startsWith / endsWith
    bool sw = text.startsWith("Hello");
    print("startsWith: $sw");

    bool ew = text.endsWith("!");
    print("endsWith: $ew");

    # toUpper / toLower
    string upper = text.toUpper();
    print("toUpper: $upper");

    string lower = text.toLower();
    print("toLower: $lower");

    # replace
    string replaced = text.replace("World", "AOT");
    print("replace: $replaced");

    # charAt
    string ch = text.charAt(0);
    print("charAt: $ch");

    # reverse
    string rev = "abcde";
    string reversed = rev.reverse();
    print("reverse: $reversed");

    # trim
    string padded = "  spaced  ";
    string trimmed = padded.trim();
    print("trim: '$trimmed'");
}
