# File: string_methods.flux
# Tests: string methods — substring, indexOf, contains, startsWith, endsWith,
#        split, trim, toUpper, toLower, replace, charAt, reverse

func main() {
    string text = "Hello, World!";

    # substring(start, length)
    string sub = text.substring(7, 5);
    print("substring(7,5): $sub");

    # indexOf(search)
    int idx = text.indexOf("World");
    print("indexOf('World'): $idx");

    int missing = text.indexOf("xyz");
    print("indexOf('xyz'): $missing");

    # contains
    bool has = text.contains("Hello");
    print("contains('Hello'): $has");

    bool noHas = text.contains("Goodbye");
    print("contains('Goodbye'): $noHas");

    # startsWith / endsWith
    bool sw = text.startsWith("Hello");
    print("startsWith('Hello'): $sw");

    bool ew = text.endsWith("!");
    print("endsWith('!'): $ew");

    # split
    string csv = "apple,banana,cherry";
    list parts = csv.split(",");
    print("split count: ${parts.length}");
    print("split[0]: ${parts[0]}");
    print("split[1]: ${parts[1]}");
    print("split[2]: ${parts[2]}");

    # trim
    string padded = "  hello  ";
    string trimmed = padded.trim();
    print("trim: '$trimmed'");

    # toUpper / toLower
    string upper = text.toUpper();
    print("toUpper: $upper");

    string lower = text.toLower();
    print("toLower: $lower");

    # replace
    string replaced = text.replace("World", "Flux");
    print("replace: $replaced");

    # charAt
    string ch = text.charAt(0);
    print("charAt(0): $ch");

    # reverse
    string rev = "abcde";
    string reversed = rev.reverse();
    print("reverse: $reversed");
}
