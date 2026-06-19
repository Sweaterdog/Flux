#include "std_net.h"
#include "../src/interpreter.h"
#include <fstream>

#ifdef FLUX_HAS_CURL
#include <curl/curl.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <map>

// ============================================================================
// std.net — HTTP Client (libcurl) and TCP Sockets
// ============================================================================

#ifdef FLUX_HAS_CURL

// Callback for libcurl to write response body
static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Callback for libcurl to capture response headers
static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userp) {
    size_t totalSize = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userp);
    std::string header(buffer, totalSize);
    size_t colon = header.find(':');
    if (colon != std::string::npos) {
        std::string key = header.substr(0, colon);
        std::string val = header.substr(colon + 1);
        // Trim whitespace
        while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0, 1);
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
        (*headers)[key] = val;
    }
    return totalSize;
}

// Create a Response FluxObject from HTTP results
static Value makeHttpResponse(int statusCode, const std::string& body,
                              const std::map<std::string, std::string>& headers) {
    auto respClass = std::make_shared<FluxClass>();
    respClass->name = "Response";
    auto respObj = std::make_shared<FluxObject>();
    respObj->classDef = respClass;
    respObj->fields["statusCode"] = Value::fromInt(statusCode);
    respObj->fields["body"] = Value::fromString(body);

    // Convert headers map to a Flux object
    auto hdrClass = std::make_shared<FluxClass>();
    hdrClass->name = "Headers";
    auto hdrObj = std::make_shared<FluxObject>();
    hdrObj->classDef = hdrClass;
    for (auto& [k, v] : headers) {
        hdrObj->fields[k] = Value::fromString(v);
    }
    Value hdrVal;
    hdrVal.type = ValueType::OBJECT;
    hdrVal.objectVal = hdrObj;
    respObj->fields["headers"] = hdrVal;

    Value result;
    result.type = ValueType::OBJECT;
    result.objectVal = respObj;
    return result;
}

// Perform an HTTP request using libcurl
static Value performCurlRequest(const std::string& method, const std::string& url,
                                const std::string& body,
                                const std::map<std::string, std::string>& reqHeaders) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw FluxException("NetworkError", "Failed to initialize HTTP client");
    }

    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Flux/0.1");

    // Set HTTP method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    // GET is the default

    // Set request headers
    struct curl_slist* headerList = nullptr;
    for (auto& [key, val] : reqHeaders) {
        std::string header = key + ": " + val;
        headerList = curl_slist_append(headerList, header.c_str());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (headerList) curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw FluxException("NetworkError", std::string("HTTP request failed: ") + curl_easy_strerror(res));
    }

    return makeHttpResponse((int)httpCode, responseBody, responseHeaders);
}

#endif // FLUX_HAS_CURL

// ============================================================================
// TCP Socket wrapper
// ============================================================================

struct FluxSocket {
    int fd = -1;
    bool connected = false;
    bool isServer = false;
};

// Store sockets keyed by a unique ID inside a static map
static std::map<int, FluxSocket> socketMap;
static int nextSocketId = 1;

void registerStdNet(std::shared_ptr<Environment> env, Interpreter& interp) {
#ifdef FLUX_HAS_CURL
    // Initialize libcurl globally (once)
    static bool curlInitialized = false;
    if (!curlInitialized) {
        curl_global_init(CURL_GLOBAL_ALL);
        curlInitialized = true;
    }
#endif

    // ========================================================================
    // HttpClient class — constructor creates an object with get/post/put/delete
    // ========================================================================
    {
        auto httpClass = std::make_shared<FluxClass>();
        httpClass->name = "HttpClient";

        // Register HttpClient as a class that can be instantiated with 'new'
        Value classVal;
        classVal.type = ValueType::CLASS_DEF;
        classVal.classVal = httpClass;

        // The HttpClient constructor creates an object with request methods
        httpClass->fieldTypes["_headers"] = "object";
        httpClass->fieldDefaults["_headers"] = Value::nil();

        // Build the init method that initializes headers storage
        // Instead, we use the CLASS_DEF and handle "new HttpClient()" in the interpreter
        // by creating an object with methods directly
        Value ctorFn;
        ctorFn.type = ValueType::NATIVE_FUNCTION;
        ctorFn.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            // Create a new HttpClient instance
            auto clientClass = std::make_shared<FluxClass>();
            clientClass->name = "HttpClient";
            auto clientObj = std::make_shared<FluxObject>();
            clientObj->classDef = clientClass;

            // Internal headers storage
            auto headersPtr = std::make_shared<std::map<std::string, std::string>>();

            // setHeader(key, value)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [headersPtr](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.size() < 2) throw FluxException("error", "setHeader requires key and value");
                    (*headersPtr)[args[0].toString()] = args[1].toString();
                    return Value::nil();
                };
                clientObj->fields["setHeader"] = fn;
            }

