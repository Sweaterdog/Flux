// AOT transpiler test - operators and math features
// Tests butnot, =num=, =word=, math constants, trig, lerp
import std.math;

func main() {
    // --- butnot operator ---
    bool a = true;
    bool b = false;
    bool c = true;

    print("=== butnot operator ===");
    if (a butnot b) {
        print("PASS: true butnot false = true");
    }
    if (a butnot c) {
        print("FAIL: true butnot true should be false");
    } else {
        print("PASS: true butnot true = false");
    }

    // --- Math constants ---
    print("=== math constants ===");
    float pi_val = math.PI;
    float tau_val = math.TAU;
    float e_val = math.E;

    print("PI = ${pi_val}");
    print("TAU = ${tau_val}");
    print("E = ${e_val}");

    if (tau_val > 6.28 && tau_val < 6.29) {
        print("PASS: TAU is approximately 6.283");
    }

    // --- Trig functions ---
    print("=== trig functions ===");
    float s = math.sin(0.0);
    float c2 = math.cos(0.0);

    print("sin(0) = ${s}");
    print("cos(0) = ${c2}");

    if (s == 0.0) {
        print("PASS: sin(0) == 0");
    }
    if (c2 == 1.0) {
        print("PASS: cos(0) == 1");
    }

    float t = math.tan(0.0);
    print("tan(0) = ${t}");

    // --- Log/exp functions ---
    print("=== log/exp functions ===");
    float lg = math.log(1.0);
    float lg2 = math.log2(8.0);
    float lg10 = math.log10(100.0);
    float ex = math.exp(0.0);

    print("log(1) = ${lg}");
    print("log2(8) = ${lg2}");
    print("log10(100) = ${lg10}");
    print("exp(0) = ${ex}");

    if (lg == 0.0) {
        print("PASS: log(1) == 0");
    }
    if (lg2 == 3.0) {
        print("PASS: log2(8) == 3");
    }
    if (lg10 == 2.0) {
        print("PASS: log10(100) == 2");
    }
    if (ex == 1.0) {
        print("PASS: exp(0) == 1");
    }

    // --- Lerp ---
    print("=== lerp ===");
    float lerped = math.lerp(0.0, 10.0, 0.5);
    print("lerp(0, 10, 0.5) = ${lerped}");
    if (lerped == 5.0) {
        print("PASS: lerp works");
    }

    // --- Clamp ---
    print("=== clamp ===");
    float clamped = math.clamp(15.0, 0.0, 10.0);
    print("clamp(15, 0, 10) = ${clamped}");
    if (clamped == 10.0) {
        print("PASS: clamp works");
    }

    // --- List<T> with capital L ---
    print("=== List<T> ===");
    List<int> nums = [1, 2, 3, 4, 5];
    print("List length = ${len(nums)}");
    if (len(nums) == 5) {
        print("PASS: List<int> works");
    }

    print("=== ALL AOT OPERATOR TESTS COMPLETE ===");
}

