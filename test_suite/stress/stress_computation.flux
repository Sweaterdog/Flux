# Stress Test: Recursion depth, nested expressions, and computational limits

# Test 1: Deep recursion - Ackermann function (small inputs!)
# Ackermann grows extremely fast, so we use tiny inputs
func ackermann(int m, int n) -> int {
    if (m == 0) {
        return n + 1;
    }
    if (n == 0) {
        return ackermann(m - 1, 1);
    }
    return ackermann(m - 1, ackermann(m, n - 1));
}

# Test 2: Fibonacci with memoization via list
func fib_memo(int n, list<int> memo) -> int {
    if (n <= 1) {
        return n;
    }
    if (memo[n] != -1) {
        return memo[n];
    }
    int result = fib_memo(n - 1, memo) + fib_memo(n - 2, memo);
    memo[n] = result;
    return result;
}

# Test 3: Deeply nested arithmetic
func nested_math() -> int {
    int a = ((((((1 + 2) * 3) - 4) + 5) * 6) - 7);
    int b = ((((a + a) * 2) + a) - 1);
    return a + b;
}

# Test 4: Large loop with accumulation
func sum_to_n(int n) -> int {
    int sum = 0;
    for (int i = 1; i <= n; i++) {
        sum = sum + i;
    }
    return sum;
}

# Test 5: Nested for loops
func matrix_sum() -> int {
    int total = 0;
    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 50; j++) {
            total = total + i * j;
        }
    }
    return total;
}

# Test 6: String building in a loop (memory pressure)
func build_long_string(int n) -> string {
    string result = "";
    for (int i = 0; i < n; i++) {
        result += "x";
    }
    return result;
}

# Test 7: List building and manipulation
func list_stress() -> void {
    int items = [];
    for (int i = 0; i < 1000; i++) {
        items.add(i);
    }
    print("List size after 1000 adds: ${items.length}");

    # Sum all elements
    int sum = 0;
    for (int i = 0; i < items.length; i++) {
        sum = sum + items[i];
    }
    print("Sum of 0..999: $sum");

    # Remove from end repeatedly
    for (int i = 0; i < 500; i++) {
        items.removeAt(items.length - 1);
    }
    print("List size after 500 removals: ${items.length}");
}

func main() {
    print("=== Computational Stress Tests ===");

    # Ackermann (small values only - ack(3,4) = 125)
    int a34 = ackermann(3, 4);
    print("ackermann(3,4) = $a34");

    # Memoized fibonacci
    int memo = [];
    for (int i = 0; i < 40; i++) {
        memo.add(-1);
    }
    int fib39 = fib_memo(39, memo);
    print("fib(39) = $fib39");

    # Nested math
    int nm = nested_math();
    print("nested_math() = $nm");

    # Large loop sum
    int s = sum_to_n(10000);
    print("sum_to_n(10000) = $s");

    # Matrix sum
    int ms = matrix_sum();
    print("matrix_sum() = $ms");

    # String building
    string long_str = build_long_string(5000);
    print("String length: ${len(long_str)}");

    # List stress
    list_stress();

    print("=== Done ===");
}
