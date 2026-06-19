#include "std_graphics.h"
#include "../src/interpreter.h"
#include <cmath>
#include <map>

// ============================================================================
// std.graphics — Window creation and basic 2D/3D rendering
//
// Compile flags:
//   -DFLUX_HAS_SDL2   -> enables SDL2 backend (link with -lSDL2)
//   -DFLUX_HAS_GLFW   -> enables GLFW backend (link with -lglfw -lGL)
//
// When both SDL2 and GLFW are available:
//   - Window starts with SDL2 for 2D rendering (text, images, shapes)
//   - enable3D() switches to GLFW+OpenGL for 3D rendering
// When only SDL2 is available:
//   - 3D methods are stubs (warn and no-op)
// When only GLFW is available:
//   - Full 2D+3D via OpenGL immediate mode (no text/image support)
//
// Provides:
//   Window(title, width, height) -> Window object
//     .isOpen()              -> bool
//     .pollEvents()          -> nil (pumps event queue)
//     .clear(r, g, b)        -> nil (clear with color, 0-255 each)
//     .present()             -> nil (swap buffers / flip)
//     .close()               -> nil
//     .setTitle(str)         -> nil
//     .width                 -> int
//     .height                -> int
//     .backend               -> string ("sdl2+glfw" | "sdl2" | "glfw" | "none")
//
//   Input namespace (populated per-frame via pollEvents):
//     Input.keyPressed(key)  -> bool
//     Input.mouseX()         -> int
//     Input.mouseY()         -> int
//     Input.mouseDown(btn)   -> bool
// ============================================================================

#ifdef FLUX_HAS_SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#endif

#ifdef FLUX_HAS_GLFW
#include <GLFW/glfw3.h>
#endif

// ============================================================================
// Backend abstraction
// ============================================================================

#ifdef FLUX_HAS_SDL2

// ---- SDL2 backend (with optional GLFW for 3D) ----
struct FluxWindow {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool open = false;
    int w = 0, h = 0;
    std::string windowTitle;
    
    // Aspect ratio snapping
    bool snapEnabled = false;
    float lockedAspectRatio = 0.0f;

#ifdef FLUX_HAS_GLFW
    GLFWwindow* glfwWindow = nullptr;
    bool using3D = false;
    static bool glfw_initialized;
#endif

    static bool sdl_initialized;

    FluxWindow(const std::string& title, int width, int height)
        : w(width), h(height), windowTitle(title) {
        if (!sdl_initialized) {
            SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
            sdl_initialized = true;
        }
        window = SDL_CreateWindow(title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        if (window) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            open = true;
        }
    }

    ~FluxWindow() {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
#ifdef FLUX_HAS_GLFW
        if (glfwWindow) glfwDestroyWindow(glfwWindow);
#endif
    }

    bool isOpen() const {
#ifdef FLUX_HAS_GLFW
        if (using3D) return open && glfwWindow && !glfwWindowShouldClose(glfwWindow);
#endif
        return open;
    }

    void pollEvents() {
#ifdef FLUX_HAS_GLFW
        if (using3D) {
            glfwPollEvents();
            if (glfwWindow && glfwWindowShouldClose(glfwWindow)) open = false;

            // Update viewport on window resize
            if (glfwWindow) {
                int fbW, fbH;
                glfwGetFramebufferSize(glfwWindow, &fbW, &fbH);
                if (fbW != w || fbH != h) {
                    // Enforce aspect ratio if snap is enabled
                    if (snapEnabled && lockedAspectRatio > 0.0f) {
                        int newW = fbW;
                        int newH = (int)(newW / lockedAspectRatio);
                        if (newH != fbH) {
                            glfwSetWindowSize(glfwWindow, newW, newH);
                            fbH = newH;
                        }
                    }
                    w = fbW;
                    h = fbH;
                    glViewport(0, 0, w, h);
                }
            }
            return;
        }
#endif
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) open = false;
            // Enforce aspect ratio on SDL2 window resize
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                if (snapEnabled && lockedAspectRatio > 0.0f) {
                    int newW = e.window.data1;
                    int newH = (int)(newW / lockedAspectRatio);
                    if (newH != e.window.data2) {
                        SDL_SetWindowSize(window, newW, newH);
                    }
                    w = newW;
                    h = newH;
                }
            }
        }
    }

    void clear(int r, int g, int b) {
#ifdef FLUX_HAS_GLFW
        if (using3D) {
            glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            return;
        }
#endif
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, 255);
            SDL_RenderClear(renderer);
        }
    }

    void present() {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) { glfwSwapBuffers(glfwWindow); return; }
#endif
        if (renderer) SDL_RenderPresent(renderer);
    }

    void close() {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) {
            glfwSetWindowShouldClose(glfwWindow, GLFW_TRUE);
        }
