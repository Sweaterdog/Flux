# File: variables.flux
# Tests: variable declarations, types, constants, type redefinition

func main() {
    # Integer declaration
    int score = 100;
    print("Score: $score");

    # Float
    float gravity = 9.81;
    print("Gravity: $gravity");

    # String
    string name = "Atlas";
    print("Name: $name");

    # Boolean
    bool alive = true;
    print("Alive: $alive");

    # Char
    char grade = 'A';
    print("Grade: $grade");

    # Zero-initialized defaults
    int counter;
    print("Counter (default): $counter");

    float temp;
    print("Temp (default): $temp");

    string buffer;
    print("Buffer (default): '$buffer'");

    bool flag;
    print("Flag (default): $flag");

    # Constants (UPPER_SNAKE_CASE)
    int MAX_PLAYERS = 16;
    print("Max players: $MAX_PLAYERS");

    # Type re-definition (the "Flux" behavior)
    int count = 42;
    print("count as int: $count");
    count = string = "forty-two";
    print("count as string: $count");

    # Type redef with auto-conversion
    int num = 100;
    num = string;
    print("num auto-converted to string: $num");
}
