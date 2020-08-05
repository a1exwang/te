#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <mutex>

struct SDL_Window;
struct SDL_Renderer;
struct _TTF_Font;
typedef _TTF_Font TTF_Font;

namespace te {

#pragma pack(push, 1)
union Color {
  uint32_t u32;
  struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
  };
};
#pragma pack(pop)

struct ScreenConfig {
  std::string font_file = "/usr/share/fonts/TTF/DejaVuSansMono.ttf";
  int font_size = 72;

  // ABGR little endian: 0xAABBGGRR
  Color background_color = Color{0xffffffff};
  Color foreground_color = Color{0xff000000};
};


class Screen {
 public:
  explicit Screen(ScreenConfig config);
  void loop();
  ~Screen();
 private:
  ScreenConfig config_;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  TTF_Font *font_ = nullptr;

  std::mutex lock_;
  std::vector<std::string> buffer_;
};

}