#ifdef FLUX_HAS_CURL
            // get(url) -> Response
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [headersPtr](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) throw FluxException("NetworkError", "get() requires a URL");
                    return performCurlRequest("GET", args[0].toString(), "", *headersPtr);
                };
                clientObj->fields["get"] = fn;
            }

            // post(url, body) -> Response
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [headersPtr](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) throw FluxException("NetworkError", "post() requires a URL");
                    std::string body = args.size() > 1 ? args[1].toString() : "";
                    return performCurlRequest("POST", args[0].toString(), body, *headersPtr);
                };
                clientObj->fields["post"] = fn;
            }

            // put(url, body) -> Response
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [headersPtr](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) throw FluxException("NetworkError", "put() requires a URL");
                    std::string body = args.size() > 1 ? args[1].toString() : "";
                    return performCurlRequest("PUT", args[0].toString(), body, *headersPtr);
                };
                clientObj->fields["put"] = fn;
            }

            // delete(url) -> Response
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [headersPtr](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) throw FluxException("NetworkError", "delete() requires a URL");
                    return performCurlRequest("DELETE", args[0].toString(), "", *headersPtr);
                };
                clientObj->fields["delete"] = fn;
            }

            // download(url, filePath) -> bool
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [headersPtr](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.size() < 2) throw FluxException("NetworkError", "download() requires url and filePath");
                    Value resp = performCurlRequest("GET", args[0].toString(), "", *headersPtr);
                    if (resp.type == ValueType::OBJECT && resp.objectVal) {
                        auto statusIt = resp.objectVal->fields.find("statusCode");
                        if (statusIt != resp.objectVal->fields.end()) {
                            int code = (int)statusIt->second.toNumber();
                            if (code >= 200 && code < 300) {
                                auto bodyIt = resp.objectVal->fields.find("body");
                                if (bodyIt != resp.objectVal->fields.end()) {
                                    std::ofstream out(args[1].toString(), std::ios::binary);
                                    if (!out.is_open()) return Value::fromBool(false);
                                    std::string body = bodyIt->second.toString();
                                    out.write(body.c_str(), body.size());
                                    return Value::fromBool(true);
                                }
                            }
                        }
                    }
                    return Value::fromBool(false);
                };
                clientObj->fields["download"] = fn;
            }
#else
            // Stubs when libcurl not available
            auto stubFn = [](const std::string& method) -> Value {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [method](Interpreter& interp, std::vector<Value> args) -> Value {
                    throw FluxException("NetworkError",
                        "HTTP " + method + " not available (Flux compiled without libcurl support)");
                    return Value::nil();
                };
                return fn;
            };
            clientObj->fields["get"] = stubFn("GET");
            clientObj->fields["post"] = stubFn("POST");
            clientObj->fields["put"] = stubFn("PUT");
            clientObj->fields["delete"] = stubFn("DELETE");
            clientObj->fields["download"] = stubFn("DOWNLOAD");
