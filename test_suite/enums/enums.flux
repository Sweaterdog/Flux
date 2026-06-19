# File: enums.flux
# Tests: enum declaration, member access, switch on enum

enum Direction {
    NORTH,
    SOUTH,
    EAST,
    WEST
}

enum HttpStatus {
    OK = 200,
    NOT_FOUND = 404,
    ERROR = 500
}

func main() {
    Direction heading = Direction.NORTH;
    print("Heading: $heading");

    if (heading == Direction.NORTH) {
        print("Going north!");
    }

    # Switch on enum
    switch (heading) {
        case Direction.NORTH:
            print("Switch: North");
            break;
        case Direction.SOUTH:
            print("Switch: South");
            break;
        default:
            print("Switch: Other direction");
    }

    # Enum with explicit values
    HttpStatus code = HttpStatus.NOT_FOUND;
    print("Status: $code");
}