#endif
        open = false;
    }

    void setTitle(const std::string& title) {
        windowTitle = title;
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) { glfwSetWindowTitle(glfwWindow, title.c_str()); return; }
#endif
        if (window) SDL_SetWindowTitle(window, title.c_str());
    }

    // ---- 2D Drawing Primitives ----

    void drawPixel(int x, int y, int r, int g, int b, int a = 255) {
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    void drawLine(int x1, int y1, int x2, int y2, int r, int g, int b, int a = 255) {
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }

    void drawRect(int x, int y, int rw, int rh, int r, int g, int b, int a = 255) {
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_Rect rect = {x, y, rw, rh};
            SDL_RenderDrawRect(renderer, &rect);
        }
    }

    void fillRect(int x, int y, int rw, int rh, int r, int g, int b, int a = 255) {
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
            SDL_Rect rect = {x, y, rw, rh};
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    void drawCircle(int cx, int cy, int radius, int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        // Midpoint circle algorithm
        int x = radius, y = 0, err = 0;
        while (x >= y) {
            SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            SDL_RenderDrawPoint(renderer, cx + y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - x, cy + y);
            SDL_RenderDrawPoint(renderer, cx - x, cy - y);
            SDL_RenderDrawPoint(renderer, cx - y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + x, cy - y);
            if (err <= 0) { y++; err += 2 * y + 1; }
            if (err > 0)  { x--; err -= 2 * x + 1; }
        }
    }

    void fillCircle(int cx, int cy, int radius, int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        for (int dy = -radius; dy <= radius; dy++) {
            int dx = (int)std::sqrt((double)(radius * radius - dy * dy));
            SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    }

    void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3,
                      int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        SDL_RenderDrawLine(renderer, x2, y2, x3, y3);
        SDL_RenderDrawLine(renderer, x3, y3, x1, y1);
    }

    void setBlendMode(bool enable) {
        if (renderer) {
            SDL_SetRenderDrawBlendMode(renderer,
                enable ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
        }
    }

    void resize(int newW, int newH) {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) {
            // Enforce aspect ratio if snap is enabled
            if (snapEnabled && lockedAspectRatio > 0.0f) {
                newH = (int)(newW / lockedAspectRatio);
            }
            glfwSetWindowSize(glfwWindow, newW, newH);
            w = newW; h = newH;
            return;
        }
#endif
        if (window) {
            // Enforce aspect ratio if snap is enabled
            if (snapEnabled && lockedAspectRatio > 0.0f) {
                newH = (int)(newW / lockedAspectRatio);
            }
            SDL_SetWindowSize(window, newW, newH);
            w = newW;
            h = newH;
        }
    }

    int getWidth() const { return w; }
    int getHeight() const { return h; }

    void snapAspectRatio(float ratio) {
        if (ratio > 0.0f) {
            snapEnabled = true;
            lockedAspectRatio = ratio;
            // Apply immediately to current window
            int newH = (int)(w / ratio);
            resize(w, newH);
        } else {
            snapEnabled = false;
            lockedAspectRatio = 0.0f;
        }
    }

    // ---- Additional Shape Primitives ----

    void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3,
                      int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        // Scanline fill via sorted vertex Y coordinates
        int minY = std::min({y1, y2, y3});
        int maxY = std::max({y1, y2, y3});
        for (int y = minY; y <= maxY; y++) {
            std::vector<int> xIntersections;
            auto addEdge = [&](int ax, int ay, int bx, int by) {
                if (ay == by) return;
                if ((y < std::min(ay, by)) || (y >= std::max(ay, by))) return;
                int x = ax + (y - ay) * (bx - ax) / (by - ay);
                xIntersections.push_back(x);
            };
            addEdge(x1, y1, x2, y2);
            addEdge(x2, y2, x3, y3);
            addEdge(x3, y3, x1, y1);
            if (xIntersections.size() >= 2) {
                std::sort(xIntersections.begin(), xIntersections.end());
                SDL_RenderDrawLine(renderer, xIntersections[0], y, xIntersections[1], y);
            }
        }
    }

    void drawEllipse(int cx, int cy, int rx, int ry, int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        int segments = std::max(32, std::max(rx, ry) * 4);
        for (int i = 0; i < segments; i++) {
            float a1 = 2.0f * M_PI * i / segments;
            float a2 = 2.0f * M_PI * (i + 1) / segments;
            int px1 = cx + (int)(rx * std::cos(a1));
            int py1 = cy + (int)(ry * std::sin(a1));
            int px2 = cx + (int)(rx * std::cos(a2));
            int py2 = cy + (int)(ry * std::sin(a2));
            SDL_RenderDrawLine(renderer, px1, py1, px2, py2);
        }
    }

    void fillEllipse(int cx, int cy, int rx, int ry, int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        for (int dy = -ry; dy <= ry; dy++) {
            int dx = (int)(rx * std::sqrt(1.0 - (double)(dy * dy) / (double)(ry * ry)));
            SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    }

    void drawRoundedRect(int x, int y, int rw, int rh, int radius,
                         int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        radius = std::min(radius, std::min(rw / 2, rh / 2));
        // Straight edges
        SDL_RenderDrawLine(renderer, x + radius, y, x + rw - radius, y);
        SDL_RenderDrawLine(renderer, x + radius, y + rh, x + rw - radius, y + rh);
        SDL_RenderDrawLine(renderer, x, y + radius, x, y + rh - radius);
        SDL_RenderDrawLine(renderer, x + rw, y + radius, x + rw, y + rh - radius);
        // Corner arcs (quarter circles)
        auto drawArc = [&](int cx, int cy, float startAngle, float endAngle) {
            int segs = std::max(8, radius);
            for (int i = 0; i < segs; i++) {
                float a1 = startAngle + (endAngle - startAngle) * i / segs;
                float a2 = startAngle + (endAngle - startAngle) * (i + 1) / segs;
                SDL_RenderDrawLine(renderer,
                    cx + (int)(radius * std::cos(a1)), cy + (int)(radius * std::sin(a1)),
                    cx + (int)(radius * std::cos(a2)), cy + (int)(radius * std::sin(a2)));
            }
        };
        drawArc(x + radius, y + radius, M_PI, 1.5 * M_PI);           // top-left
        drawArc(x + rw - radius, y + radius, 1.5 * M_PI, 2.0 * M_PI); // top-right
        drawArc(x + rw - radius, y + rh - radius, 0, 0.5 * M_PI);     // bottom-right
        drawArc(x + radius, y + rh - radius, 0.5 * M_PI, M_PI);       // bottom-left
    }

    void fillRoundedRect(int x, int y, int rw, int rh, int radius,
                         int r, int g, int b, int a = 255) {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);
        radius = std::min(radius, std::min(rw / 2, rh / 2));
        // Fill center rectangle
        SDL_Rect center = {x, y + radius, rw + 1, rh - 2 * radius};
        SDL_RenderFillRect(renderer, &center);
        // Fill top and bottom strips
        SDL_Rect top = {x + radius, y, rw - 2 * radius + 1, radius};
        SDL_RenderFillRect(renderer, &top);
        SDL_Rect bot = {x + radius, y + rh - radius, rw - 2 * radius + 1, radius + 1};
        SDL_RenderFillRect(renderer, &bot);
        // Fill corners with quarter circles
        auto fillCorner = [&](int cx, int cy) {
            for (int dy = -radius; dy <= radius; dy++) {
                int dx = (int)std::sqrt((double)(radius * radius - dy * dy));
                SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
            }
        };
        fillCorner(x + radius, y + radius);
        fillCorner(x + rw - radius, y + radius);
        fillCorner(x + radius, y + rh - radius);
        fillCorner(x + rw - radius, y + rh - radius);
    }

    // ---- Text Rendering (SDL_ttf) ----

    static bool ttf_initialized;
    std::map<std::string, TTF_Font*> fontCache;

    TTF_Font* getFont(const std::string& path, int size) {
        std::string key = path + ":" + std::to_string(size);
        auto it = fontCache.find(key);
        if (it != fontCache.end()) return it->second;
        if (!ttf_initialized) {
            TTF_Init();
            ttf_initialized = true;
        }
        TTF_Font* font = TTF_OpenFont(path.c_str(), size);
        if (font) fontCache[key] = font;
        return font;
    }

    void drawText(const std::string& text, int x, int y,
                  const std::string& fontPath, int fontSize,
                  int r, int g, int b, int a = 255) {
        if (!renderer) return;
        TTF_Font* font = getFont(fontPath, fontSize);
        if (!font) return;
        SDL_Color color = {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect dst = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }

    // Returns [width, height] of text
    std::pair<int, int> measureText(const std::string& text,
                                     const std::string& fontPath, int fontSize) {
        TTF_Font* font = getFont(fontPath, fontSize);
        if (!font) return {0, 0};
        int w = 0, h = 0;
        TTF_SizeUTF8(font, text.c_str(), &w, &h);
        return {w, h};
    }

    // ---- Image Loading and Drawing (SDL2_image) ----

    static bool img_initialized;
    std::map<std::string, SDL_Texture*> imageCache;

    SDL_Texture* loadImageTexture(const std::string& path) {
        auto it = imageCache.find(path);
        if (it != imageCache.end()) return it->second;
        if (!img_initialized) {
            IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
            img_initialized = true;
        }
        if (!renderer) return nullptr;
        SDL_Texture* tex = IMG_LoadTexture(renderer, path.c_str());
        if (tex) imageCache[path] = tex;
        return tex;
    }

    void drawImage(const std::string& path, int x, int y) {
        SDL_Texture* tex = loadImageTexture(path);
        if (!tex || !renderer) return;
        int w, h;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
        SDL_Rect dst = {x, y, w, h};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
    }

    void drawImageScaled(const std::string& path, int x, int y, int dw, int dh) {
        SDL_Texture* tex = loadImageTexture(path);
        if (!tex || !renderer) return;
        SDL_Rect dst = {x, y, dw, dh};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
    }

    // Get image dimensions: returns [width, height]
    std::pair<int, int> getImageSize(const std::string& path) {
        SDL_Texture* tex = loadImageTexture(path);
        if (!tex) return {0, 0};
        int w, h;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
        return {w, h};
    }

    // ---- 3D Rendering (GLFW+OpenGL when available, stubs otherwise) ----
    void enable3D() {
#ifdef FLUX_HAS_GLFW
        if (!using3D) {
            // Close SDL2 window, reopen with GLFW+OpenGL
            if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
            if (window) { SDL_DestroyWindow(window); window = nullptr; }
            if (!glfw_initialized) { glfwInit(); glfw_initialized = true; }
            glfwWindow = glfwCreateWindow(w, h, windowTitle.c_str(), nullptr, nullptr);
            if (glfwWindow) { glfwMakeContextCurrent(glfwWindow); }
            using3D = true;
        }
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
#else
        static bool warned = false;
        if (!warned) {
            std::cerr << "[Flux] Warning: 3D requires GLFW (compile with -DFLUX_HAS_GLFW)" << std::endl;
            warned = true;
        }
#endif
    }
    void disable3D() {
#ifdef FLUX_HAS_GLFW
        if (using3D) { glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); }
#endif
    }
    void clearDepth() {
#ifdef FLUX_HAS_GLFW
        if (using3D) glClear(GL_DEPTH_BUFFER_BIT);
#endif
    }
    void setPerspective(float fov, float aspect, float nearPlane, float farPlane) {
#ifdef FLUX_HAS_GLFW
        if (!using3D) return;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float f = 1.0f / std::tan(fov * 0.5f * 3.14159265f / 180.0f);
        float m[16] = {
            f/aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (farPlane+nearPlane)/(nearPlane-farPlane), -1,
            0, 0, (2*farPlane*nearPlane)/(nearPlane-farPlane), 0
        };
        glLoadMatrixf(m);
        glMatrixMode(GL_MODELVIEW);
#else
        (void)fov; (void)aspect; (void)nearPlane; (void)farPlane;
#endif
    }
    void setCamera(float eyeX, float eyeY, float eyeZ,
                   float lookX, float lookY, float lookZ,
                   float upX, float upY, float upZ) {
#ifdef FLUX_HAS_GLFW
        if (!using3D) return;
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        float fx = lookX-eyeX, fy = lookY-eyeY, fz = lookZ-eyeZ;
        float flen = std::sqrt(fx*fx+fy*fy+fz*fz);
        fx/=flen; fy/=flen; fz/=flen;
        float rx = fy*upZ-fz*upY, ry = fz*upX-fx*upZ, rz = fx*upY-fy*upX;
        float rlen = std::sqrt(rx*rx+ry*ry+rz*rz);
        rx/=rlen; ry/=rlen; rz/=rlen;
        float ux = ry*fz-rz*fy, uy = rz*fx-rx*fz, uz = rx*fy-ry*fx;
        float m[16] = {
            rx, ux, -fx, 0,
            ry, uy, -fy, 0,
            rz, uz, -fz, 0,
            -(rx*eyeX+ry*eyeY+rz*eyeZ),
            -(ux*eyeX+uy*eyeY+uz*eyeZ),
            (fx*eyeX+fy*eyeY+fz*eyeZ), 1
        };
        glLoadMatrixf(m);
#else
        (void)eyeX;(void)eyeY;(void)eyeZ;(void)lookX;(void)lookY;(void)lookZ;
        (void)upX;(void)upY;(void)upZ;
#endif
    }
    void pushMatrix() {
#ifdef FLUX_HAS_GLFW
        if (using3D) glPushMatrix();
#endif
    }
    void popMatrix() {
#ifdef FLUX_HAS_GLFW
        if (using3D) glPopMatrix();
#endif
    }
    void translate(float x, float y, float z) {
#ifdef FLUX_HAS_GLFW
        if (using3D) glTranslatef(x, y, z);
#else
        (void)x;(void)y;(void)z;
#endif
    }
    void rotate(float angle, float x, float y, float z) {
#ifdef FLUX_HAS_GLFW
        if (using3D) glRotatef(angle, x, y, z);
#else
        (void)angle;(void)x;(void)y;(void)z;
#endif
    }
    void scale(float x, float y, float z) {
#ifdef FLUX_HAS_GLFW
        if (using3D) glScalef(x, y, z);
#else
        (void)x;(void)y;(void)z;
#endif
    }
    int loadTexture(const std::string& path) {
#if defined(FLUX_HAS_GLFW) && defined(FLUX_HAS_SDL2_IMAGE)
        if (!using3D) return 0;
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) return 0;
        GLuint texID;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        GLenum format = (surf->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, surf->w, surf->h, 0,
                     format, GL_UNSIGNED_BYTE, surf->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        SDL_FreeSurface(surf);
        return (int)texID;
#else
        (void)path;
        return 0;
#endif
    }
    void bindTexture(int texID) {
#ifdef FLUX_HAS_GLFW
        if (!using3D) return;
        if (texID > 0) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, (GLuint)texID); }
        else { glDisable(GL_TEXTURE_2D); }
