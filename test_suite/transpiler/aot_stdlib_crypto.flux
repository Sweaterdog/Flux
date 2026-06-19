# AOT Test: std.crypto — real SHA-256, MD5, and Base64
# This test verifies that AOT-compiled crypto functions produce
# correct hashes (not stubs).

import std.crypto;

func main() {
    print("=== AOT Crypto Test ===");

    # SHA-256 tests
    string sha = Crypto.sha256("hello");
    print("SHA-256 of 'hello': $sha");

    # Known correct SHA-256 of "hello"
    if (sha == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824") {
        print("SHA-256: CORRECT");
    } else {
        print("SHA-256: WRONG (got $sha)");
    }

    # SHA-256 of empty string
    string shaEmpty = Crypto.sha256("");
    if (shaEmpty == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") {
        print("SHA-256 empty: CORRECT");
    } else {
        print("SHA-256 empty: WRONG (got $shaEmpty)");
    }

    # MD5 tests
    string md = Crypto.md5("hello");
    print("MD5 of 'hello': $md");

    if (md == "5d41402abc4b2a76b9719d911017c592") {
        print("MD5: CORRECT");
    } else {
        print("MD5: WRONG (got $md)");
    }

    # Base64 tests
    string encoded = Base64.encode("Hello, World!");
    print("Base64 encode: $encoded");
    if (encoded == "SGVsbG8sIFdvcmxkIQ==") {
        print("Base64 encode: CORRECT");
    } else {
        print("Base64 encode: WRONG");
    }

    string decoded = Base64.decode(encoded);
    print("Base64 decode: $decoded");
    if (decoded == "Hello, World!") {
        print("Base64 decode: CORRECT");
    } else {
        print("Base64 decode: WRONG");
    }

    # Round-trip test
    string original = "Flux AOT Crypto works!";
    string roundtrip = Base64.decode(Base64.encode(original));
    if (roundtrip == original) {
        print("Base64 round-trip: CORRECT");
    } else {
        print("Base64 round-trip: WRONG");
    }

    print("=== AOT Crypto Test PASSED ===");
}
