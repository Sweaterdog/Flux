# Stress Test: Complex class interactions
# Multiple objects interacting, method calls returning objects, chained calls

class Vector2 {
    public float x;
    public float y;

    func init(float x_val, float y_val) {
        x = x_val;
        y = y_val;
    }

    public func add(Vector2 other) -> Vector2 {
        return new Vector2(x + other.x, y + other.y);
    }

    public func scale(float factor) -> Vector2 {
        return new Vector2(x * factor, y * factor);
    }

    public func magnitude() -> float {
        return math.sqrt(x * x + y * y);
    }

    public func to_string() -> string {
        return "($x, $y)";
    }
}

# Test: Object method returning new object, chained construction
func test_vector_ops() -> void {
    Vector2 a = new Vector2(3.0, 4.0);
    Vector2 b = new Vector2(1.0, 2.0);

    Vector2 c = a.add(b);
    print("a + b = ${c.to_string()}");

    Vector2 d = a.scale(2.0);
    print("a * 2 = ${d.to_string()}");

    float mag = a.magnitude();
    print("|a| = $mag");
}

# Test: List of objects
func test_object_list() -> void {
    Vector2 points = [];

    for (int i = 0; i < 10; i++) {
        float fx = i * 1.0;
        float fy = i * 2.0;
        points.add(new Vector2(fx, fy));
    }

    print("Created ${points.length} vectors");

    float total_mag = 0.0;
    for (int i = 0; i < points.length; i++) {
        total_mag = total_mag + points[i].magnitude();
    }
    print("Total magnitude: $total_mag");
}

# Test: Class with method that calls other methods on same object
class Calculator {
    public float memory;
    public int operations;

    func init() {
        memory = 0.0;
        operations = 0;
    }

    public func add(float val) -> void {
        memory = memory + val;
        operations = operations + 1;
    }

    public func multiply(float val) -> void {
        memory = memory * val;
        operations = operations + 1;
    }

    public func reset() -> void {
        memory = 0.0;
        operations = operations + 1;
    }

    public func status() -> string {
        return "mem=$memory ops=$operations";
    }
}

func test_calculator() -> void {
    Calculator calc = new Calculator();
    calc.add(10.0);
    calc.multiply(3.0);
    calc.add(5.0);
    print("calc: ${calc.status()}");
    print("memory = ${calc.memory}");
    print("operations = ${calc.operations}");
}

# Test: Enum used in switch
enum Direction {
    NORTH = 0,
    SOUTH = 1,
    EAST = 2,
    WEST = 3
}

func direction_name(int dir) -> string {
    if (dir == 0) { return "NORTH"; }
    if (dir == 1) { return "SOUTH"; }
    if (dir == 2) { return "EAST"; }
    if (dir == 3) { return "WEST"; }
    return "UNKNOWN";
}

func test_enum() -> void {
    int d = Direction.NORTH;
    print("Direction value: $d");
    print("Direction name: ${direction_name(d)}");

    int e = Direction.EAST;
    print("East = $e");
}

func main() {
    print("=== Complex Interaction Stress Tests ===");
    test_vector_ops();
    test_object_list();
    test_calculator();
    test_enum();
    print("=== Done ===");
}
