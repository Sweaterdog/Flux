#include "std_crypto.h"
#include "../src/interpreter.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <vector>

// ============================================================================
// std.crypto — Hashing and encoding utilities
//
// Provides:
//   Crypto.sha256(str)     -> string (hex)
//   Crypto.md5(str)        -> string (hex)
//   Base64.encode(str)     -> string
//   Base64.decode(str)     -> string
//
// These are pure C++ implementations (no external deps).
// ============================================================================

// ==========================================================================
// SHA-256 implementation (FIPS 180-4)
// ==========================================================================
namespace {

static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

std::string sha256(const std::string& input) {
    // Pre-processing: padding
    uint64_t bitLen = input.size() * 8;
    std::vector<uint8_t> msg(input.begin(), input.end());
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; i--) msg.push_back((uint8_t)(bitLen >> (i * 8)));

    // Initial hash values
    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

    // Process each 512-bit chunk
    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)msg[offset + i*4] << 24) |
                    ((uint32_t)msg[offset + i*4+1] << 16) |
                    ((uint32_t)msg[offset + i*4+2] << 8) |
                    ((uint32_t)msg[offset + i*4+3]);
        }
        for (int i = 16; i < 64; i++) {
            w[i] = gamma1(w[i-2]) + w[i-7] + gamma0(w[i-15]) + w[i-16];
        }

        uint32_t a = h0, b = h1, c = h2, d = h3;
        uint32_t e = h4, f = h5, g = h6, h = h7;

        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + sigma1(e) + ch(e, f, g) + SHA256_K[i] + w[i];
            uint32_t t2 = sigma0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        h0 += a; h1 += b; h2 += c; h3 += d;
        h4 += e; h5 += f; h6 += g; h7 += h;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << h0 << std::setw(8) << h1
        << std::setw(8) << h2 << std::setw(8) << h3
        << std::setw(8) << h4 << std::setw(8) << h5
        << std::setw(8) << h6 << std::setw(8) << h7;
    return oss.str();
}

// ==========================================================================
// MD5 implementation (RFC 1321)
// ==========================================================================

static const uint32_t MD5_S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

static const uint32_t MD5_K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

inline uint32_t md5_rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

std::string md5(const std::string& input) {
    uint64_t bitLen = input.size() * 8;
    std::vector<uint8_t> msg(input.begin(), input.end());
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) msg.push_back(0x00);
    // Little-endian length
    for (int i = 0; i < 8; i++) msg.push_back((uint8_t)(bitLen >> (i * 8)));

    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;

    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++) {
            M[i] = ((uint32_t)msg[offset + i*4]) |
                    ((uint32_t)msg[offset + i*4+1] << 8) |
                    ((uint32_t)msg[offset + i*4+2] << 16) |
                    ((uint32_t)msg[offset + i*4+3] << 24);
        }

        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; i++) {
            uint32_t F, g;
            if (i < 16) {
                F = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                F = (D & B) | (~D & C);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                F = B ^ C ^ D;
                g = (3 * i + 5) % 16;
            } else {
                F = C ^ (B | ~D);
                g = (7 * i) % 16;
            }
            F = F + A + MD5_K[i] + M[g];
            A = D; D = C; C = B;
            B = B + md5_rotl(F, MD5_S[i]);
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    // Output (little-endian)
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    auto out = [&](uint32_t v) {
        for (int i = 0; i < 4; i++) {
            oss << std::setw(2) << ((v >> (i * 8)) & 0xFF);
        }
    };
    out(a0); out(b0); out(c0); out(d0);
    return oss.str();
}

// ==========================================================================
// Base64 encode/decode
// ==========================================================================

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::string& input) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out += BASE64_CHARS[(val >> valb) & 0x3F];
            valb -= 6;
        }
    }
    if (valb > -6) out += BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F];
    while (out.size() % 4) out += '=';
    return out;
}

std::string base64Decode(const std::string& input) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(unsigned char)BASE64_CHARS[i]] = i;

    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out += (char)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}

} // anonymous namespace

// ============================================================================
// Registration
// ============================================================================

void registerStdCrypto(std::shared_ptr<Environment> env, Interpreter& interp) {

    // Crypto namespace
    auto cryptoClass = std::make_shared<FluxClass>();
    cryptoClass->name = "Crypto";
    auto cryptoObj = std::make_shared<FluxObject>();
    cryptoObj->classDef = cryptoClass;

    // Crypto.sha256(str) -> string (hex)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Crypto.sha256 requires a string argument");
            return Value::fromString(sha256(args[0].toString()));
        };
        cryptoObj->fields["sha256"] = fn;
    }

    // Crypto.md5(str) -> string (hex)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Crypto.md5 requires a string argument");
            return Value::fromString(md5(args[0].toString()));
        };
        cryptoObj->fields["md5"] = fn;
    }

    Value cryptoVal;
    cryptoVal.type = ValueType::OBJECT;
    cryptoVal.objectVal = cryptoObj;
    env->define("Crypto", cryptoVal, "object");

    // Base64 namespace
    auto b64Class = std::make_shared<FluxClass>();
    b64Class->name = "Base64";
    auto b64Obj = std::make_shared<FluxObject>();
    b64Obj->classDef = b64Class;

    // Base64.encode(str) -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Base64.encode requires a string argument");
            return Value::fromString(base64Encode(args[0].toString()));
        };
        b64Obj->fields["encode"] = fn;
    }

    // Base64.decode(str) -> string
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "Base64.decode requires a string argument");
            return Value::fromString(base64Decode(args[0].toString()));
        };
        b64Obj->fields["decode"] = fn;
    }

    Value b64Val;
    b64Val.type = ValueType::OBJECT;
    b64Val.objectVal = b64Obj;
    env->define("Base64", b64Val, "object");
}
