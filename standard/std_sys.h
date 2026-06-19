#pragma once

#include "../src/value.h"
#include "../src/environment.h"
#include <memory>

// ============================================================================
// std.sys — Threads, mutex, system utilities
// ============================================================================

class Interpreter;

void registerStdSys(std::shared_ptr<Environment> env, Interpreter& interp);
