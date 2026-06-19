#pragma once

#include "../src/value.h"
#include "../src/environment.h"
#include <memory>

// ============================================================================
// std.io — File system and console I/O
//
// Provides:
//   fs.read(path) -> string
//   fs.write(path, data) -> void
//   fs.append(path, data) -> void
//   fs.exists(path) -> bool
//   fs.delete(path) -> void
//   fs.list(dir) -> List<string>
//   fs.mkdir(path) -> void
//   fs.size(path) -> int
// ============================================================================

class Interpreter;

void registerStdIO(std::shared_ptr<Environment> env, Interpreter& interp);
