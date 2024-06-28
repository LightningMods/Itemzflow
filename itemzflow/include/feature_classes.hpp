#pragma once
#include "GLES2_common.h"
#include "log.h"
#include <csignal>
#include <freetype-gl.h> // links against libfreetype-gl
#include <fstream>
#include <iostream>
#include <math.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

void text_gl_extra(vertex_buffer_t *buffer, int num, GLuint texture, int idx,
                   vec4 col, GLuint sh_prog);

class Favorites {
public:
  void addFavorite(const std::string &game) { favorites_.insert(game); }

  void removeFavorite(const std::string &tid) { favorites_.erase(tid); }

  bool isFavorite(const item_t &game)
      const { // THIS FUNCTION IS NOT USED AFTER LOADING THE INI GAMES LIST
    return favorites_.find(game.info.id) != favorites_.end();
  }

  bool saveToFile(const std::string &filename) const {
    std::ofstream file(filename);
    if (!file) {
      log_error("Error writing file: %s", filename.c_str());
      return false;
    }
    for (const auto &game_name : favorites_) {
      file << game_name << std::endl;
    }
    return true;
  }

  bool loadFromFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file) {
      log_error("Error opening file for reading: %s", filename.c_str());
      return false;
    }
    favorites_.clear();
    std::string line;
    while (std::getline(file, line)) {
      favorites_.insert(line);
    }
    return true;
  }

private:
  std::unordered_set<std::string> favorites_;
};

class SafeString {
public:
  void set(const std::string &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = value;
  }

  std::string get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
  }

private:
  mutable std::mutex mutex_;
  std::string value_;
};

extern SafeString title_id;

typedef struct {
  float s, t;
} st;
typedef struct {
  float x, y, z;
} xyz;
typedef struct {
  float r, g, b, a;
} rgba;

typedef struct {
  xyz position, normal;
  rgba color;
} v3f3f4f_t;

typedef struct {
  xyz position;
  st tex_uv;
  rgba color;
} v3f2f4f_t;

// then we can print them splitted
typedef struct {
  int offset;
  int count;
} textline_t;

// ------------------------------------------------------- typedef & struct ---
typedef struct {
  float x, y, z;    // position (3f)
  float s, t;       // texture  (2f)
  float r, g, b, a; // color    (4f)
} vertex_t;

void ftgl_render_vbo(vertex_buffer_t *vbo, vec3 *offset);

extern GLuint sh_prog1, sh_prog2, sh_prog3, sh_prog4, sh_prog5;

class VertexBuffer {
public:
  std::unique_ptr<vertex_buffer_t, decltype(&vertex_buffer_delete)> buffer{
      nullptr, vertex_buffer_delete};

  VertexBuffer() = default;
  vec4 c[8];

  VertexBuffer(const char *format) {
    if (!buffer)
      buffer.reset(vertex_buffer_new(format));
  }

  void render(GLenum mode) {
    if (buffer)
      vertex_buffer_render(buffer.get(), mode);
    else
      log_error("vertex_buffer_render failed: Buffer is NULL");
  }

  void render_setup(GLenum mode) {
    if (buffer)
      vertex_buffer_render_setup(buffer.get(), mode);
  }

  void render_item(int numb) {
    if (buffer)
      vertex_buffer_render_item(buffer.get(), numb);
  }

  void render_finish() {
    if (buffer)
      vertex_buffer_render_finish(buffer.get());
  }

  // You might need to replace texture_font_t and texture_glyph_t
  // with your actual definitions
  void add_text(struct texture_font_t *font, std::string text, vec4 &color,
                vec2 &pen) {
    if (!font || !buffer) {
      log_error("add_text failed: Font is %s, Buffer is %s",
                font ? "OK" : "NULL", buffer ? "OK" : "NULL");
      return;
    }

    // log_info("message: %s", text.c_str());
    size_t i;
    float r = color.r, g = color.g, b = color.b, a = color.a;

    for (i = 0; i < text.length(); ++i) {
      texture_glyph_t *glyph =
          texture_font_get_glyph(font, (const char *)&text.at(i));
      if (glyph) {
        float kerning = 0.0f;
        if (i > 0)
          kerning =
              texture_glyph_get_kerning(glyph, (const char *)&text.at(i - 1));
        pen.x += kerning;
        int x0 = (int)(pen.x + glyph->offset_x);
        int y0 = (int)(pen.y + glyph->offset_y);
        int x1 = (int)(x0 + glyph->width);
        int y1 = (int)(y0 - glyph->height);
        float s0 = glyph->s0;
        float t0 = glyph->t0;
        float s1 = glyph->s1;
        float t1 = glyph->t1;
        GLuint indices[6] = {0, 1, 2, 0, 2, 3}; // (two triangles)
        /* VBO is setup as: "vertex:3f, tex_coord:2f, color:4f" */
        vertex_t vertices[4];

        vertices[0].x = vertices[1].x = x0;
        vertices[2].x = vertices[3].x = x1;

        vertices[0].y = vertices[3].y = y0;
        vertices[1].y = vertices[2].y = y1;

        vertices[0].z = vertices[1].z = vertices[2].z = vertices[3].z = 0;

        vertices[1].s = vertices[0].s = s0;
        vertices[2].s = vertices[3].s = s1;

        vertices[1].t = vertices[2].t = t1;
        vertices[0].t = vertices[3].t = t0;

        vertices[0].r = vertices[1].r = vertices[2].r = vertices[3].r = r;
        vertices[0].g = vertices[1].g = vertices[2].g = vertices[3].g = g;
        vertices[0].b = vertices[1].b = vertices[2].b = vertices[3].b = b;
        vertices[0].a = vertices[1].a = vertices[2].a = vertices[3].a = a;

        vertex_buffer_push_back(buffer.get(), vertices, 4, indices, 6);
        pen.x += glyph->advance_x;
      }
    }
  }

