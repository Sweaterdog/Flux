# File: crypto_test.flux
# Tests: Crypto hashing and Base64 encoding from std.crypto

import std.crypto;

func main() {
    # SHA-256
    string hash = Crypto.sha256("hello");
    print("SHA-256 length: ${hash.length}");
    print("SHA-256 of hello: $hash");

    # MD5
    string md5hash = Crypto.md5("hello");
    print("MD5 length: ${md5hash.length}");
    print("MD5 of hello: $md5hash");

    # Base64 encode/decode
    string encoded = Base64.encode("Hello, Flux!");
    print("Base64 encoded: $encoded");

    string decoded = Base64.decode(encoded);
    print("Base64 decoded: $decoded");
}
