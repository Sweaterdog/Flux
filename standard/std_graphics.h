#pragma once
#include "../src/environment.h"

class Interpreter;

// Registers std.graphics module: Window creation, rendering (SDL2/GLFW backends)
void registerStdGraphics(std::shared_ptr<Environment> env, Interpreter& interp);