  void render_vbo(vec3 *offset) {
    if (buffer)
      ftgl_render_vbo(buffer.get(), offset);
  }

  void clear() {
    if (buffer)
      buffer.reset();
  }

  void push_back(const void *vertices, size_t vcount, const GLuint *indices,
                 size_t icount) {
    if (buffer) {
      vertex_buffer_push_back(buffer.get(), vertices, vcount, indices, icount);
    }
  }

  bool empty() const { return buffer == nullptr; }

  explicit operator bool() const { return static_cast<bool>(buffer); }

  void append_to_vbo(vec3 *v) {
    float s0 = 0., t0 = 0., s1 = 1., t1 = 1.;
    GLuint idx[6] = {0, 1, 2, 0, 2, 3};
    v3f2f4f_t vtx[4];

    vtx[0].position = (xyz){(v + 0)->x, (v + 0)->y, (v + 0)->z};
    vtx[0].color = (rgba){c[0].r, c[0].g, c[0].b, c[0].a};
    vtx[0].tex_uv = (st){s1, t0};

    vtx[1].position = (xyz){(v + 1)->x, (v + 1)->y, (v + 1)->z};
    vtx[1].color = (rgba){c[1].r, c[1].g, c[1].b, c[1].a};
    vtx[1].tex_uv = (st){s0, t0};

    vtx[2].position = (xyz){(v + 2)->x, (v + 2)->y, (v + 2)->z};
    vtx[2].color = (rgba){c[2].r, c[2].g, c[2].b, c[2].a};
    vtx[2].tex_uv = (st){s0, t1};

    vtx[3].position = (xyz){(v + 3)->x, (v + 3)->y, (v + 3)->z};
    vtx[3].color = (rgba){c[3].r, c[3].g, c[3].b, c[3].a};
    vtx[3].tex_uv = (st){s1, t1};

    vertex_buffer_push_back(buffer.get(), vtx, 4, idx, 6);
  }

  void render_tex(GLuint texture, int idx, vec4 col) {
    // we already clean in main renderloop()!

    GLuint sh_prog = sh_prog2;
    if (idx == -1) // reflection
      sh_prog = sh_prog3;
    else // fake shadow
      if (idx == -2)
        sh_prog = sh_prog4;
      else if (idx == -3)
        sh_prog = sh_prog5;

    glUseProgram(sh_prog);
    text_gl_extra(buffer.get(), 2, texture, idx, col, sh_prog);
  }
};

