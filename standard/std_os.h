#pragma once
#include "../src/environment.h"

class Interpreter;

// Registers std.os module: OS.exec(), OS.env(), OS.cwd(), OS.pid(), etc.
void registerStdOS(std::shared_ptr<Environment> env, Interpreter& interp);