#else
        (void)texID;
#endif
    }
    void drawTexturedCube(float size, int texID) {
#ifdef FLUX_HAS_GLFW
        if (!using3D) return;
        if (texID > 0) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, (GLuint)texID); }
        glColor4f(1,1,1,1);
        float s = size / 2;
        glBegin(GL_QUADS);
        // Front
        glTexCoord2f(0,0); glVertex3f(-s,-s,s);
        glTexCoord2f(1,0); glVertex3f(s,-s,s);
        glTexCoord2f(1,1); glVertex3f(s,s,s);
        glTexCoord2f(0,1); glVertex3f(-s,s,s);
        // Back
        glTexCoord2f(1,0); glVertex3f(-s,-s,-s);
        glTexCoord2f(1,1); glVertex3f(-s,s,-s);
        glTexCoord2f(0,1); glVertex3f(s,s,-s);
        glTexCoord2f(0,0); glVertex3f(s,-s,-s);
        // Top
        glTexCoord2f(0,1); glVertex3f(-s,s,-s);
        glTexCoord2f(0,0); glVertex3f(-s,s,s);
        glTexCoord2f(1,0); glVertex3f(s,s,s);
        glTexCoord2f(1,1); glVertex3f(s,s,-s);
        // Bottom
        glTexCoord2f(1,1); glVertex3f(-s,-s,-s);
        glTexCoord2f(0,1); glVertex3f(s,-s,-s);
        glTexCoord2f(0,0); glVertex3f(s,-s,s);
        glTexCoord2f(1,0); glVertex3f(-s,-s,s);
        // Right
        glTexCoord2f(1,0); glVertex3f(s,-s,-s);
        glTexCoord2f(1,1); glVertex3f(s,s,-s);
        glTexCoord2f(0,1); glVertex3f(s,s,s);
        glTexCoord2f(0,0); glVertex3f(s,-s,s);
        // Left
        glTexCoord2f(0,0); glVertex3f(-s,-s,-s);
        glTexCoord2f(1,0); glVertex3f(-s,-s,s);
        glTexCoord2f(1,1); glVertex3f(-s,s,s);
        glTexCoord2f(0,1); glVertex3f(-s,s,-s);
        glEnd();
        if (texID > 0) glDisable(GL_TEXTURE_2D);
#else
        (void)size;(void)texID;
#endif
    }

    // Set the current 3D draw color (affects untextured geometry)
    void setColor(float r, float g, float b, float a = 1.0f) {
#ifdef FLUX_HAS_GLFW
        if (using3D) { glColor4f(r, g, b, a); }
#else
        (void)r;(void)g;(void)b;(void)a;
#endif
    }

    // Draw a colored quad in 3D (4 vertices, useful for floors/walls)
    void drawQuad(float x1, float y1, float z1,
                  float x2, float y2, float z2,
                  float x3, float y3, float z3,
                  float x4, float y4, float z4) {
#ifdef FLUX_HAS_GLFW
        if (!using3D) return;
        // Ensure white color so texture isn't multiplied by a dark color
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        // Disable face culling so quad is visible from both sides
        GLboolean cullingWasEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullingWasEnabled) glDisable(GL_CULL_FACE);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(x1, y1, z1);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(x2, y2, z2);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(x3, y3, z3);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(x4, y4, z4);
        glEnd();
        if (cullingWasEnabled) glEnable(GL_CULL_FACE);
#else
        (void)x1;(void)y1;(void)z1;(void)x2;(void)y2;(void)z2;
        (void)x3;(void)y3;(void)z3;(void)x4;(void)y4;(void)z4;
