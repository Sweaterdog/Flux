# File: string_interpolation.flux
# Tests: $varName, ${expression}, string operations

func main() {
    string name = "Flux";
    int version = 1;
    float pi = 3.14159;

    # Simple variable interpolation
    print("Language: $name");

    # Expression interpolation
    print("Version times 10: ${version * 10}");
    print("Pi rounded: ${pi}");

    # Mixed
    print("$name v$version is running!");

    # String concatenation with +
    string a = "Hello";
    string b = "World";
    string c = a + ", " + b + "!";
    print(c);

    # String length
    print("Length of '$c': ${c.length}");

    # Compound string assignment
    string msg = "Start";
    msg += " Middle";
    msg += " End";
    print(msg);
}
