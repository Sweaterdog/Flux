# File: collections_stack_queue.flux
# Tests: Stack and Queue from std.collections

import std.collections;

func main() {
    # Stack tests
    auto s = Stack();
    s.push(10);
    s.push(20);
    s.push(30);
    print("Stack size: ${s.size()}");
    print("Stack peek: ${s.peek()}");
    print("Stack pop: ${s.pop()}");
    print("Stack size after pop: ${s.size()}");
    print("Stack isEmpty: ${s.isEmpty()}");

    # Queue tests
    auto q = Queue();
    q.enqueue("first");
    q.enqueue("second");
    q.enqueue("third");
    print("Queue size: ${q.size()}");
    print("Queue peek: ${q.peek()}");
    print("Queue dequeue: ${q.dequeue()}");
    print("Queue size after dequeue: ${q.size()}");
    print("Queue isEmpty: ${q.isEmpty()}");
}