#endif
    }

    // ---- Input Methods ----
    bool keyPressed(const std::string& key) {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) {
            int glfwKey = GLFW_KEY_UNKNOWN;
            if (key=="W"||key=="w") glfwKey=GLFW_KEY_W;
            else if (key=="A"||key=="a") glfwKey=GLFW_KEY_A;
            else if (key=="S"||key=="s") glfwKey=GLFW_KEY_S;
            else if (key=="D"||key=="d") glfwKey=GLFW_KEY_D;
            else if (key=="E"||key=="e") glfwKey=GLFW_KEY_E;
            else if (key=="SPACE") glfwKey=GLFW_KEY_SPACE;
            else if (key=="SHIFT") glfwKey=GLFW_KEY_LEFT_SHIFT;
            else if (key=="CTRL") glfwKey=GLFW_KEY_LEFT_CONTROL;
            else if (key=="ESC") glfwKey=GLFW_KEY_ESCAPE;
            return glfwKey != GLFW_KEY_UNKNOWN && glfwGetKey(glfwWindow, glfwKey) == GLFW_PRESS;
        }
#endif
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (key == "W" || key == "w") return state[SDL_SCANCODE_W];
        if (key == "A" || key == "a") return state[SDL_SCANCODE_A];
        if (key == "S" || key == "s") return state[SDL_SCANCODE_S];
        if (key == "D" || key == "d") return state[SDL_SCANCODE_D];
        if (key == "E" || key == "e") return state[SDL_SCANCODE_E];
        if (key == "SPACE") return state[SDL_SCANCODE_SPACE];
        if (key == "SHIFT") return state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
        if (key == "CTRL") return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
        if (key == "ESC") return state[SDL_SCANCODE_ESCAPE];
        return false;
    }
    std::vector<int32_t> getMousePos() {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) {
            double x, y;
            glfwGetCursorPos(glfwWindow, &x, &y);
            return {(int32_t)x, (int32_t)y};
        }
#endif
        int x, y;
        SDL_GetMouseState(&x, &y);
        return {x, y};
    }
    void setMousePos(int x, int y) {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) {
            glfwSetCursorPos(glfwWindow, (double)x, (double)y);
            return;
        }
#endif
        SDL_WarpMouseInWindow(window, x, y);
    }

    void setCursorMode(const std::string& mode) {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) {
            if (mode == "disabled")
                glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            else
                glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            return;
        }
#endif
        if (mode == "disabled") SDL_SetRelativeMouseMode(SDL_TRUE);
        else SDL_SetRelativeMouseMode(SDL_FALSE);
    }

    bool mouseButtonPressed(int button) {
#ifdef FLUX_HAS_GLFW
        if (using3D && glfwWindow) {
            int glfwBtn = GLFW_MOUSE_BUTTON_LEFT;
            if (button == 1) glfwBtn = GLFW_MOUSE_BUTTON_MIDDLE;
            else if (button == 2) glfwBtn = GLFW_MOUSE_BUTTON_RIGHT;
            return glfwGetMouseButton(glfwWindow, glfwBtn) == GLFW_PRESS;
        }
#endif
        Uint32 state = SDL_GetMouseState(nullptr, nullptr);
        switch (button) {
            case 0: return (state & SDL_BUTTON_LMASK) != 0;
            case 1: return (state & SDL_BUTTON_MMASK) != 0;
            case 2: return (state & SDL_BUTTON_RMASK) != 0;
            default: return false;
        }
    }
};

bool FluxWindow::sdl_initialized = false;
bool FluxWindow::ttf_initialized = false;
bool FluxWindow::img_initialized = false;
#ifdef FLUX_HAS_GLFW
bool FluxWindow::glfw_initialized = false;
#endif

#elif defined(FLUX_HAS_GLFW)

// ---- GLFW backend ----
struct FluxWindow {
    GLFWwindow* window = nullptr;
    bool open = false;
    int w = 0, h = 0;

    // Aspect ratio snapping
    bool snapEnabled = false;
    float lockedAspectRatio = 0.0f;

    static bool glfw_initialized;

    FluxWindow(const std::string& title, int width, int height) : w(width), h(height) {
        if (!glfw_initialized) {
            glfwInit();
            glfw_initialized = true;
        }
        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (window) {
            glfwMakeContextCurrent(window);
            open = true;
        }
    }

    ~FluxWindow() {
        if (window) glfwDestroyWindow(window);
    }

    bool isOpen() const { return open && window && !glfwWindowShouldClose(window); }

    void pollEvents() {
        glfwPollEvents();
        if (window && glfwWindowShouldClose(window)) open = false;
        
        // Handle window resize with aspect ratio enforcement
        if (window) {
            int fbW, fbH;
            glfwGetFramebufferSize(window, &fbW, &fbH);
            if (fbW != w || fbH != h) {
                if (snapEnabled && lockedAspectRatio > 0.0f) {
                    int newW = fbW;
                    int newH = (int)(newW / lockedAspectRatio);
                    if (newH != fbH) {
                        glfwSetWindowSize(window, newW, newH);
                        fbH = newH;
                    }
                }
                w = fbW;
                h = fbH;
                glViewport(0, 0, w, h);
            }
        }
    }

    void clear(int r, int g, int b) {
        glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void present() {
        if (window) glfwSwapBuffers(window);
    }

    void close() {
        open = false;
        if (window) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    void setTitle(const std::string& title) {
        if (window) glfwSetWindowTitle(window, title.c_str());
    }

    // ---- 2D Drawing Primitives (OpenGL immediate mode) ----

    void drawPixel(int x, int y, int r, int g, int b, int a = 255) {
        glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        glBegin(GL_POINTS);
        // Convert from pixel coords to NDC
        float nx = 2.0f * x / w - 1.0f;
        float ny = 1.0f - 2.0f * y / h;
        glVertex2f(nx, ny);
        glEnd();
    }

    void drawLine(int x1, int y1, int x2, int y2, int r, int g, int b, int a = 255) {
        glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        glBegin(GL_LINES);
        glVertex2f(2.0f * x1 / w - 1.0f, 1.0f - 2.0f * y1 / h);
        glVertex2f(2.0f * x2 / w - 1.0f, 1.0f - 2.0f * y2 / h);
        glEnd();
    }

    void drawRect(int x, int y, int rw, int rh, int r, int g, int b, int a = 255) {
        drawLine(x, y, x + rw, y, r, g, b, a);
        drawLine(x + rw, y, x + rw, y + rh, r, g, b, a);
        drawLine(x + rw, y + rh, x, y + rh, r, g, b, a);
        drawLine(x, y + rh, x, y, r, g, b, a);
    }

    void fillRect(int x, int y, int rw, int rh, int r, int g, int b, int a = 255) {
        glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        float nx1 = 2.0f * x / w - 1.0f;
        float ny1 = 1.0f - 2.0f * y / h;
        float nx2 = 2.0f * (x + rw) / w - 1.0f;
        float ny2 = 1.0f - 2.0f * (y + rh) / h;
        glBegin(GL_QUADS);
        glVertex2f(nx1, ny1);
        glVertex2f(nx2, ny1);
        glVertex2f(nx2, ny2);
        glVertex2f(nx1, ny2);
        glEnd();
    }

    void drawCircle(int cx, int cy, int radius, int r, int g, int b, int a = 255) {
        glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; i++) {
            float angle = 2.0f * 3.14159265f * i / 64.0f;
            float px = cx + radius * std::cos(angle);
            float py = cy + radius * std::sin(angle);
            glVertex2f(2.0f * px / w - 1.0f, 1.0f - 2.0f * py / h);
        }
        glEnd();
    }

    void fillCircle(int cx, int cy, int radius, int r, int g, int b, int a = 255) {
        glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(2.0f * cx / w - 1.0f, 1.0f - 2.0f * cy / h);
        for (int i = 0; i <= 64; i++) {
            float angle = 2.0f * 3.14159265f * i / 64.0f;
            float px = cx + radius * std::cos(angle);
            float py = cy + radius * std::sin(angle);
            glVertex2f(2.0f * px / w - 1.0f, 1.0f - 2.0f * py / h);
        }
        glEnd();
    }

    void drawTriangle(int x1, int y1, int x2, int y2, int x3, int y3,
                      int r, int g, int b, int a = 255) {
        drawLine(x1, y1, x2, y2, r, g, b, a);
        drawLine(x2, y2, x3, y3, r, g, b, a);
        drawLine(x3, y3, x1, y1, r, g, b, a);
    }

    void setBlendMode(bool enable) {
        if (enable) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }
    }

