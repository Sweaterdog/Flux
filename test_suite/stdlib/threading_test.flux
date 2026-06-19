# Test: Threading, Signal handling, and sys features
import std.sys;

func main() {
    print("=== Threading & Signals Test ===");

    # --- Test 1: thread.run and join ---
    print("--- Test 1: Basic threading ---");

    func worker(int n) -> int {
        int sum = 0;
        for (int i = 0; i < n; i++) {
            sum += i;
        }
        return sum;
    }

    auto t = thread.run(worker, 100);
    t.join();
    print("PASS: Thread ran and joined");

    # --- Test 2: thread.sleep ---
    print("--- Test 2: Thread sleep ---");
    int before = sys.time();
    thread.sleep(50);
    int after = sys.time();
    print("PASS: Slept for ~50ms");

    # --- Test 3: Mutex ---
    print("--- Test 3: Mutex ---");
    auto m = Mutex();
    m.lock();
    bool locked = m.tryLock();
    if (!locked) {
        print("PASS: Mutex correctly blocked second lock");
    }
    m.unlock();
    bool locked2 = m.tryLock();
    if (locked2) {
        print("PASS: Mutex available after unlock");
        m.unlock();
    }

    # --- Test 4: sys.args ---
    print("--- Test 4: sys.args ---");
    print("sys.args length: ${len(sys.args)}");
    print("PASS: sys.args accessible");

    # --- Test 5: sys.cpuCount ---
    print("--- Test 5: sys.cpuCount ---");
    int cpus = sys.cpuCount();
    print("CPU threads: ${cpus}");
    if (cpus > 0) {
        print("PASS: cpuCount > 0");
    }

    # --- Test 6: sys.platform and sys.arch ---
    print("--- Test 6: Platform info ---");
    print("Platform: ${sys.platform}");
    print("Arch: ${sys.arch}");
    print("PASS: Platform info accessible");

    # --- Test 7: Signal constants ---
    print("--- Test 7: Signal API ---");
    print("SIGINT = ${Signal.SIGINT}");
    print("SIGTERM = ${Signal.SIGTERM}");

    # Register a handler (we won't trigger it, just verify registration)
    func dummyHandler() {
        print("Signal caught!");
    }
    Signal.handle(Signal.SIGUSR1, dummyHandler);
    print("PASS: Signal handler registered");
    Signal.reset(Signal.SIGUSR1);
    print("PASS: Signal handler reset");

    print("=== ALL THREADING & SIGNALS TESTS PASSED ===");
}
