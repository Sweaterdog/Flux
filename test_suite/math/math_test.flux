# File: math_test.flux
# Tests: math module functions and constants

import std.math;

func main() {
    # Basic math functions
    print("sqrt(144) = ${math.sqrt(144)}");
    print("pow(2, 10) = ${math.pow(2, 10)}");
    print("abs(-42.5) = ${math.abs(-42.5)}");
    print("floor(3.9) = ${math.floor(3.9)}");
    print("ceil(3.1) = ${math.ceil(3.1)}");
    print("round(3.5) = ${math.round(3.5)}");
    print("min(5, 3) = ${math.min(5, 3)}");
    print("max(5, 3) = ${math.max(5, 3)}");
    print("clamp(15, 0, 10) = ${math.clamp(15, 0, 10)}");
    print("lerp(0, 100, 0.5) = ${math.lerp(0, 100, 0.5)}");

    # Constants
    print("PI = ${math.PI}");
    print("E = ${math.E}");

    # Trigonometry
    print("sin(PI/2) = ${math.sin(math.PI / 2)}");
    print("cos(0) = ${math.cos(0)}");

    # Random
    float r = float.random;
    print("Random float: $r");

    int ri = int.random;
    print("Random int: $ri");

    bool rb = bool.random;
    print("Random bool: $rb");
}