    void resize(int newW, int newH) {
        if (window) {
            // Enforce aspect ratio if snap is enabled
            if (snapEnabled && lockedAspectRatio > 0.0f) {
                newH = (int)(newW / lockedAspectRatio);
            }
            glfwSetWindowSize(window, newW, newH);
            w = newW;
            h = newH;
        }
    }

    int getWidth() const { return w; }
    int getHeight() const { return h; }

    void snapAspectRatio(float ratio) {
        if (ratio > 0.0f) {
            snapEnabled = true;
            lockedAspectRatio = ratio;
            int newH = (int)(w / ratio);
            resize(w, newH);
        } else {
            snapEnabled = false;
            lockedAspectRatio = 0.0f;
        }
    }

    // ---- 3D Rendering Support ----
    
    void enable3D() {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    void disable3D() {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
    }

    void clearDepth() {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void setPerspective(float fov, float aspect, float nearPlane, float farPlane) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float f = 1.0f / std::tan(fov * 0.5f * 3.14159265f / 180.0f);
        float m[16] = {
            f/aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (farPlane+nearPlane)/(nearPlane-farPlane), -1,
            0, 0, (2*farPlane*nearPlane)/(nearPlane-farPlane), 0
        };
        glLoadMatrixf(m);
        glMatrixMode(GL_MODELVIEW);
    }

    void setCamera(float eyeX, float eyeY, float eyeZ,
                   float lookX, float lookY, float lookZ,
                   float upX, float upY, float upZ) {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Compute forward, right, up vectors
        float fx = lookX - eyeX, fy = lookY - eyeY, fz = lookZ - eyeZ;
        float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
        fx /= flen; fy /= flen; fz /= flen;
        
        float rx = fy*upZ - fz*upY;
        float ry = fz*upX - fx*upZ;
        float rz = fx*upY - fy*upX;
        float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
        rx /= rlen; ry /= rlen; rz /= rlen;
        
        float ux = ry*fz - rz*fy;
        float uy = rz*fx - rx*fz;
        float uz = rx*fy - ry*fx;
        
        float m[16] = {
            rx, ux, -fx, 0,
            ry, uy, -fy, 0,
            rz, uz, -fz, 0,
            -(rx*eyeX + ry*eyeY + rz*eyeZ),
            -(ux*eyeX + uy*eyeY + uz*eyeZ),
            (fx*eyeX + fy*eyeY + fz*eyeZ),
            1
        };
        glLoadMatrixf(m);
    }

    void pushMatrix() { glPushMatrix(); }
    void popMatrix() { glPopMatrix(); }
    void translate(float x, float y, float z) { glTranslatef(x, y, z); }
    void rotate(float angle, float x, float y, float z) { glRotatef(angle, x, y, z); }
    void scale(float x, float y, float z) { glScalef(x, y, z); }

    int loadTexture(const std::string& path) {
        #ifdef FLUX_HAS_SDL2_IMAGE
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (!surf) return 0;
        
        GLuint texID;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        
        GLenum format = (surf->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, surf->w, surf->h, 0,
                     format, GL_UNSIGNED_BYTE, surf->pixels);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        SDL_FreeSurface(surf);
        return (int)texID;
        #else
        return 0;
        #endif
    }

    void bindTexture(int texID) {
        if (texID > 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, (GLuint)texID);
        } else {
            glDisable(GL_TEXTURE_2D);
        }
    }

    void drawTexturedCube(float size, int texID) {
        if (texID > 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, (GLuint)texID);
        }
        
        glColor4f(1, 1, 1, 1);
        float s = size / 2;
        
        glBegin(GL_QUADS);
        
        // Front
        glTexCoord2f(0, 0); glVertex3f(-s, -s,  s);
        glTexCoord2f(1, 0); glVertex3f( s, -s,  s);
        glTexCoord2f(1, 1); glVertex3f( s,  s,  s);
        glTexCoord2f(0, 1); glVertex3f(-s,  s,  s);
        
        // Back
        glTexCoord2f(1, 0); glVertex3f(-s, -s, -s);
        glTexCoord2f(1, 1); glVertex3f(-s,  s, -s);
        glTexCoord2f(0, 1); glVertex3f( s,  s, -s);
        glTexCoord2f(0, 0); glVertex3f( s, -s, -s);
        
        // Top
        glTexCoord2f(0, 1); glVertex3f(-s,  s, -s);
        glTexCoord2f(0, 0); glVertex3f(-s,  s,  s);
        glTexCoord2f(1, 0); glVertex3f( s,  s,  s);
        glTexCoord2f(1, 1); glVertex3f( s,  s, -s);
        
        // Bottom
        glTexCoord2f(1, 1); glVertex3f(-s, -s, -s);
        glTexCoord2f(0, 1); glVertex3f( s, -s, -s);
        glTexCoord2f(0, 0); glVertex3f( s, -s,  s);
        glTexCoord2f(1, 0); glVertex3f(-s, -s,  s);
        
        // Right
        glTexCoord2f(1, 0); glVertex3f( s, -s, -s);
        glTexCoord2f(1, 1); glVertex3f( s,  s, -s);
        glTexCoord2f(0, 1); glVertex3f( s,  s,  s);
        glTexCoord2f(0, 0); glVertex3f( s, -s,  s);
        
        // Left
        glTexCoord2f(0, 0); glVertex3f(-s, -s, -s);
        glTexCoord2f(1, 0); glVertex3f(-s, -s,  s);
        glTexCoord2f(1, 1); glVertex3f(-s,  s,  s);
        glTexCoord2f(0, 1); glVertex3f(-s,  s, -s);
        
        glEnd();
        
        if (texID > 0) glDisable(GL_TEXTURE_2D);
    }

    // ---- Input Handling ----
    
    bool keyPressed(const std::string& key) {
        if (!window) return false;
        int glfwKey = GLFW_KEY_UNKNOWN;
        
        if (key == "W" || key == "w") glfwKey = GLFW_KEY_W;
        else if (key == "A" || key == "a") glfwKey = GLFW_KEY_A;
        else if (key == "S" || key == "s") glfwKey = GLFW_KEY_S;
        else if (key == "D" || key == "d") glfwKey = GLFW_KEY_D;
        else if (key == "E" || key == "e") glfwKey = GLFW_KEY_E;
        else if (key == "SPACE") glfwKey = GLFW_KEY_SPACE;
        else if (key == "SHIFT") glfwKey = GLFW_KEY_LEFT_SHIFT;
        else if (key == "CTRL") glfwKey = GLFW_KEY_LEFT_CONTROL;
        else if (key == "ESC") glfwKey = GLFW_KEY_ESCAPE;
        
        return glfwGetKey(window, glfwKey) == GLFW_PRESS;
    }

    std::vector<int32_t> getMousePos() {
        if (!window) return {0, 0};
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        return {(int32_t)x, (int32_t)y};
    }

    void setMousePos(int x, int y) {
        if (window) glfwSetCursorPos(window, x, y);
    }

    void setCursorMode(const std::string& mode) {
        if (!window) return;
        if (mode == "normal") glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else if (mode == "hidden") glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        else if (mode == "disabled") glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    bool mouseButtonPressed(int button) {
        if (!window) return false;
        return glfwGetMouseButton(window, button) == GLFW_PRESS;
    }
};

bool FluxWindow::glfw_initialized = false;

#else

// ---- No backend (stub) ----
struct FluxWindow {
    bool open = false;
    int w = 0, h = 0;
    bool snapEnabled = false;
    float lockedAspectRatio = 0.0f;

    FluxWindow(const std::string&, int width, int height) : w(width), h(height) {
        std::cerr << "[Flux] Warning: No graphics backend available (compile with -DFLUX_HAS_SDL2 or -DFLUX_HAS_GLFW)" << std::endl;
    }

