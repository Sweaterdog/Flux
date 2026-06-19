# File: control_flow.flux
# Tests: if/elif/else, for, while, do-while, switch, break, continue

func main() {
    # if / elif / else
    int score = 75;
    if (score >= 90) {
        print("Grade: A");
    } elif (score >= 80) {
        print("Grade: B");
    } elif (score >= 70) {
        print("Grade: C");
    } else {
        print("Grade: F");
    }

    # For loop
    print("For loop 0-4:");
    for (int i = 0; i < 5; i++) {
        print("  $i");
    }

    # While loop
    print("While loop countdown:");
    int n = 3;
    while (n > 0) {
        print("  $n");
        n--;
    }

    # Do-while loop
    print("Do-while:");
    int count = 0;
    do {
        count++;
        print("  attempt $count");
    } while (count < 3);

    # Break
    print("Break at 3:");
    for (int i = 0; i < 10; i++) {
        if (i == 3) {
            break;
        }
        print("  $i");
    }

    # Continue
    print("Continue (odd only):");
    for (int i = 0; i < 6; i++) {
        if (i % 2 == 0) {
            continue;
        }
        print("  $i");
    }

    # Switch
    int status = 404;
    switch (status) {
        case 200:
            print("200 OK");
            break;
        case 404:
            print("404 Not Found");
            break;
        case 500:
            print("500 Server Error");
            break;
        default:
            print("Unknown status");
    }
}