#endif

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = clientObj;
            return result;
        };

        // Register HttpClient as a callable constructor
        env->define("HttpClient", ctorFn, "native_function");
    }

    // ========================================================================
    // Socket class — TCP/UDP socket operations
    // ========================================================================
    {
        // Socket(protocol) constructor
        Value socketCtor;
        socketCtor.type = ValueType::NATIVE_FUNCTION;
        socketCtor.nativeFn = [](Interpreter& interp, std::vector<Value> args) -> Value {
            auto sockClass = std::make_shared<FluxClass>();
            sockClass->name = "Socket";
            auto sockObj = std::make_shared<FluxObject>();
            sockObj->classDef = sockClass;

            int id = nextSocketId++;
            FluxSocket& sock = socketMap[id];
            sock.fd = socket(AF_INET, SOCK_STREAM, 0);
            if (sock.fd < 0) {
                throw FluxException("NetworkError", "Failed to create socket");
            }

            sockObj->fields["_id"] = Value::fromInt(id);

            // connect(host, port)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.size() < 2) throw FluxException("NetworkError", "connect requires host and port");
                    auto it = socketMap.find(id);
                    if (it == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");

                    std::string host = args[0].toString();
                    int port = (int)args[1].toNumber();

                    struct hostent* he = gethostbyname(host.c_str());
                    if (!he) throw FluxException("NetworkError", "DNS lookup failed for: " + host);

                    struct sockaddr_in addr;
                    memset(&addr, 0, sizeof(addr));
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(port);
                    memcpy(&addr.sin_addr, he->h_addr, he->h_length);

                    if (::connect(it->second.fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                        throw FluxException("NetworkError", "Connection failed to " + host + ":" + std::to_string(port));
                    }
                    it->second.connected = true;
                    return Value::nil();
                };
                sockObj->fields["connect"] = fn;
            }

            // bind(port)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) throw FluxException("NetworkError", "bind requires a port");
                    auto it = socketMap.find(id);
                    if (it == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");

                    int port = (int)args[0].toNumber();
                    int opt = 1;
                    setsockopt(it->second.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

                    struct sockaddr_in addr;
                    memset(&addr, 0, sizeof(addr));
                    addr.sin_family = AF_INET;
                    addr.sin_addr.s_addr = INADDR_ANY;
                    addr.sin_port = htons(port);

                    if (::bind(it->second.fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                        throw FluxException("NetworkError", "Failed to bind to port " + std::to_string(port));
                    }
                    it->second.isServer = true;
                    return Value::nil();
                };
                sockObj->fields["bind"] = fn;
            }

            // listen(backlog)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id](Interpreter& interp, std::vector<Value> args) -> Value {
                    auto it = socketMap.find(id);
                    if (it == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");
                    int backlog = args.empty() ? 10 : (int)args[0].toNumber();
                    if (::listen(it->second.fd, backlog) < 0) {
                        throw FluxException("NetworkError", "Failed to listen");
                    }
                    return Value::nil();
                };
                sockObj->fields["listen"] = fn;
            }

            // accept() -> Socket (accepts an incoming connection, returns new socket)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id, &nextSocketId](Interpreter& interp, std::vector<Value> args) -> Value {
                    auto it = socketMap.find(id);
                    if (it == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");

                    struct sockaddr_in clientAddr;
                    socklen_t addrLen = sizeof(clientAddr);
                    int clientFd = ::accept(it->second.fd, (struct sockaddr*)&clientAddr, &addrLen);
                    if (clientFd < 0) {
                        throw FluxException("NetworkError", "Failed to accept connection");
                    }

                    // Create a new socket object for the client connection
                    int clientId = nextSocketId++;
                    FluxSocket& clientSock = socketMap[clientId];
                    clientSock.fd = clientFd;
                    clientSock.connected = true;

                    auto clientClass = std::make_shared<FluxClass>();
                    clientClass->name = "Socket";
                    auto clientObj = std::make_shared<FluxObject>();
                    clientObj->classDef = clientClass;
                    clientObj->fields["_id"] = Value::fromInt(clientId);

                    // write(data) on accepted socket
                    {
                        Value wfn;
                        wfn.type = ValueType::NATIVE_FUNCTION;
                        wfn.nativeFn = [clientId](Interpreter& interp, std::vector<Value> args) -> Value {
                            if (args.empty()) return Value::nil();
                            auto it2 = socketMap.find(clientId);
                            if (it2 == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");
                            std::string data = args[0].toString();
                            ssize_t sent = ::send(it2->second.fd, data.c_str(), data.size(), 0);
                            if (sent < 0) throw FluxException("NetworkError", "Failed to send data");
                            return Value::fromInt((int)sent);
                        };
                        clientObj->fields["write"] = wfn;
                    }

                    // readLine() on accepted socket
                    {
                        Value rfn;
                        rfn.type = ValueType::NATIVE_FUNCTION;
                        rfn.nativeFn = [clientId](Interpreter& interp, std::vector<Value> args) -> Value {
                            auto it2 = socketMap.find(clientId);
                            if (it2 == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");
                            std::string line;
                            char c;
                            while (::recv(it2->second.fd, &c, 1, 0) > 0) {
                                if (c == '\n') break;
                                if (c != '\r') line += c;
                            }
                            return Value::fromString(line);
                        };
                        clientObj->fields["readLine"] = rfn;
                    }

                    // read(maxBytes) on accepted socket
                    {
                        Value rfn;
                        rfn.type = ValueType::NATIVE_FUNCTION;
                        rfn.nativeFn = [clientId](Interpreter& interp, std::vector<Value> args) -> Value {
                            auto it2 = socketMap.find(clientId);
                            if (it2 == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");
                            int maxBytes = args.empty() ? 4096 : (int)args[0].toNumber();
                            std::vector<char> buf(maxBytes);
                            ssize_t received = ::recv(it2->second.fd, buf.data(), maxBytes, 0);
                            if (received < 0) throw FluxException("NetworkError", "Failed to read data");
                            return Value::fromString(std::string(buf.data(), received));
                        };
                        clientObj->fields["read"] = rfn;
                    }

                    // close() on accepted socket
                    {
                        Value cfn;
                        cfn.type = ValueType::NATIVE_FUNCTION;
                        cfn.nativeFn = [clientId](Interpreter& interp, std::vector<Value> args) -> Value {
                            auto it2 = socketMap.find(clientId);
                            if (it2 != socketMap.end()) {
                                ::close(it2->second.fd);
                                socketMap.erase(it2);
                            }
                            return Value::nil();
                        };
                        clientObj->fields["close"] = cfn;
                    }

                    Value result;
                    result.type = ValueType::OBJECT;
                    result.objectVal = clientObj;
                    return result;
                };
                sockObj->fields["accept"] = fn;
            }

            // write(data)
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id](Interpreter& interp, std::vector<Value> args) -> Value {
                    if (args.empty()) return Value::nil();
                    auto it = socketMap.find(id);
                    if (it == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");
                    std::string data = args[0].toString();
                    ssize_t sent = ::send(it->second.fd, data.c_str(), data.size(), 0);
                    if (sent < 0) throw FluxException("NetworkError", "Failed to send data");
                    return Value::fromInt((int)sent);
                };
                sockObj->fields["write"] = fn;
            }

            // readLine() -> string
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id](Interpreter& interp, std::vector<Value> args) -> Value {
                    auto it = socketMap.find(id);
                    if (it == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");
                    std::string line;
                    char c;
                    while (::recv(it->second.fd, &c, 1, 0) > 0) {
                        if (c == '\n') break;
                        if (c != '\r') line += c;
                    }
                    return Value::fromString(line);
                };
                sockObj->fields["readLine"] = fn;
            }

            // read(maxBytes) -> string
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id](Interpreter& interp, std::vector<Value> args) -> Value {
                    auto it = socketMap.find(id);
                    if (it == socketMap.end()) throw FluxException("NetworkError", "Invalid socket");
                    int maxBytes = args.empty() ? 4096 : (int)args[0].toNumber();
                    std::vector<char> buf(maxBytes);
                    ssize_t received = ::recv(it->second.fd, buf.data(), maxBytes, 0);
                    if (received < 0) throw FluxException("NetworkError", "Failed to read data");
                    return Value::fromString(std::string(buf.data(), received));
                };
                sockObj->fields["read"] = fn;
            }

            // close()
            {
                Value fn;
                fn.type = ValueType::NATIVE_FUNCTION;
                fn.nativeFn = [id](Interpreter& interp, std::vector<Value> args) -> Value {
                    auto it = socketMap.find(id);
                    if (it != socketMap.end()) {
                        ::close(it->second.fd);
                        socketMap.erase(it);
                    }
                    return Value::nil();
                };
                sockObj->fields["close"] = fn;
            }

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = sockObj;
            return result;
        };

        env->define("Socket", socketCtor, "native_function");
    }

    // Protocol enum
    {
        auto protEnum = std::make_shared<FluxEnum>();
        protEnum->name = "Protocol";
        protEnum->values["TCP"] = 0;
        protEnum->values["UDP"] = 1;
        Value enumVal;
        enumVal.type = ValueType::ENUM_DEF;
        enumVal.enumVal = protEnum;
        env->define("Protocol", enumVal, "enum");
    }
}