    bool isOpen() const { return false; }
    void pollEvents() {}
    void clear(int, int, int) {}
    void present() {}
    void close() {}
    void setTitle(const std::string&) {}
    void drawPixel(int, int, int, int, int, int = 255) {}
    void drawLine(int, int, int, int, int, int, int, int = 255) {}
    void drawRect(int, int, int, int, int, int, int, int = 255) {}
    void fillRect(int, int, int, int, int, int, int, int = 255) {}
    void drawCircle(int, int, int, int, int, int, int = 255) {}
    void fillCircle(int, int, int, int, int, int, int = 255) {}
    void drawTriangle(int, int, int, int, int, int, int, int, int, int = 255) {}
    void fillTriangle(int, int, int, int, int, int, int, int, int, int = 255) {}
    void drawEllipse(int, int, int, int, int, int, int, int = 255) {}
    void fillEllipse(int, int, int, int, int, int, int, int = 255) {}
    void drawRoundedRect(int, int, int, int, int, int, int, int, int = 255) {}
    void fillRoundedRect(int, int, int, int, int, int, int, int, int = 255) {}
    void drawText(const std::string&, int, int, const std::string&, int, int, int, int, int = 255) {}
    std::pair<int, int> measureText(const std::string&, const std::string&, int) { return {0, 0}; }
    void drawImage(const std::string&, int, int) {}
    void drawImageScaled(const std::string&, int, int, int, int) {}
    std::pair<int, int> getImageSize(const std::string&) { return {0, 0}; }
    void setBlendMode(bool) {}
    void resize(int, int) {}
    int getWidth() const { return w; }
    int getHeight() const { return h; }
    void snapAspectRatio(float) {}
};

#endif

// ============================================================================
// Registration
// ============================================================================

