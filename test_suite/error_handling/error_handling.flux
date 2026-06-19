# File: error_handling.flux
# Tests: try/catch, throw, finally, panic

func riskyOperation(int x) -> int {
    if (x < 0) {
        throw "Negative value not allowed";
    }
    return x * 2;
}

func main() {
    # Basic try/catch
    try {
        int result = riskyOperation(-5);
        print("Result: $result");
    } catch (error e) {
        print("Caught error: ${e.message}");
    } finally {
        print("Finally block executed.");
    }

    # Successful try
    try {
        int result = riskyOperation(10);
        print("Success: $result");
    } catch (error e) {
        print("This should not print");
    }

    # Division by zero
    try {
        int a = 10;
        int b = 0;
        int c = a / b;
        print("Should not reach here");
    } catch (error e) {
        print("Caught: ${e.message}");
    }
}
