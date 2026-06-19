# File: casting.flux
# Tests: explicit type casting

func main() {
    # Float to int (truncation)
    float score = 98.6;
    int rounded = (int) score;
    print("(int) 98.6 = $rounded");

    # Int to string
    int num = 42;
    string numStr = (string) num;
    print("(string) 42 = '$numStr'");

    # String to int
    string input = "123";
    int parsed = (int) input;
    print("(int) '123' = $parsed");

    # Int to float
    int a = 5;
    float b = (float) a;
    print("(float) 5 = $b");

    # Bool to int
    bool t = true;
    int truthVal = (int) t;
    print("(int) true = $truthVal");

    # Int to bool
    int zero = 0;
    int one = 1;
    print("(bool) 0 = ${(bool) zero}");
    print("(bool) 1 = ${(bool) one}");
}
