# Stress Test: Edge cases in string interpolation, type coercion, and control flow
# Uses only currently-valid Flux syntax

# Test 1: Nested string interpolation
func test_nested_interpolation() -> void {
    int x = 5;
    int y = 10;
    string msg = "result is ${x + y} and double is ${(x + y) * 2}";
    print(msg);

    # Interpolation with function calls
    print("sqrt(144) = ${math.sqrt(144)}");

    # Interpolation with nested object access
    print("PI is ${math.PI}");

    # Multiple interpolations in one string
    string a = "x=$x, y=$y, sum=${x + y}";
    print(a);
}

# Test 2: Type coercion edge cases
func test_type_coercion() -> void {
    # int + float should become float
    int a = 5;
    float b = 2.5;
    float c = a + b;
    print("int + float = $c (type: ${typeof(c)})");

    # String concatenation with + and non-strings
    string s = "value: " + 42;
    print(s);

    # Boolean in arithmetic context - use C-style cast
    bool t = true;
    int bval = (int) t;
    print("(int) true = $bval");

    # string to int cast
    string numStr = "123";
    int parsed = (int) numStr;
    print("(int) \"123\" = $parsed");
}

# Test 3: Complex control flow
func test_control_flow() -> void {
    # Nested if/elif/else
    for (int i = 0; i < 10; i++) {
        if (i < 3) {
            print_raw("lo ");
        } elif (i < 7) {
            print_raw("mid ");
        } else {
            print_raw("hi ");
        }
    }
    print("");

    # While with break and continue
    int count = 0;
    int sum = 0;
    while (true) {
        count++;
        if (count % 2 == 0) {
            continue;
        }
        if (count > 20) {
            break;
        }
        sum += count;
    }
    print("Sum of odd numbers 1-19: $sum");

    # Do-while
    int n = 0;
    do {
        n++;
    } while (n < 5);
    print("do-while result: $n");
}

# Test 4: For-each over strings (using 'for' with 'in')
func test_foreach() -> void {
    string word = "Hello";
    string reversed = "";
    string chars = [];
    for (string ch in word) {
        chars.add(ch);
    }
    # Build reversed string
    for (int i = chars.length - 1; i >= 0; i--) {
        reversed += chars[i];
    }
    print("Reversed '$word' = '$reversed'");
}

# Test 5: Switch with default
func classify(int n) -> string {
    switch (n) {
        case 1:
            return "one";
        case 2:
            return "two";
        case 3:
            return "three";
        default:
            return "other";
    }
}

func test_switch() -> void {
    print("classify(1) = ${classify(1)}");
    print("classify(2) = ${classify(2)}");
    print("classify(3) = ${classify(3)}");
    print("classify(99) = ${classify(99)}");
}

# Test 6: Error handling with try/catch nesting
func risky(int depth) -> int {
    if (depth <= 0) {
        throw "too deep";
    }
    return risky(depth - 1);
}

func test_error_handling() -> void {
    try {
        risky(0);
    } catch (error e) {
        print("Caught: ${e.message}");
    }

    # Nested try/catch
    try {
        try {
            throw "inner error";
        } catch (error e1) {
            print("Inner catch: ${e1.message}");
            throw "rethrown";
        }
    } catch (error e2) {
        print("Outer catch: ${e2.message}");
    }
}

# Test 7: Const enforcement via UPPER_SNAKE_CASE
func test_const() -> void {
    int MAX = 100;
    print("MAX = $MAX");

    try {
        # This SHOULD throw an error since MAX is UPPER_SNAKE_CASE
        MAX = 200;
    } catch (error e) {
        print("Const violation caught: ${e.message}");
    }
}

# Test 8: Integer overflow and edge values
func test_numeric_edges() -> void {
    int big = 2147483647;
    print("Max int: $big");
    int overflow = big + 1;
    print("Max int + 1: $overflow");

    # Float division by zero should give inf, but currently throws
    try {
        float inf = 1.0 / 0.0;
        print("1.0 / 0.0 = $inf");
    } catch (error e) {
        print("Float div by zero threw: ${e.message} (BUG - should return inf)");
    }
}

# Test 9: Empty and null edge cases
func test_empty_cases() -> void {
    string empty = "";
    print("Empty string length: ${len(empty)}");
    bool is_empty = empty == "";
    print("Empty string is empty: $is_empty");

    int zero = 0;
    print("Zero is: $zero");
}

func main() {
    print("=== Edge Case Stress Tests ===");
    test_nested_interpolation();
    test_type_coercion();
    test_control_flow();
    test_foreach();
    test_switch();
    test_error_handling();
    test_const();
    test_numeric_edges();
    test_empty_cases();
    print("=== Done ===");
}