void registerStdGraphics(std::shared_ptr<Environment> env, Interpreter& interp) {

    // Window(title, width, height) constructor
    Value windowCtor;
    windowCtor.type = ValueType::NATIVE_FUNCTION;
    windowCtor.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
        std::string title = args.size() > 0 ? args[0].toString() : "Flux Window";
        int width  = args.size() > 1 ? (int)args[1].toNumber() : 800;
        int height = args.size() > 2 ? (int)args[2].toNumber() : 600;

        auto win = std::make_shared<FluxWindow>(title, width, height);

        auto cls = std::make_shared<FluxClass>();
        cls->name = "Window";
        auto obj = std::make_shared<FluxObject>();
        obj->classDef = cls;

        obj->fields["width"]  = Value::fromInt(width);
        obj->fields["height"] = Value::fromInt(height);

#if defined(FLUX_HAS_SDL2) && defined(FLUX_HAS_GLFW)
        obj->fields["backend"] = Value::fromString("sdl2+glfw");
#elif defined(FLUX_HAS_SDL2)
        obj->fields["backend"] = Value::fromString("sdl2");
#elif defined(FLUX_HAS_GLFW)
        obj->fields["backend"] = Value::fromString("glfw");
#else
        obj->fields["backend"] = Value::fromString("none");
#endif

        // .isOpen() -> bool
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                return Value::fromBool(win->isOpen());
            };
            obj->fields["isOpen"] = fn;
        }

        // .pollEvents()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->pollEvents();
                return Value::nil();
            };
            obj->fields["pollEvents"] = fn;
        }

        // .clear(r, g, b)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int r = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int g = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int b = args.size() > 2 ? (int)args[2].toNumber() : 0;
                win->clear(r, g, b);
                return Value::nil();
            };
            obj->fields["clear"] = fn;
        }

        // .present()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->present();
                return Value::nil();
            };
            obj->fields["present"] = fn;
        }

        // .close()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->close();
                return Value::nil();
            };
            obj->fields["close"] = fn;
        }

        // .setTitle(str)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                if (!args.empty()) win->setTitle(args[0].toString());
                return Value::nil();
            };
            obj->fields["setTitle"] = fn;
        }

        // .drawPixel(x, y, r, g, b) or .drawPixel(x, y, r, g, b, a)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int r = args.size() > 2 ? (int)args[2].toNumber() : 255;
                int g = args.size() > 3 ? (int)args[3].toNumber() : 255;
                int b = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int a = args.size() > 5 ? (int)args[5].toNumber() : 255;
                win->drawPixel(x, y, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawPixel"] = fn;
        }

        // .drawLine(x1, y1, x2, y2, r, g, b) or with alpha
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x1 = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y1 = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int x2 = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int y2 = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int r  = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int g  = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int b  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int a  = args.size() > 7 ? (int)args[7].toNumber() : 255;
                win->drawLine(x1, y1, x2, y2, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawLine"] = fn;
        }

        // .drawRect(x, y, w, h, r, g, b) or with alpha
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x  = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y  = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rw = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int rh = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int r  = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int g  = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int b  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int a  = args.size() > 7 ? (int)args[7].toNumber() : 255;
                win->drawRect(x, y, rw, rh, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawRect"] = fn;
        }

        // .fillRect(x, y, w, h, r, g, b) or with alpha
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x  = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y  = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rw = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int rh = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int r  = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int g  = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int b  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int a  = args.size() > 7 ? (int)args[7].toNumber() : 255;
                win->fillRect(x, y, rw, rh, r, g, b, a);
                return Value::nil();
            };
            obj->fields["fillRect"] = fn;
        }

        // .drawCircle(cx, cy, radius, r, g, b) or with alpha
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int cx = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int cy = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rad = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int r  = args.size() > 3 ? (int)args[3].toNumber() : 255;
                int g  = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int b  = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int a  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                win->drawCircle(cx, cy, rad, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawCircle"] = fn;
        }

        // .fillCircle(cx, cy, radius, r, g, b) or with alpha
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int cx = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int cy = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rad = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int r  = args.size() > 3 ? (int)args[3].toNumber() : 255;
                int g  = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int b  = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int a  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                win->fillCircle(cx, cy, rad, r, g, b, a);
                return Value::nil();
            };
            obj->fields["fillCircle"] = fn;
        }

        // .drawTriangle(x1, y1, x2, y2, x3, y3, r, g, b) or with alpha
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x1 = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y1 = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int x2 = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int y2 = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int x3 = args.size() > 4 ? (int)args[4].toNumber() : 0;
                int y3 = args.size() > 5 ? (int)args[5].toNumber() : 0;
                int r  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int g  = args.size() > 7 ? (int)args[7].toNumber() : 255;
                int b  = args.size() > 8 ? (int)args[8].toNumber() : 255;
                int a  = args.size() > 9 ? (int)args[9].toNumber() : 255;
                win->drawTriangle(x1, y1, x2, y2, x3, y3, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawTriangle"] = fn;
        }

        // .setBlendMode(enabled)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                bool enable = args.size() > 0 ? args[0].isTruthy() : true;
                win->setBlendMode(enable);
                return Value::nil();
            };
            obj->fields["setBlendMode"] = fn;
        }

        // .resize(w, h)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win, obj](Interpreter&, std::vector<Value> args) -> Value {
                int newW = args.size() > 0 ? (int)args[0].toNumber() : 800;
                int newH = args.size() > 1 ? (int)args[1].toNumber() : 600;
                win->resize(newW, newH);
                obj->fields["width"] = Value::fromInt(win->getWidth());
                obj->fields["height"] = Value::fromInt(win->getHeight());
                return Value::nil();
            };
            obj->fields["resize"] = fn;
        }

        // .getWidth() -> int
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                return Value::fromInt(win->getWidth());
            };
            obj->fields["getWidth"] = fn;
        }

        // .getHeight() -> int
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                return Value::fromInt(win->getHeight());
            };
            obj->fields["getHeight"] = fn;
        }

        // .snapAspectRatio(ratio) - Pass 0 or negative to disable snapping
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win, obj](Interpreter&, std::vector<Value> args) -> Value {
                float ratio = args.size() > 0 ? (float)args[0].toNumber() : 0.0f;
                win->snapAspectRatio(ratio);
                obj->fields["width"] = Value::fromInt(win->getWidth());
                obj->fields["height"] = Value::fromInt(win->getHeight());
                return Value::nil();
            };
            obj->fields["snapAspectRatio"] = fn;
        }

        // ---- Additional Shape Methods ----

        // .fillTriangle(x1, y1, x2, y2, x3, y3, r, g, b [, a])
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x1 = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y1 = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int x2 = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int y2 = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int x3 = args.size() > 4 ? (int)args[4].toNumber() : 0;
                int y3 = args.size() > 5 ? (int)args[5].toNumber() : 0;
                int r  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int g  = args.size() > 7 ? (int)args[7].toNumber() : 255;
                int b  = args.size() > 8 ? (int)args[8].toNumber() : 255;
                int a  = args.size() > 9 ? (int)args[9].toNumber() : 255;
                win->fillTriangle(x1, y1, x2, y2, x3, y3, r, g, b, a);
                return Value::nil();
            };
            obj->fields["fillTriangle"] = fn;
        }

        // .drawEllipse(cx, cy, rx, ry, r, g, b [, a])
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int cx = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int cy = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rx = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int ry = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int r  = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int g  = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int b  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int a  = args.size() > 7 ? (int)args[7].toNumber() : 255;
                win->drawEllipse(cx, cy, rx, ry, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawEllipse"] = fn;
        }

        // .fillEllipse(cx, cy, rx, ry, r, g, b [, a])
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int cx = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int cy = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rx = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int ry = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int r  = args.size() > 4 ? (int)args[4].toNumber() : 255;
                int g  = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int b  = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int a  = args.size() > 7 ? (int)args[7].toNumber() : 255;
                win->fillEllipse(cx, cy, rx, ry, r, g, b, a);
                return Value::nil();
            };
            obj->fields["fillEllipse"] = fn;
        }

        // .drawRoundedRect(x, y, w, h, radius, r, g, b [, a])
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x     = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y     = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rw    = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int rh    = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int rad   = args.size() > 4 ? (int)args[4].toNumber() : 0;
                int r     = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int g     = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int b     = args.size() > 7 ? (int)args[7].toNumber() : 255;
                int a     = args.size() > 8 ? (int)args[8].toNumber() : 255;
                win->drawRoundedRect(x, y, rw, rh, rad, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawRoundedRect"] = fn;
        }

        // .fillRoundedRect(x, y, w, h, radius, r, g, b [, a])
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x     = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y     = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int rw    = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int rh    = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int rad   = args.size() > 4 ? (int)args[4].toNumber() : 0;
                int r     = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int g     = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int b     = args.size() > 7 ? (int)args[7].toNumber() : 255;
                int a     = args.size() > 8 ? (int)args[8].toNumber() : 255;
                win->fillRoundedRect(x, y, rw, rh, rad, r, g, b, a);
                return Value::nil();
            };
            obj->fields["fillRoundedRect"] = fn;
        }

        // ---- Text Rendering Methods ----

        // .drawText(text, x, y, fontPath, fontSize, r, g, b [, a])
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string text = args.size() > 0 ? args[0].toString() : "";
                int x       = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int y       = args.size() > 2 ? (int)args[2].toNumber() : 0;
                std::string font = args.size() > 3 ? args[3].toString() : "";
                int size    = args.size() > 4 ? (int)args[4].toNumber() : 16;
                int r       = args.size() > 5 ? (int)args[5].toNumber() : 255;
                int g       = args.size() > 6 ? (int)args[6].toNumber() : 255;
                int b       = args.size() > 7 ? (int)args[7].toNumber() : 255;
                int a       = args.size() > 8 ? (int)args[8].toNumber() : 255;
                win->drawText(text, x, y, font, size, r, g, b, a);
                return Value::nil();
            };
            obj->fields["drawText"] = fn;
        }

        // .measureText(text, fontPath, fontSize) -> [width, height]
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string text = args.size() > 0 ? args[0].toString() : "";
                std::string font = args.size() > 1 ? args[1].toString() : "";
                int size = args.size() > 2 ? (int)args[2].toNumber() : 16;
                auto [w, h] = win->measureText(text, font, size);
                auto list = std::make_shared<std::vector<Value>>();
                list->push_back(Value::fromInt(w));
                list->push_back(Value::fromInt(h));
                Value result;
                result.type = ValueType::LIST;
                result.listVal = list;
                return result;
            };
            obj->fields["measureText"] = fn;
        }

        // ---- Image Methods ----

        // .drawImage(path, x, y)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string path = args.size() > 0 ? args[0].toString() : "";
                int x = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int y = args.size() > 2 ? (int)args[2].toNumber() : 0;
                win->drawImage(path, x, y);
                return Value::nil();
            };
            obj->fields["drawImage"] = fn;
        }

        // .drawImageScaled(path, x, y, width, height)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string path = args.size() > 0 ? args[0].toString() : "";
                int x  = args.size() > 1 ? (int)args[1].toNumber() : 0;
                int y  = args.size() > 2 ? (int)args[2].toNumber() : 0;
                int dw = args.size() > 3 ? (int)args[3].toNumber() : 0;
                int dh = args.size() > 4 ? (int)args[4].toNumber() : 0;
                win->drawImageScaled(path, x, y, dw, dh);
                return Value::nil();
            };
            obj->fields["drawImageScaled"] = fn;
        }

        // .getImageSize(path) -> [width, height]
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string path = args.size() > 0 ? args[0].toString() : "";
                auto [w, h] = win->getImageSize(path);
                auto list = std::make_shared<std::vector<Value>>();
                list->push_back(Value::fromInt(w));
                list->push_back(Value::fromInt(h));
                Value result;
                result.type = ValueType::LIST;
                result.listVal = list;
                return result;
            };
            obj->fields["getImageSize"] = fn;
        }

        // ---- 3D Rendering Methods ----

        // .enable3D()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->enable3D();
                return Value::nil();
            };
            obj->fields["enable3D"] = fn;
        }

        // .disable3D()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->disable3D();
                return Value::nil();
            };
            obj->fields["disable3D"] = fn;
        }

        // .clearDepth()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->clearDepth();
                return Value::nil();
            };
            obj->fields["clearDepth"] = fn;
        }

        // .setPerspective(fov, aspect, near, far)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                float fov = args.size() > 0 ? (float)args[0].toNumber() : 60.0f;
                float aspect = args.size() > 1 ? (float)args[1].toNumber() : 1.0f;
                float nearPlane = args.size() > 2 ? (float)args[2].toNumber() : 0.1f;
                float farPlane = args.size() > 3 ? (float)args[3].toNumber() : 100.0f;
                win->setPerspective(fov, aspect, nearPlane, farPlane);
                return Value::nil();
            };
            obj->fields["setPerspective"] = fn;
        }

        // .setCamera(eyeX, eyeY, eyeZ, lookX, lookY, lookZ, upX, upY, upZ)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                float eyeX = args.size() > 0 ? (float)args[0].toNumber() : 0.0f;
                float eyeY = args.size() > 1 ? (float)args[1].toNumber() : 0.0f;
                float eyeZ = args.size() > 2 ? (float)args[2].toNumber() : 0.0f;
                float lookX = args.size() > 3 ? (float)args[3].toNumber() : 0.0f;
                float lookY = args.size() > 4 ? (float)args[4].toNumber() : 0.0f;
                float lookZ = args.size() > 5 ? (float)args[5].toNumber() : 0.0f;
                float upX = args.size() > 6 ? (float)args[6].toNumber() : 0.0f;
                float upY = args.size() > 7 ? (float)args[7].toNumber() : 1.0f;
                float upZ = args.size() > 8 ? (float)args[8].toNumber() : 0.0f;
                win->setCamera(eyeX, eyeY, eyeZ, lookX, lookY, lookZ, upX, upY, upZ);
                return Value::nil();
            };
            obj->fields["setCamera"] = fn;
        }

        // .pushMatrix()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->pushMatrix();
                return Value::nil();
            };
            obj->fields["pushMatrix"] = fn;
        }

        // .popMatrix()
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                win->popMatrix();
                return Value::nil();
            };
            obj->fields["popMatrix"] = fn;
        }

        // .translate(x, y, z)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                float x = args.size() > 0 ? (float)args[0].toNumber() : 0.0f;
                float y = args.size() > 1 ? (float)args[1].toNumber() : 0.0f;
                float z = args.size() > 2 ? (float)args[2].toNumber() : 0.0f;
                win->translate(x, y, z);
                return Value::nil();
            };
            obj->fields["translate"] = fn;
        }

        // .rotate(angle, x, y, z)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                float angle = args.size() > 0 ? (float)args[0].toNumber() : 0.0f;
                float x = args.size() > 1 ? (float)args[1].toNumber() : 0.0f;
                float y = args.size() > 2 ? (float)args[2].toNumber() : 1.0f;
                float z = args.size() > 3 ? (float)args[3].toNumber() : 0.0f;
                win->rotate(angle, x, y, z);
                return Value::nil();
            };
            obj->fields["rotate"] = fn;
        }

        // .scale(x, y, z)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                float x = args.size() > 0 ? (float)args[0].toNumber() : 1.0f;
                float y = args.size() > 1 ? (float)args[1].toNumber() : 1.0f;
                float z = args.size() > 2 ? (float)args[2].toNumber() : 1.0f;
                win->scale(x, y, z);
                return Value::nil();
            };
            obj->fields["scale"] = fn;
        }

        // .loadTexture(path) -> textureID
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string path = args.size() > 0 ? args[0].toString() : "";
                int texID = win->loadTexture(path);
                return Value::fromInt(texID);
            };
            obj->fields["loadTexture"] = fn;
        }

        // .bindTexture(texID)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int texID = args.size() > 0 ? (int)args[0].toNumber() : 0;
                win->bindTexture(texID);
                return Value::nil();
            };
            obj->fields["bindTexture"] = fn;
        }

        // .drawTexturedCube(size, texID)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                float size = args.size() > 0 ? (float)args[0].toNumber() : 1.0f;
                int texID = args.size() > 1 ? (int)args[1].toNumber() : 0;
                win->drawTexturedCube(size, texID);
                return Value::nil();
            };
            obj->fields["drawTexturedCube"] = fn;
        }

        // .setColor(r, g, b, a=1.0)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                float r = args.size() > 0 ? (float)args[0].toNumber() : 1.0f;
                float g = args.size() > 1 ? (float)args[1].toNumber() : 1.0f;
                float b = args.size() > 2 ? (float)args[2].toNumber() : 1.0f;
                float a = args.size() > 3 ? (float)args[3].toNumber() : 1.0f;
                win->setColor(r, g, b, a);
                return Value::nil();
            };
            obj->fields["setColor"] = fn;
        }

        // .drawQuad(x1,y1,z1, x2,y2,z2, x3,y3,z3, x4,y4,z4)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                if (args.size() < 12) return Value::nil();
                win->drawQuad(
                    (float)args[0].toNumber(), (float)args[1].toNumber(), (float)args[2].toNumber(),
                    (float)args[3].toNumber(), (float)args[4].toNumber(), (float)args[5].toNumber(),
                    (float)args[6].toNumber(), (float)args[7].toNumber(), (float)args[8].toNumber(),
                    (float)args[9].toNumber(), (float)args[10].toNumber(), (float)args[11].toNumber()
                );
                return Value::nil();
            };
            obj->fields["drawQuad"] = fn;
        }

        // .keyPressed(key) -> bool
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string key = args.size() > 0 ? args[0].toString() : "";
                bool pressed = win->keyPressed(key);
                return Value::fromBool(pressed);
            };
            obj->fields["keyPressed"] = fn;
        }

        // .getMousePos() -> [x, y]
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value>) -> Value {
                auto pos = win->getMousePos();
                auto list = std::make_shared<std::vector<Value>>();
                list->push_back(Value::fromInt(pos[0]));
                list->push_back(Value::fromInt(pos[1]));
                Value result;
                result.type = ValueType::LIST;
                result.listVal = list;
                return result;
            };
            obj->fields["getMousePos"] = fn;
        }

        // .setMousePos(x, y)
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int x = args.size() > 0 ? (int)args[0].toNumber() : 0;
                int y = args.size() > 1 ? (int)args[1].toNumber() : 0;
                win->setMousePos(x, y);
                return Value::nil();
            };
            obj->fields["setMousePos"] = fn;
        }

        // .setCursorMode(mode) - "normal", "hidden", "disabled"
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                std::string mode = args.size() > 0 ? args[0].toString() : "normal";
                win->setCursorMode(mode);
                return Value::nil();
            };
            obj->fields["setCursorMode"] = fn;
        }

        // .mouseButtonPressed(button) -> bool
        {
            Value fn;
            fn.type = ValueType::NATIVE_FUNCTION;
            fn.nativeFn = [win](Interpreter&, std::vector<Value> args) -> Value {
                int button = args.size() > 0 ? (int)args[0].toNumber() : 0;
                bool pressed = win->mouseButtonPressed(button);
                return Value::fromBool(pressed);
            };
            obj->fields["mouseButtonPressed"] = fn;
        }

        Value result;
        result.type = ValueType::OBJECT;
        result.objectVal = obj;
        return result;
    };
    env->define("Window", windowCtor, "native_function");

    // ========================================================================
    // Input namespace — keyboard and mouse state
    // ========================================================================
    auto inputClass = std::make_shared<FluxClass>();
    inputClass->name = "Input";
    auto inputObj = std::make_shared<FluxObject>();
    inputObj->classDef = inputClass;

    // Input.keyPressed(key_name) -> bool
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) return Value::fromBool(false);
#ifdef FLUX_HAS_SDL2
            std::string key = args[0].toString();
            const Uint8* state = SDL_GetKeyboardState(nullptr);
            SDL_Scancode sc = SDL_GetScancodeFromName(key.c_str());
            return Value::fromBool(state[sc] != 0);
