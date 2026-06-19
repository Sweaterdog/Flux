# File: operators.flux
# Tests: arithmetic, comparison, logical, butnot, semantic comparators

func main() {
    # Arithmetic
    int a = 10;
    int b = 3;
    print("10 + 3 = ${a + b}");
    print("10 - 3 = ${a - b}");
    print("10 * 3 = ${a * b}");
    print("10 / 3 = ${a / b}");
    print("10 % 3 = ${a % b}");

    # Increment / decrement
    int x = 5;
    x++;
    print("5++ = $x");
    x--;
    print("6-- = $x");

    # Compound assignment
    int y = 10;
    y += 5;
    print("10 += 5 = $y");
    y -= 3;
    print("15 -= 3 = $y");
    y *= 2;
    print("12 *= 2 = $y");
    y /= 4;
    print("24 /= 4 = $y");
    y %= 4;
    print("6 %= 4 = $y");

    # Comparison
    print("5 == 5: ${5 == 5}");
    print("5 != 6: ${5 != 6}");
    print("3 < 5: ${3 < 5}");
    print("5 > 3: ${5 > 3}");
    print("5 <= 5: ${5 <= 5}");
    print("10 >= 9: ${10 >= 9}");

    # Logical
    bool t = true;
    bool f = false;
    print("true && false: ${t && f}");
    print("true || false: ${t || f}");
    print("!true: ${!t}");

    # butnot
    bool isAdmin = true;
    bool isBanned = false;
    if (isAdmin butnot isBanned) {
        print("Access granted (butnot works).");
    }

    # Semantic comparators
    # == (identity equality)
    string s10 = "10";
    bool idEq1 = s10 == 10;
    print("\"10\" == 10: $idEq1");
    bool idEq2 = 1 == true;
    print("1 == true: $idEq2");

    # =num= (numeric strict)
    bool numEq1 = 50.0 =num= 50;
    print("50.0 =num= 50: $numEq1");
    string s50 = "50";
    bool numEq2 = s50 =num= 50;
    print("\"50\" =num= 50: $numEq2");

    # =word= (string strict)
    bool wordEq1 = "hello" =word= "hello";
    print("\"hello\" =word= \"hello\": $wordEq1");
    bool wordEq2 = "Hello" =word= "hello";
    print("\"Hello\" =word= \"hello\": $wordEq2");
    bool wordEq3 = s50 =word= 50;
    print("\"50\" =word= 50: $wordEq3");
}
