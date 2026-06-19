#pragma once
#include "../src/environment.h"

class Interpreter;

// Registers std.time module: Time.now(), Time.format(), Time.parse(), etc.
void registerStdTime(std::shared_ptr<Environment> env, Interpreter& interp);
