# Stress Test: Closures, lambdas, and variable capture
# Uses only features the parser currently supports

# Test 1: Lambda stored inline and called
func test_lambda_basic() -> void {
    int result = ((int x) => { return x * 3; })(5);
    print("lambda(5) = $result");
}

# Test 2: Nested function calls (deep call stack)
func double_val(int x) -> int {
    return x * 2;
}

func triple_val(int x) -> int {
    return x * 3;
}

func test_nested_function_calls() -> void {
    int r = double_val(triple_val(double_val(5)));
    print("double(triple(double(5))) = $r");

    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum = sum + double_val(i);
    }
    print("sum(double(0..9)) = $sum");
}

# Test 3: Mutual recursion (two-pass declaration should handle this)
func is_even(int n) -> bool {
    if (n == 0) { return true; }
    return is_odd(n - 1);
}

func is_odd(int n) -> bool {
    if (n == 0) { return false; }
    return is_even(n - 1);
}

func test_mutual_recursion() -> void {
    print("is_even(10) = ${is_even(10)}");
    print("is_odd(7) = ${is_odd(7)}");
    print("is_even(0) = ${is_even(0)}");
    print("is_odd(0) = ${is_odd(0)}");
}

# Test 4: Deep recursive fibonacci
func fib(int n) -> int {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}

func test_deep_recursion() -> void {
    int r = fib(25);
    print("fib(25) = $r");
}

# Test 5: Lambda passed to list.sort
func test_lambda_sort() -> void {
    int nums = [5, 2, 8, 1, 9, 3, 7, 4, 6, 0];
    nums.sort((int a, int b) => { return a < b; });
    print_raw("Sorted: ");
    for (int i = 0; i < nums.length; i++) {
        print_raw("${nums[i]} ");
    }
    print("");
}

# Test 6: Immediately invoked lambda with multiple args
func test_iife() -> void {
    int x = ((int a, int b) => { return a * b + 1; })(6, 7);
    print("IIFE result: $x");
}

func main() {
    print("=== Closure & Lambda Stress Tests ===");
    test_lambda_basic();
    test_nested_function_calls();
    test_mutual_recursion();
    test_deep_recursion();
    test_lambda_sort();
    test_iife();
    print("=== Done ===");
}
