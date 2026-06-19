# File: classes.flux
# Tests: class declaration, constructors, methods, inheritance, access modifiers

class Animal {
    public string name;
    public int legs;

    func init(string n, int l) {
        name = n;
        legs = l;
    }

    public func speak() -> void {
        print("$name says hello.");
    }

    public func describe() -> string {
        return "$name has $legs legs.";
    }
}

class Dog extends Animal {
    public string breed;

    func init(string n, string b) {
        super.init(n, 4);
        breed = b;
    }

    # Override parent method
    public func speak() -> void {
        print("$name barks: Woof!");
    }

    public func fetch() -> void {
        print("$name fetches the ball!");
    }
}

func main() {
    # Create an instance
    Animal cat = new Animal("Whiskers", 4);
    cat.speak();
    string desc = cat.describe();
    print(desc);

    # Inheritance
    Dog rex = new Dog("Rex", "Golden Retriever");
    rex.speak();
    rex.fetch();
    print("Breed: ${rex.breed}");
}