#elif defined(FLUX_HAS_GLFW)
            // GLFW requires a window handle — simplified version
            return Value::fromBool(false);
#else
            return Value::fromBool(false);
#endif
        };
        inputObj->fields["keyPressed"] = fn;
    }

    // Input.mouseX() -> int
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
#ifdef FLUX_HAS_SDL2
            int x, y;
            SDL_GetMouseState(&x, &y);
            return Value::fromInt(x);
#else
            return Value::fromInt(0);
#endif
        };
        inputObj->fields["mouseX"] = fn;
    }

    // Input.mouseY() -> int
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
#ifdef FLUX_HAS_SDL2
            int x, y;
            SDL_GetMouseState(&x, &y);
            return Value::fromInt(y);
#else
            return Value::fromInt(0);
#endif
        };
        inputObj->fields["mouseY"] = fn;
    }

    // Input.mouseDown(button) -> bool (0=left, 1=middle, 2=right)
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
#ifdef FLUX_HAS_SDL2
            int btn = args.empty() ? 0 : (int)args[0].toNumber();
            Uint32 state = SDL_GetMouseState(nullptr, nullptr);
            switch (btn) {
                case 0: return Value::fromBool((state & SDL_BUTTON_LMASK) != 0);
                case 1: return Value::fromBool((state & SDL_BUTTON_MMASK) != 0);
                case 2: return Value::fromBool((state & SDL_BUTTON_RMASK) != 0);
                default: return Value::fromBool(false);
            }
#else
            return Value::fromBool(false);
#endif
        };
        inputObj->fields["mouseDown"] = fn;
    }

    Value inputVal;
    inputVal.type = ValueType::OBJECT;
    inputVal.objectVal = inputObj;
    env->define("Input", inputVal, "object");
}
