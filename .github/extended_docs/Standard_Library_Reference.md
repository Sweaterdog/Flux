# Flux Standard Library Reference

> Complete API documentation for Flux's built-in standard library modules.
> Import a module with `import std.<module>` to access its functionality.

---

## Table of Contents

1. [std.io — File System](#stdio--file-system)
2. [std.sys — System, Threading & Signals](#stdsys--system-threading--signals)
3. [std.time — Time & Timers](#stdtime--time--timers)
4. [std.net — Networking (HTTP & Sockets)](#stdnet--networking-http--sockets)
5. [std.collections — Map, Stack & Queue](#stdcollections--map-stack--queue)
6. [std.json — JSON Parsing & Serialization](#stdjson--json-parsing--serialization)
7. [std.crypto — Hashing & Encoding](#stdcrypto--hashing--encoding)
8. [std.regex — Regular Expressions](#stdregex--regular-expressions)
9. [std.os — Operating System Interface](#stdos--operating-system-interface)
10. [std.gpu — GPU Compute](#stdgpu--gpu-compute)
11. [std.graphics — Windowing & 2D Drawing](#stdgraphics--windowing--2d-drawing)

---

## std.io — File System

**Import:** `import std.io`

Provides file system operations through the global `fs` object.

### fs.read(path: string) → string

Reads the entire contents of a file and returns it as a string.

```flux
import std.io
string content = fs.read("/etc/hostname")
print(content)
```

### fs.write(path: string, data: string) → nil

Writes data to a file, creating it if it doesn't exist, overwriting if it does.

```flux
fs.write("output.txt", "Hello, Flux!")
```

### fs.append(path: string, data: string) → nil

Appends data to the end of a file.

```flux
fs.append("log.txt", "New log entry\n")
```

### fs.exists(path: string) → bool

Returns `true` if the file or directory exists at the given path.

```flux
if fs.exists("config.json") {
    string cfg = fs.read("config.json")
}
```

### fs.delete(path: string) → nil

Deletes a file. Also available as `fs.remove()`.

```flux
fs.delete("temp.txt")
// or equivalently:
fs.remove("temp.txt")
```

### fs.list(dir: string) → list\<string\>

Returns a list of filenames in the given directory.

```flux
list<string> files = fs.list("/home/user/documents")
for file in files {
    print(file)
}
```

### fs.mkdir(path: string) → nil

Creates a directory (and any necessary parent directories).

```flux
fs.mkdir("/tmp/flux_app/data")
```

### fs.size(path: string) → long

Returns the size of a file in bytes.

```flux
long bytes = fs.size("large_file.bin")
print("File is ${bytes} bytes")
```

### fs.copy(src: string, dst: string) → nil

Copies a file from `src` to `dst`, overwriting if the destination exists.

```flux
fs.copy("original.txt", "backup.txt")
```

### fs.rename(oldPath: string, newPath: string) → nil

Renames or moves a file.

```flux
fs.rename("old_name.txt", "new_name.txt")
```

---

## std.sys — System, Threading & Signals

**Import:** `import std.sys`

Provides system utilities, multi-threading, mutexes, and POSIX signal handling.

### sys Object

#### sys.platform → string (property)

The current operating system: `"linux"`, `"macos"`, or `"windows"`.

```flux
print("Running on: " + sys.platform)
```

#### sys.arch → string (property)

The CPU architecture: `"x86_64"`, `"aarch64"`, or `"arm"`.

#### sys.args → list\<string\> (property)

Command-line arguments passed to the program (excluding the interpreter/binary name).

```flux
list<string> args = sys.args
for arg in args {
    print("Arg: " + arg)
}
```

#### sys.time() → long

Returns the current Unix timestamp in milliseconds.

```flux
long ms = sys.time()
```

#### sys.env(key: string) → string

Returns the value of an environment variable, or an empty string if not set.

```flux
string home = sys.env("HOME")
string path = sys.env("PATH")
```

#### sys.exit(code?: int) → never

Terminates the program with the given exit code (default: 0).

```flux
sys.exit(1)  // exit with error
```

#### sys.cpuCount() → int

Returns the number of hardware threads available.

```flux
int cores = sys.cpuCount()
print("Available threads: ${cores}")
```

### Threading

#### thread.run(func: function, args...) → ThreadHandle

Spawns a new thread to execute `func` with the given arguments.
Returns a `ThreadHandle` that can be joined to wait for completion.

```flux
func worker(int n) {
    print("Working on ${n}")
}

object handle = thread.run(worker, 42)
handle.join()  // wait for thread to finish
```

#### thread.sleep(ms: int) → nil

Pauses the current thread for the given number of milliseconds.

```flux
thread.sleep(1000)  // sleep 1 second
```

#### ThreadHandle.join() → nil

Blocks until the thread finishes execution.

#### ThreadHandle.result() → any

Returns the value returned by the thread function (after `join()`).

#### ThreadHandle.error() → string

Returns the error message if the thread threw an exception, or an empty string.

### Mutex

Mutex provides mutual exclusion for thread synchronization.

#### Mutex() → Mutex

Creates a new mutex.

```flux
object m = Mutex()
```

#### Mutex.lock() → nil

Acquires the mutex. Blocks if another thread holds it.

#### Mutex.unlock() → nil

Releases the mutex.

#### Mutex.tryLock() → bool

Attempts to acquire the mutex without blocking. Returns `true` if successful.

```flux
object m = Mutex()
m.lock()
// ... critical section ...
m.unlock()
```

### Signal Handling

The `Signal` object provides POSIX signal handling.

#### Signal.handle(signum: int, func: function) → nil

Registers a callback function to handle the given signal.

```flux
func onInterrupt() {
    print("Caught SIGINT!")
}
Signal.handle(Signal.SIGINT, onInterrupt)
```

#### Signal.raise(signum: int) → nil

Sends a signal to the current process.

#### Signal.ignore(signum: int) → nil

Tells the OS to ignore the given signal.

#### Signal.reset(signum: int) → nil

Resets a signal to its default behavior, removing any custom handler.

#### Signal Constants

| Constant | Value | Description |
|---|---|---|
| `Signal.SIGINT` | 2 | Interrupt (Ctrl+C) |
| `Signal.SIGTERM` | 15 | Termination request |
| `Signal.SIGABRT` | 6 | Abort |
| `Signal.SIGFPE` | 8 | Floating-point exception |
| `Signal.SIGSEGV` | 11 | Segmentation fault |
| `Signal.SIGHUP` | 1 | Hangup (Unix) |
| `Signal.SIGUSR1` | 10 | User-defined 1 (Unix) |
| `Signal.SIGUSR2` | 12 | User-defined 2 (Unix) |
| `Signal.SIGPIPE` | 13 | Broken pipe (Unix) |
| `Signal.SIGALRM` | 14 | Alarm timer (Unix) |
| `Signal.SIGCHLD` | 17 | Child process status (Unix) |

---

## std.time — Time & Timers

**Import:** `import std.time`

Provides time retrieval, formatting, parsing, and high-resolution timing.

### Time Object

#### Time.now() → float

Returns the current Unix timestamp in seconds (as a float for sub-second precision potential).

```flux
float timestamp = Time.now()
print("Unix time: ${timestamp}")
```

#### Time.nowMs() → long

Returns the current Unix timestamp in milliseconds.

```flux
long ms = Time.nowMs()
```

#### Time.format(timestamp: number, fmt: string) → string

Formats a Unix timestamp using `strftime`-style format codes.

```flux
string dateStr = Time.format(Time.now(), "%Y-%m-%d %H:%M:%S")
print(dateStr)  // e.g. "2025-01-15 14:30:00"
```

Common format codes: `%Y` (year), `%m` (month), `%d` (day), `%H` (hour 24h), `%M` (minute), `%S` (second), `%A` (weekday name).

#### Time.parse(str: string, fmt: string) → int

Parses a date string using the given format and returns a Unix timestamp.

```flux
int ts = Time.parse("2025-01-15", "%Y-%m-%d")
```

#### Time.year(ts: number) → int

Extracts the year from a Unix timestamp.

#### Time.month(ts: number) → int

Extracts the month (1–12) from a Unix timestamp.

#### Time.day(ts: number) → int

Extracts the day of the month (1–31) from a Unix timestamp.

#### Time.hour(ts: number) → int

Extracts the hour (0–23) from a Unix timestamp.

#### Time.minute(ts: number) → int

Extracts the minute (0–59) from a Unix timestamp.

#### Time.second(ts: number) → int

Extracts the second (0–59) from a Unix timestamp.

#### Time.dayOfWeek(ts: number) → int

Returns the day of the week (0 = Sunday, 6 = Saturday).

#### Time.elapsed(startMs: number) → float

Returns the number of seconds elapsed since `startMs` (in milliseconds).

```flux
long start = Time.nowMs()
// ... do work ...
float secs = Time.elapsed(start)
print("Took ${secs} seconds")
```

### Timer

A high-resolution stopwatch for benchmarking.

#### Timer() → Timer

Creates a new timer.

#### Timer.start() → nil

Starts (or restarts) the timer.

#### Timer.stop() → nil

Stops the timer.

#### Timer.elapsed() → float

Returns the elapsed time in seconds between start and stop (microsecond precision).

```flux
object timer = Timer()
timer.start()
// ... expensive operation ...
timer.stop()
print("Elapsed: ${timer.elapsed()} seconds")
```

---

## std.net — Networking (HTTP & Sockets)

**Import:** `import std.net`

Provides HTTP client and TCP socket functionality.

### HttpClient

An HTTP client for making web requests. Requires libcurl at build time.

#### HttpClient() → HttpClient

Creates a new HTTP client.

```flux
object http = HttpClient()
```

#### HttpClient.setHeader(key: string, value: string) → nil

Sets a header for subsequent requests.

```flux
http.setHeader("Content-Type", "application/json")
http.setHeader("Authorization", "Bearer token123")
```

#### HttpClient.get(url: string) → Response

Performs an HTTP GET request.

```flux
object resp = http.get("https://api.example.com/data")
print("Status: ${resp.statusCode}")
print("Body: ${resp.body}")
```

#### HttpClient.post(url: string, body?: string) → Response

Performs an HTTP POST request with an optional body.

```flux
object resp = http.post("https://api.example.com/items", "{\"name\":\"test\"}")
```

#### HttpClient.put(url: string, body?: string) → Response

Performs an HTTP PUT request.

#### HttpClient.httpDelete(url: string) → Response

Performs an HTTP DELETE request.

### Response Object

Returned by all HTTP methods.

| Field | Type | Description |
|---|---|---|
| `statusCode` | int | HTTP status code (200, 404, etc.) |
| `body` | string | Response body content |
| `headers` | object | Response headers as key-value pairs |

### Socket

TCP socket for low-level network communication.

#### Socket() → Socket

Creates a new TCP socket.

```flux
object sock = Socket()
```

#### Socket.connect(host: string, port: int) → nil

Connects to a remote host.

```flux
sock.connect("example.com", 80)
```

#### Socket.bind(port: int) → nil

Binds the socket to a local port (for servers).

#### Socket.listen(backlog?: int) → nil

Begins listening for incoming connections. Default backlog is 5.

#### Socket.accept() → Socket

Accepts an incoming connection. Returns a new socket for the client connection.
The returned socket has `write()`, `readLine()`, `read()`, and `close()` methods.

```flux
object server = Socket()
server.bind(8080)
server.listen(10)
object client = server.accept()
string request = client.readLine()
client.write("HTTP/1.1 200 OK\r\n\r\nHello!")
client.close()
```

#### Socket.write(data: string) → int

Sends data and returns the number of bytes written.

#### Socket.readLine() → string

Reads a line of text (up to `\n`).

#### Socket.read(maxBytes?: int) → string

Reads up to `maxBytes` of data (default: 4096).

#### Socket.close() → nil

Closes the socket.

### Protocol Enum

| Value | Int | Description |
|---|---|---|
| `Protocol.TCP` | 0 | TCP protocol |
| `Protocol.UDP` | 1 | UDP protocol (reserved) |

---

## std.collections — Map, Stack & Queue

**Import:** `import std.collections`

Provides hash maps, stacks, and queues.

### Map

An ordered key-value store with string keys.

#### Map() → Map

Creates a new empty map.

```flux
object m = Map()
m.put("name", "Alice")
m.put("age", "30")
```

#### Map.put(key: string, value: any) → nil

Inserts or updates a key-value pair.

#### Map.get(key: string) → any

Returns the value for the given key, or `nil` if not found.

```flux
string name = m.get("name")  // "Alice"
```

#### Map.hasKey(key: string) → bool

Returns `true` if the key exists in the map.

```flux
if m.hasKey("name") {
    print("Found: " + m.get("name"))
}
```

#### Map.remove(key: string) → nil

Removes a key-value pair.

#### Map.keys() → list\<string\>

Returns all keys as a list.

#### Map.values() → list\<any\>

Returns all values as a list.

#### Map.length() → int

Returns the number of entries. Also available as `Map.size()`.

### Stack

A last-in, first-out (LIFO) data structure.

#### Stack() → Stack

Creates a new empty stack.

#### Stack.push(item: any) → nil

Pushes an item onto the top of the stack.

#### Stack.pop() → any

Removes and returns the top item.

#### Stack.peek() → any

Returns the top item without removing it.

#### Stack.size() → int

Returns the number of items.

#### Stack.isEmpty() → bool

Returns `true` if the stack has no items.

```flux
object stk = Stack()
stk.push("first")
stk.push("second")
print(stk.pop())   // "second"
print(stk.peek())  // "first"
```

### Queue

A first-in, first-out (FIFO) data structure.

#### Queue() → Queue

Creates a new empty queue.

#### Queue.enqueue(item: any) → nil

Adds an item to the back of the queue.

#### Queue.dequeue() → any

Removes and returns the front item.

#### Queue.peek() → any

Returns the front item without removing it.

#### Queue.size() → int

Returns the number of items.

#### Queue.isEmpty() → bool

Returns `true` if the queue has no items.

```flux
object q = Queue()
q.enqueue("A")
q.enqueue("B")
print(q.dequeue())  // "A"
```

---

## std.json — JSON Parsing & Serialization

**Import:** `import std.json`

Provides JSON parsing and stringification through the global `JSON` object.

### JSON.parse(str: string) → any

Parses a JSON string and returns a Flux value. Objects become Flux objects with fields, arrays become lists, and primitives map to their Flux equivalents.

```flux
object data = JSON.parse("{\"name\": \"Alice\", \"age\": 30}")
print(data.name)  // "Alice"
print(data.age)   // 30
```

### JSON.stringify(value: any, indent?: int) → string

Serializes a Flux value to a JSON string. The optional `indent` parameter controls pretty-printing.

```flux
string json = JSON.stringify(data)
// {"name":"Alice","age":30}

string pretty = JSON.stringify(data, 2)
// {
//   "name": "Alice",
//   "age": 30
// }
```

---

## std.crypto — Hashing & Encoding

**Import:** `import std.crypto`

Provides cryptographic hashing and Base64 encoding/decoding.

### Crypto Object

#### Crypto.sha256(data: string) → string

Returns the SHA-256 hash of the input as a lowercase hexadecimal string.

```flux
string hash = Crypto.sha256("hello world")
print(hash)  // "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"
```

#### Crypto.md5(data: string) → string

Returns the MD5 hash of the input as a lowercase hexadecimal string.

```flux
string hash = Crypto.md5("hello")
print(hash)  // "5d41402abc4b2a76b9719d911017c592"
```

### Base64 Object

#### Base64.encode(data: string) → string

Encodes a string to Base64.

```flux
string encoded = Base64.encode("Hello, World!")
print(encoded)  // "SGVsbG8sIFdvcmxkIQ=="
```

#### Base64.decode(data: string) → string

Decodes a Base64 string back to plain text.

```flux
string decoded = Base64.decode("SGVsbG8sIFdvcmxkIQ==")
print(decoded)  // "Hello, World!"
```

---

## std.regex — Regular Expressions

**Import:** `import std.regex`

Provides regular expression matching, searching, replacing, and splitting.

### Regex(pattern: string) → Regex

Creates a new regex object from the given pattern string. Uses ECMAScript regex syntax.

```flux
object re = Regex("[A-Za-z]+")
```

### Regex.pattern → string (property)

The original pattern string.

### Regex.match(str: string) → bool

Returns `true` if the **entire** string matches the pattern.

```flux
object re = Regex("[0-9]+")
print(re.match("12345"))   // true
print(re.match("abc123"))  // false (not a full match)
```

### Regex.search(str: string) → string | nil

Returns the first substring that matches the pattern, or `nil` if no match.

```flux
object re = Regex("[0-9]+")
string found = re.search("abc 42 def")
print(found)  // "42"
```

### Regex.findAll(str: string) → list\<string\>

Returns all non-overlapping matches as a list.

```flux
object re = Regex("[0-9]+")
list<string> nums = re.findAll("a1 b22 c333")
// nums = ["1", "22", "333"]
```

### Regex.replace(str: string, replacement: string) → string

Replaces all matches with the replacement string.

```flux
object re = Regex("[0-9]+")
string result = re.replace("abc 42 def 99", "NUM")
print(result)  // "abc NUM def NUM"
```

### Regex.split(str: string) → list\<string\>

Splits the string at each match of the pattern.

```flux
object re = Regex("[,;\\s]+")
list<string> parts = re.split("a, b; c d")
// parts = ["a", "b", "c", "d"]
```

### Regex.groups(str: string) → list\<string\>

Returns the captured groups from the first match (excludes group 0 / full match).

```flux
object re = Regex("(\\w+)@(\\w+)")
list<string> groups = re.groups("user@host")
// groups = ["user", "host"]
```

---

## std.os — Operating System Interface

**Import:** `import std.os`

Provides OS-level operations through the global `OS` object.

### OS.platform → string (property)

The current operating system: `"linux"`, `"macos"`, or `"windows"`.

### OS.exec(command: string) → string

Executes a shell command and returns its stdout output.

```flux
string files = OS.exec("ls -la")
print(files)
```

### OS.execStatus(command: string) → int

Executes a shell command and returns its exit code.

```flux
int status = OS.execStatus("gcc -o test test.c")
if status == 0 {
    print("Compilation succeeded")
}
```

### OS.env(key: string) → string

Returns the value of an environment variable, or an empty string if not set.

### OS.setEnv(key: string, value: string) → nil

Sets an environment variable for the current process.

```flux
OS.setEnv("MY_VAR", "hello")
```

### OS.cwd() → string

Returns the current working directory.

### OS.chdir(path: string) → nil

Changes the current working directory.

```flux
OS.chdir("/tmp")
print(OS.cwd())  // "/tmp"
```

### OS.pid() → int

Returns the current process ID.

### OS.hostname() → string

Returns the system hostname.

### OS.username() → string

Returns the current username.

### OS.tempDir() → string

Returns the system temporary directory path (e.g., `"/tmp"` on Linux).

---

## std.gpu — GPU Compute

**Import:** `import std.gpu`

Provides GPU compute abstraction with CUDA, ROCm, and CPU fallback backends.

### GPU Object

#### GPU.available → bool (property)

`true` if a GPU compute backend is available.

#### GPU.isAvailable() → bool

Function form of `GPU.available`.

#### GPU.backend → string (property)

The active backend: `"cuda"`, `"rocm"`, or `"none"`.

#### GPU.deviceCount() → int

Returns the number of GPU devices available.

#### GPU.deviceName(id?: int) → string

Returns the name of the GPU device at the given index (default: 0).

```flux
if GPU.available {
    print("GPU: " + GPU.deviceName())
    print("Backend: " + GPU.backend)
}
```

#### GPU.allocate(sizeInFloats: int) → GPUBuffer

Allocates a GPU buffer for the given number of float elements.

#### GPU.memcpyToDevice(buf: GPUBuffer, data: list\<float\>) → nil

Copies data from CPU to GPU.

#### GPU.memcpyToHost(buf: GPUBuffer) → list\<float\>

Copies data from GPU back to CPU.

#### GPU.free(buf: GPUBuffer) → nil

Frees a GPU buffer.

#### GPU.sync() → nil

Synchronizes CPU with GPU (waits for all GPU operations to complete).

### GPUBuffer

Returned by `GPU.allocate()`.

| Field | Type | Description |
|---|---|---|
| `size` | int | Number of float elements |
| `_ptr` | long | Internal device pointer |
| `_backend` | string | Backend that allocated the buffer |

---

## std.graphics — Windowing & 2D Drawing

**Import:** `import std.graphics`

Provides window management and 2D drawing. Uses SDL2 as the primary backend, with GLFW and stub fallbacks.

### Window(title: string, width: int, height: int) → Window

Creates a new window with the given title and dimensions.

```flux
Window win = Window("My App", 800, 600)
```

#### Window.backend → string (property)

The active graphics backend: `"sdl2"`, `"glfw"`, or `"stub"`.

#### Window.isOpen() → bool

Returns `true` if the window is still open.

#### Window.pollEvents() → nil

Processes pending OS events (keyboard, mouse, close button). Must be called each frame.

#### Window.clear(r: int, g: int, b: int) → nil

Clears the window with the given RGB color (0–255 each).

#### Window.present() → nil

Swaps the back buffer to display the rendered frame.

#### Window.close() → nil

Closes the window.

#### Window.setTitle(title: string) → nil

Changes the window title.

#### Window.resize(width: int, height: int) → nil

Resizes the window.

### 2D Drawing Methods

All drawing methods are called on a `Window` object. Colors are RGB (0–255).

#### Window.drawPixel(x: int, y: int, r: int, g: int, b: int) → nil

Draws a single pixel.

#### Window.drawLine(x1: int, y1: int, x2: int, y2: int, r: int, g: int, b: int) → nil

Draws a line between two points.

#### Window.drawRect(x: int, y: int, w: int, h: int, r: int, g: int, b: int) → nil

Draws a rectangle outline.

#### Window.fillRect(x: int, y: int, w: int, h: int, r: int, g: int, b: int) → nil

Draws a filled rectangle. An optional 8th alpha parameter (0–255) is supported.

#### Window.drawCircle(cx: int, cy: int, radius: int, r: int, g: int, b: int) → nil

Draws a circle outline.

#### Window.fillCircle(cx: int, cy: int, radius: int, r: int, g: int, b: int) → nil

Draws a filled circle.

#### Window.drawTriangle(x1: int, y1: int, x2: int, y2: int, x3: int, y3: int, r: int, g: int, b: int) → nil

Draws a triangle outline.

#### Window.setBlendMode(enable: bool) → nil

Enables or disables alpha blending.

### Input Object

The global `Input` object provides keyboard and mouse state.

#### Input.keyPressed(key: string) → bool

Returns `true` if the given key is currently pressed. Key names match SDL2 scancodes (e.g., `"A"`, `"Space"`, `"Escape"`, `"Left"`, `"Right"`).

#### Input.mouseX() → int

Returns the current mouse X position within the window.

#### Input.mouseY() → int

Returns the current mouse Y position within the window.

#### Input.mouseDown(button: int) → bool

Returns `true` if the given mouse button is pressed (1 = left, 2 = middle, 3 = right).

### Render Loop Example

```flux
import std.graphics

Window win = Window("Demo", 640, 480)
int frame = 0

while win.isOpen() {
    win.pollEvents()
    win.clear(30, 30, 30)

    // Draw a moving rectangle
    win.fillRect(frame % 600, 200, 40, 40, 255, 100, 50)

    // Draw UI elements
    win.drawRect(10, 10, 200, 30, 200, 200, 200)
    win.fillCircle(320, 240, 50, 100, 200, 255)

    win.present()
    frame = frame + 1
}
```

---

## AOT Compilation Notes

All standard library modules support AOT (Ahead-of-Time) compilation via `flux compile`. When compiling:

- **std.io** — Full functionality. Uses `<filesystem>` and `<fstream>`. Link with `-lstdc++fs` if needed.
- **std.sys** — Full signal, threading, and system info support. Links `-lpthread` automatically.
- **std.time** — Full functionality using `<chrono>` and POSIX time functions.
- **std.net** — Stub implementations for HTTP (full functionality requires `-lcurl`). Sockets are stubs in AOT.
- **std.collections** — Full C++ template implementations for Map, Stack, and Queue.
- **std.json** — Stub for AOT (full parser available in JIT mode).
- **std.crypto** — Stub for AOT (full SHA-256/MD5/Base64 available in JIT mode).
- **std.regex** — Full functionality using `<regex>`.
- **std.os** — Full functionality using POSIX APIs.
- **std.gpu** — Stub for AOT (full CUDA/ROCm support available in JIT mode with proper drivers).
- **std.graphics** — Stub for AOT (full SDL2/GLFW rendering available in JIT mode).

To compile a Flux program with stdlib:

```bash
flux compile myprogram.flux -o myprogram
./myprogram
```
