# Stress Test: Class and OOP edge cases
# Tries to break inheritance, method dispatch, and field write-back

# Test 1: Three-level inheritance chain with method override at each level
class Base {
    public string tag;

    func init() {
        tag = "Base";
    }

    public func identify() -> string {
        return "I am $tag";
    }

    public func value() -> int {
        return 1;
    }
}

class Middle extends Base {
    public int level;

    func init() {
        super.init();
        tag = "Middle";
        level = 2;
    }

    public func value() -> int {
        return level * 10;
    }
}

class Child extends Middle {
    public string extra;

    func init() {
        super.init();
        tag = "Child";
        extra = "special";
    }

    public func value() -> int {
        return level * 100;
    }

    public func full_info() -> string {
        return "${identify()} / level=$level / extra=$extra / value=${value()}";
    }
}

# Test 2: Object modifying its own fields via method calls
class Counter {
    public int count;

    func init() {
        count = 0;
    }

    public func increment() -> void {
        count = count + 1;
    }

    public func increment_by(int n) -> void {
        for (int i = 0; i < n; i++) {
            increment();
        }
    }

    public func get_count() -> int {
        return count;
    }
}

# Test 3: Object passed to function and modified
func double_counter(Counter c) -> void {
    int current = c.get_count();
    c.increment_by(current);
}

# Test 4: Class with method returning new instance
class Node {
    public int val;
    public string label;

    func init(int v) {
        val = v;
        label = "node_$v";
    }

    public func next() -> Node {
        return new Node(val + 1);
    }

    public func describe() -> string {
        return "[$label: $val]";
    }
}

func main() {
    print("=== OOP Stress Tests ===");

    # Three-level inheritance
    Child c = new Child();
    print(c.full_info());
    print("tag = ${c.tag}");
    print("level = ${c.level}");

    # Method modifying own fields
    Counter ctr = new Counter();
    ctr.increment();
    ctr.increment();
    ctr.increment();
    print("After 3 increments: ${ctr.get_count()}");

    ctr.increment_by(7);
    print("After increment_by(7): ${ctr.get_count()}");

    # Chained method calls via new objects
    Node n = new Node(1);
    Node n2 = n.next();
    Node n3 = n2.next();
    print("n=${n.val} / n2=${n2.val} / n3=${n3.val}");
    print(n.describe());
    print(n2.describe());
    print(n3.describe());

    print("=== Done ===");
}
