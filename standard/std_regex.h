#pragma once
#include "../src/environment.h"

class Interpreter;

// Registers std.regex module: Regex(pattern), match(), search(), replace(), split()
void registerStdRegex(std::shared_ptr<Environment> env, Interpreter& interp);