class Base64 {
public:
  static std::string Encode(const std::string data) {
    static constexpr char sEncodingTable[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

    size_t in_len = data.size();
    size_t out_len = 4 * ((in_len + 2) / 3);
    std::string ret(out_len, '\0');
    size_t i;
    char *p = const_cast<char *>(ret.c_str());

    for (i = 0; i < in_len - 2; i += 3) {
      *p++ = sEncodingTable[(data[i] >> 2) & 0x3F];
      *p++ = sEncodingTable[((data[i] & 0x3) << 4) |
                            ((int)(data[i + 1] & 0xF0) >> 4)];
      *p++ = sEncodingTable[((data[i + 1] & 0xF) << 2) |
                            ((int)(data[i + 2] & 0xC0) >> 6)];
      *p++ = sEncodingTable[data[i + 2] & 0x3F];
    }
    if (i < in_len) {
      *p++ = sEncodingTable[(data[i] >> 2) & 0x3F];
      if (i == (in_len - 1)) {
        *p++ = sEncodingTable[((data[i] & 0x3) << 4)];
        *p++ = '=';
      } else {
        *p++ = sEncodingTable[((data[i] & 0x3) << 4) |
                              ((int)(data[i + 1] & 0xF0) >> 4)];
        *p++ = sEncodingTable[((data[i + 1] & 0xF) << 2)];
      }
      *p++ = '=';
    }

    return ret;
  }

  static std::string Decode(const std::string &input, std::string &out) {
    static constexpr unsigned char kDecodingTable[] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,
        7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64};

    size_t in_len = input.size();
    if (in_len % 4 != 0)
      return "Input data size is not a multiple of 4";

    size_t out_len = in_len / 4 * 3;
    if (input[in_len - 1] == '=')
      out_len--;
    if (input[in_len - 2] == '=')
      out_len--;

    out.resize(out_len);

    for (size_t i = 0, j = 0; i < in_len;) {
      uint32_t a = input[i] == '='
                       ? 0 & i++
                       : kDecodingTable[static_cast<int>(input[i++])];
      uint32_t b = input[i] == '='
                       ? 0 & i++
                       : kDecodingTable[static_cast<int>(input[i++])];
      uint32_t c = input[i] == '='
                       ? 0 & i++
                       : kDecodingTable[static_cast<int>(input[i++])];
      uint32_t d = input[i] == '='
                       ? 0 & i++
                       : kDecodingTable[static_cast<int>(input[i++])];

      uint32_t triple =
          (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

      if (j < out_len)
        out[j++] = (triple >> 2 * 8) & 0xFF;
      if (j < out_len)
        out[j++] = (triple >> 1 * 8) & 0xFF;
      if (j < out_len)
        out[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return "";
  }
};

template <typename T> class ThreadSafeVector {
private:
  std::vector<T> data;
  mutable std::mutex mtx;

public:
  ThreadSafeVector() = default;
  ThreadSafeVector(size_t n) {
    std::lock_guard<std::mutex> lock(mtx);
    data.resize(n);
  }
  void push_back(const T &value) {
    std::lock_guard<std::mutex> lock(mtx);
    data.push_back(value);
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mtx);
    return data.empty();
  }

  T &operator[](size_t i) {
    static T default_value;
    std::lock_guard<std::mutex> lock(mtx);
    if (data.empty() || i >= data.size() || i < 0) {
      log_error("ThreadSafeVector: Out of bounds");
      return default_value; // Return the default value if the index is out of
                            // bounds
    }
    //log_info("ThreadSafeVector: %i, size %i", i, data.size());
    return data.at(i);
  }

  const T &operator[](size_t i) const {
    static T default_value;
    std::lock_guard<std::mutex> lock(mtx);
    if (data.empty() || i >= data.size() || i < 0) {
      log_error("ThreadSafeVector: Out of bounds");
      return default_value; // Return the default value if the index is out of
                            // bounds
    }
    return data.at(i);
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mtx);
    return data.size();
  }

  ThreadSafeVector(const ThreadSafeVector &other) {
    std::lock_guard<std::mutex> lock_other(other.mtx);
    data = other.data;
  }

  ThreadSafeVector &operator=(const ThreadSafeVector &other) {
    if (&other == this) {
      return *this;
    }

    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    std::unique_lock<std::mutex> lock_other(other.mtx, std::defer_lock);
    std::lock(lock, lock_other);
    data = other.data;

    return *this;
  }
  operator std::vector<T>() const {
    std::lock_guard<std::mutex> lock(mtx);
    return data;
  }

  ThreadSafeVector &operator=(const std::vector<T> &other) {
    std::lock_guard<std::mutex> lock(mtx);
    data = other;
    return *this;
  }

  typename std::vector<T>::iterator begin() {
    std::lock_guard<std::mutex> lock(mtx);
    return data.begin();
  }

  typename std::vector<T>::iterator end() {
    std::lock_guard<std::mutex> lock(mtx);
    return data.end();
  }

  typename std::vector<T>::const_iterator begin() const {
    std::lock_guard<std::mutex> lock(mtx);
    return data.begin();
  }

  typename std::vector<T>::const_iterator end() const {
    std::lock_guard<std::mutex> lock(mtx);
    return data.end();
  }

  T &at(size_t i) {
    static T default_value;
    std::lock_guard<std::mutex> lock(mtx);
    if (data.size() < i) {
      log_error("ThreadSafeVector: Out of bounds");
      return default_value; // Return the default value if the index is out of
                            // bounds
    }
    return data.at(i);
  }

  const T &at(size_t i) const {
    static T default_value;
    std::lock_guard<std::mutex> lock(mtx);
    if (data.size() < i) {
      log_error("ThreadSafeVector: Out of bounds");
      return default_value; // Return the default value if the index is out of
                            // bounds
    }
    return data.at(i);
  }

  void resize(size_t new_size) {
    // log_info("resize to %i", new_size);
    std::lock_guard<std::mutex> lock(mtx);
    data.resize(new_size);
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mtx);
    data.clear();
  }
};

class Timer {
public:
  Timer() { m_StartTimepoint = std::chrono::high_resolution_clock::now(); }

  ~Timer() { Stop(); }

  void Stop() {
    if (has_stopped)
      return;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - m_StartTimepoint);
    double ms = duration.count() * 0.001;
    seconds = ms / 1000.0;
    minutes = seconds / 60.0;

    log_info("Operation took %.3f ms", ms); // Log with higher precision
    has_stopped = true;
  }

  double GetTotalMinutes() {
    Stop();
    return minutes;
  }

  double GetFractionalMinutes() {
    Stop();
    double wholeMinutes;
    double fractionalMinutes = modf(minutes, &wholeMinutes);
    return fractionalMinutes *
           60.0; // Convert fractional minutes to seconds for display
  }

private:
  std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
  double seconds = 0.0;
  double minutes = 0.0;
  bool has_stopped = false;
};