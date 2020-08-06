#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <mutex>
#include <bitset>

struct SDL_Window;
struct SDL_Renderer;
struct _TTF_Font;
typedef _TTF_Font TTF_Font;

namespace te {

constexpr uint8_t ESC = 0x1bu;
static const char *CSI = "\x1b[";

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
constexpr Color ColorWhite = Color{0xffcccccc};
constexpr Color ColorBlack = Color{0xff000000};
constexpr Color ColorRed = Color{0xffcc0000};
constexpr Color ColorGreen = Color{0xff00cc00};
constexpr Color ColorBlue = Color{0xff0000cc};
constexpr Color ColorYellow = Color{0xff888800};
constexpr Color ColorCyan = Color{0xff008888};
constexpr Color ColorMagenta = Color{0xff880088};

constexpr Color ColorBrightWhite = Color{0xffeeeeee};
constexpr Color ColorBrightBlack = Color{0xff444444};
constexpr Color ColorBrightRed = Color{0xffee0000};
constexpr Color ColorBrightGreen = Color{0xff00ee00};
constexpr Color ColorBrightBlue = Color{0xff0000ee};
constexpr Color ColorBrightYellow = Color{0xffaaaa00};
constexpr Color ColorBrightCyan = Color{0xff00aaaa};
constexpr Color ColorBrightMagenta = Color{0xffaa00aa};


struct ScreenConfig {
  std::string font_file = "/usr/share/fonts/TTF/DejaVuSansMono.ttf";
  int font_size = 36;
};

enum class TTYInputType {
  Char,
  CSI,
  OSC,
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
    OSC,
  };

  InputState input_state = InputState::Idle;
  char last_char_;
  std::vector<std::uint8_t> buffer_;
};

struct Char {
  void reset() {
    *this = Char();
  }
  char c = 0;
  Color fg_color = ColorWhite;
  Color bg_color = ColorBlack; // TODO
  bool bold = false;
};

class Screen {
 public:
  explicit Screen(ScreenConfig config, char **envp);
  void loop();
  ~Screen();

  void reset_tty_buffer() {
    lines_.clear();
    for (int i = 0; i < max_rows_; i++) {
      lines_.emplace_back(max_cols_);
    }
    cursor_row = 0;
    cursor_col = 0;
  }

  bool process_csi(const std::vector<uint8_t> &seq);
  void process_input();
  bool check_child_process();
  void write_pending_input_data(std::vector<uint8_t> &input_buffer);
  void resize(int w, int h);
 private:
  void new_line() {
    if (cursor_row == max_rows_ - 1) {
      std::vector<std::vector<Char>> tmp(lines_.begin()+1, lines_.end());
      lines_ = tmp;
      lines_.emplace_back(max_cols_);
    } else {
      cursor_row++;
    }
  }
  void next_cursor() {
    if (cursor_col < max_cols_) {
      cursor_col++;
    }
  }

  Color get_default_fg_color() const {
    return ColorWhite;
  }
  Color get_default_bg_color() const {
    return ColorBlack;
  }

  // both including
  void clear_screen(int from_row, int from_col, int to_row, int to_col) {
    for (int i = from_row; i <= to_row ; i++) {
      for (int j = from_col; j <= to_col; j++) {
        lines_[i][j].reset();
      }
    }
  }

  void write_to_tty(const std::string &s);
 private:
  ScreenConfig config_;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  TTF_Font *font_ = nullptr;

  std::vector<std::vector<Char>> lines_;
  int tty_fd_ = -1;
  int child_pid_ = 0;

  // cursor related
  int cursor_row = 0, cursor_col = 0;
  Color current_bg_color = Color{0xff000000}, current_fg_color = Color{0xffffffff};
  // cursor rendering
  int cursor_flip = 0;
  std::chrono::high_resolution_clock::time_point cursor_last_time;
  Color cursor_color = ColorWhite;
  bool cursor_blink = false;
  bool cursor_show = true;
  std::chrono::milliseconds blink_interval = std::chrono::milliseconds(300);

  int glyph_height_, glyph_width_;
  enum {
    CHAR_ATTR_BOLD = 0,
    CHAR_ATTR_FAINT,
    CHAR_ATTR_ITALIC,
    CHAR_ATTR_UNDERLINE,
    CHAR_ATTR_INVERT,
    CHAR_ATTR_CROSSED_OUT,
  };
  std::bitset<32> current_attrs;

  int resolution_w_, resolution_h_;
  int max_rows_;
  int max_cols_;




  TTYInput tty_input_;
  std::array<char, 1024> input_buffer_;
};

}

