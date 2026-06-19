#pragma once
#include "../src/environment.h"

class Interpreter;

// Registers std.crypto module: Crypto.sha256(), Crypto.md5(), Base64.encode/decode
void registerStdCrypto(std::shared_ptr<Environment> env, Interpreter& interp);
