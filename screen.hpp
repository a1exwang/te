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
constexpr Color ColorWhite = Color{0xffffffff};
constexpr Color ColorBlack = Color{0xff000000};
constexpr Color ColorRed = Color{0xffcc0000};
constexpr Color ColorGreen = Color{0xff00cc00};
constexpr Color ColorBlue = Color{0xff0000cc};
constexpr Color ColorYellow = Color{0xff888800};
constexpr Color ColorCyan = Color{0xff008888};
constexpr Color ColorMagnenta = Color{0xff880088};


struct ScreenConfig {
  std::string font_file = "/usr/share/fonts/TTF/DejaVuSansMono.ttf";
  int font_size = 36;

  int resolution_w = 1920;
  int resolution_h = 1080;
};

enum class TTYInputType {
  Char,
  CSI,
  Unknown,
  Intermediate
};
class TTYInput {
 public:
  TTYInputType receive_char(uint8_t c);

  enum class InputState {
    Idle,
    Escape,
    CSI,
  };

  InputState input_state = InputState::Idle;
  char last_char_;
  std::vector<std::uint8_t> csi_buffer_;
};

struct Char {
  char c = 0;
  Color fg_color = ColorWhite;
  Color bg_color = ColorBlack; // TODO
};

class Screen {
 public:
  explicit Screen(ScreenConfig config, char **envp);
  void loop();
  ~Screen();

  void resize_lines() {
    lines_.clear();
    for (int i = 0; i < max_lines_; i++) {
      lines_.emplace_back(max_cols_);
    }
    cursor_row = 0;
    cursor_col = 0;
  }

  void set_window_size(int w, int h);
  void process_csi(const std::vector<uint8_t> &seq);
  void process_input();
  bool check_child_process();
  void write_pending_input_data(std::vector<uint8_t> &input_buffer);
 private:
  void new_line() {
    if (cursor_row == max_lines_ - 1) {
      std::vector<std::vector<Char>> tmp(lines_.begin()+1, lines_.end());
      lines_ = tmp;
      lines_.emplace_back(max_cols_);
    } else {
      cursor_row++;
    }
    cursor_col = 0;
  }
  void next_cursor() {
    if (cursor_col == max_cols_ - 1) {
      new_line();
    } else {
      cursor_col++;
    }
  }

  Color get_default_fg_color() const {
    return ColorWhite;
  }
  Color get_default_bg_color() const {
    return ColorBlack;
  }
 private:
  ScreenConfig config_;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  TTF_Font *font_ = nullptr;

  std::vector<std::vector<Char>> lines_;
  int tty_fd_ = -1;
  int child_pid_ = 0;
  int max_lines_ = 40;
  int max_cols_ = 80;
  int cursor_row = 0, cursor_col = 0;
  Color current_bg_color = Color{0xff000000}, current_fg_color = Color{0xffffffff};
  int glyph_height_, glyph_width_;

  TTYInput tty_input_;
  std::array<char, 1024> input_buffer_;
};

}

