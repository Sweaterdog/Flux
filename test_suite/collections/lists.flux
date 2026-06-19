# File: lists.flux
# Tests: list literals, length, add, indexing, for-each, sort

func main() {
    # List literal
    int nums = [10, 20, 30, 40, 50];
    print("List: $nums");
    print("Length: ${nums.length}");

    # Index access
    print("nums[0] = ${nums[0]}");
    print("nums[2] = ${nums[2]}");

    # Index set
    nums[1] = 25;
    print("After nums[1] = 25: $nums");

    # add
    nums.add(60);
    print("After add(60): $nums");
    print("New length: ${nums.length}");

    # For-each iteration
    print("For-each:");
    string names = ["Alice", "Bob", "Carol"];
    for (string name in names) {
        print("  Hello, $name!");
    }

    # contains
    string checkBob = "Bob";
    string checkDave = "Dave";
    bool hasBob = names.contains(checkBob);
    bool hasDave = names.contains(checkDave);
    print("Contains 'Bob': $hasBob");
    print("Contains 'Dave': $hasDave");

    # sort with lambda
    int unsorted = [5, 2, 8, 1, 9, 3];
    print("Before sort: $unsorted");
    unsorted.sort((a, b) => a < b);
    print("After sort: $unsorted");
}
