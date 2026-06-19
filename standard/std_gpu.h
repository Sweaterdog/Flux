#pragma once
#include "../src/environment.h"

class Interpreter;

// Registers std.gpu module: GPU compute via CUDA/ROCm (stubs when unavailable)
void registerStdGPU(std::shared_ptr<Environment> env, Interpreter& interp);
