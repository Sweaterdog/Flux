#pragma once

#include "../src/value.h"
#include "../src/environment.h"
#include <memory>

// ============================================================================
// std.net — Networking (HTTP client, TCP/UDP sockets)
//
// HTTP (requires libcurl):
//   HttpClient class with get/post/put/delete methods
//   Response object with statusCode, body, headers
//
// Sockets:
//   Socket class with connect/bind/listen/accept/write/readLine/close
// ============================================================================

class Interpreter;

void registerStdNet(std::shared_ptr<Environment> env, Interpreter& interp);
