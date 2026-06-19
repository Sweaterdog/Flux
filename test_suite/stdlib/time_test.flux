# File: time_test.flux
# Tests: Time module from std.time

import std.time;

func main() {
    # Get current timestamp
    float now = Time.now();
    print("Timestamp > 0: ${now > 0}");

    # Get millisecond timestamp
    long nowMs = Time.nowMs();
    print("Ms timestamp > 0: ${nowMs > 0}");

    # Format current time
    string formatted = Time.format(now, "%Y-%m-%d");
    print("Formatted date: $formatted");

    # Get year
    int year = Time.year(now);
    print("Year >= 2024: ${year >= 2024}");
}
