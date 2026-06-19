# File: functions.flux
# Tests: function declaration, return types, default params, named args, recursion, lambdas

func add(int a, int b) -> int {
    return a + b;
}

func greet(string name, int times) -> string {
    string result = "";
    for (int i = 0; i < times; i++) {
        result += "Hello, $name! ";
    }
    return result;
}

# Default return type (int, auto-returns 0)
func doNothing() {
    # implicitly returns 0
}

# Void return
func logMessage(string msg) -> void {
    print("[LOG] $msg");
}

# Default parameter values
func connect(string host, int port = 80) -> void {
    print("Connecting to $host:$port");
}

# Recursive factorial
func factorial(int n) -> int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

func main() {
    # Basic function call
    int sum = add(3, 7);
    print("add(3, 7) = $sum");

    # Function with string building
    string msg = greet("World", 2);
    print(msg);

    # Default return
    int status = doNothing();
    print("doNothing() returned: $status");

    # Void function
    logMessage("Server started");

    # Default parameters
    connect("google.com");
    connect("google.com", 443);

    # Recursion
    int fact5 = factorial(5);
    print("factorial(5) = $fact5");

    # Lambda
    int result = ((int x, int y) => x * y)(6, 7);
    print("Lambda (6 * 7) = $result");
}
