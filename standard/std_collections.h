#pragma once

#include "../src/value.h"
#include "../src/environment.h"
#include <memory>

// ============================================================================
// std.collections — Map<K,V>, Stack<T>, Queue<T>
// ============================================================================

class Interpreter;

void registerStdCollections(std::shared_ptr<Environment> env, Interpreter& interp);
