#pragma once
#include "../src/environment.h"

class Interpreter;

// Registers std.json module: JSON.parse(), JSON.stringify()
void registerStdJSON(std::shared_ptr<Environment> env, Interpreter& interp);
