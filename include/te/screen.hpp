#pragma once

#include <bitset>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Window;
struct SDL_Texture;
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
  Unknown,
  Intermediate,
  TerminatedByST,
};
class TTYInput {
 public:
  TTYInputType receive_char(uint8_t c);

  enum class InputState {
    Idle,
    Escape,
    CSI,
    WaitForST,
  };

  InputState input_state = InputState::Idle;
  char last_char_;
  std::vector<std::uint8_t> buffer_;
};


enum {
  CHAR_ATTR_BOLD = 0,
  CHAR_ATTR_FAINT,
  CHAR_ATTR_ITALIC,
  CHAR_ATTR_UNDERLINE,
  CHAR_ATTR_INVERT,
  CHAR_ATTR_CROSSED_OUT,


  // CSI ?0h
  CHAR_ATTR_CURSOR_APPLICATION_MODE,
  // CSI ?5h
  CHAR_ATTR_REVERSE_VIDEO,
  // CSI ?7h
  CHAR_ATTR_AUTO_WRAP_MODE,

  // CSI ?1049h
  CHAR_ATTR_XTERM_WINDOW_FOCUS_TRACKING,
  // CSI ?2004h
  CHAR_ATTR_XTERM_BLOCK_PASTE,

  CHAR_ATTR_COUNT
};

struct Char {
  void reset() {
    *this = Char();
  }
  char c = 0;
  Color fg_color = ColorWhite;
  Color bg_color = ColorBlack; // TODO

  std::bitset<CHAR_ATTR_COUNT> attr;
};

class FontCache {
 public:
  FontCache(SDL_Renderer *renderer, TTF_Font *font);
  SDL_Texture *at(std::tuple<uint32_t, char> pos) const {
    return fc.at(std::get<0>(pos))[std::get<1>(pos)];
  }
 private:
  std::unordered_map<uint32_t, std::vector<SDL_Texture*>> fc;
  SDL_Renderer *renderer_;
  TTF_Font *font_;
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
    current_screen_start_row = 0;
    cursor_row = 0;
    cursor_col = 0;
  }

  bool process_csi(const std::vector<uint8_t> &seq);
  void process_input();
  bool check_child_process();
  void write_pending_input_data(std::vector<uint8_t> &input_buffer);
  void resize(int w, int h);
  void normal_mode() {
    current_attrs.reset();
    current_attrs.set(CHAR_ATTR_AUTO_WRAP_MODE);
  }
 private:
  void new_line() {
    if (cursor_row == max_rows_ - 1) {
//      std::vector<std::vector<Char>> tmp(lines_.begin()+1, lines_.end());
//      lines_ = tmp;
      lines_.emplace_back(max_cols_);
      current_screen_start_row++;
    } else {
      cursor_row++;
    }
  }
  void carriage_return() {
    cursor_col = 0;
  }
  void fill_current_cursor(char c) {
    get_row(cursor_row)[cursor_col].c = c;
    get_row(cursor_row)[cursor_col].bg_color = current_bg_color;
    get_row(cursor_row)[cursor_col].fg_color = current_fg_color;
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
        get_row(i)[j].reset();
      }
    }
  }

  int current_screen_start_row = 0;
  std::vector<Char> &get_row(int row) {
    return lines_[current_screen_start_row + row];
  }

  void write_to_tty(const std::string &s);

  void clear_selection();

  // x,  y -> row, col
  std::tuple<int, int> window_to_console(int x, int y) const {
    return {y / glyph_height_, x / glyph_width_};
  }

  bool less_than(std::tuple<int, int> lhs, std::tuple<int, int> rhs) const {
    int n_lhs = std::get<0>(lhs) * max_cols_ + std::get<1>(lhs);
    int n_rhs = std::get<0>(rhs) * max_cols_ + std::get<1>(rhs);
    return n_lhs < n_rhs;
  }

  // is target in [start, end]
  bool in_range(std::tuple<int, int> start, std::tuple<int, int> end, std::tuple<int, int> target) const {
    int n_target = std::get<0>(target) * max_cols_ + std::get<1>(target);
    int n_start = std::get<0>(start) * max_cols_ + std::get<1>(start);
    int n_end = std::get<0>(end) * max_cols_ + std::get<1>(end);
    return (n_start <= n_target && n_target <= n_end) || (n_end <= n_target && n_target <= n_start);
  }

  void render_chars();
  void render_background_image();

  Color map_color(Color color) const {
    if (current_attrs.test(CHAR_ATTR_REVERSE_VIDEO)) {
      color.r = 0x100 - color.r;
      color.g = 0x100 - color.g;
      color.b = 0x100 - color.b;
    }

    return color;
  }
 private:
  ScreenConfig config_;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  TTF_Font *font_ = nullptr;

  std::unique_ptr<FontCache> font_cache_;

  // background image
  SDL_Texture *background_image_texture = nullptr;
  int background_image_width = 0, background_image_height = 0;
  // 0 - 255
  int background_image_opaque = 128;

  std::string window_title_ = "alex's te";
  std::vector<std::string> xterm_title_stack_;

  std::vector<std::vector<Char>> lines_;
  int tty_fd_ = -1;
  int child_pid_ = 0;

  // cursor related
  int cursor_row = 0, cursor_col = 0;
  Color current_bg_color = Color{0xff000000}, current_fg_color = Color{0xffffffff};
  // cursor rendering
  int cursor_flip = 0;
  std::chrono::high_resolution_clock::time_point cursor_last_time;
  Color cursor_color = ColorWhite, cursor_fg_color = ColorBrightBlack;
  bool cursor_blink = true;
  bool cursor_show = true;
  std::chrono::milliseconds blink_interval = std::chrono::milliseconds(300);

  // clipboard
  bool has_selection = false;
  bool mouse_left_button_down = false;
  int selection_start_row = 0, selection_start_col = 0;
  int selection_end_row = 0, selection_end_col = 0;
  Color selection_bg_color = Color{0xff666666}, selection_fg_color = Color{0xff111111};

  int glyph_height_, glyph_width_;
  std::bitset<CHAR_ATTR_COUNT> current_attrs;

  int resolution_w_, resolution_h_;
  int max_rows_;
  int max_cols_;

  std::ofstream log_stream;

  TTYInput tty_input_;
  std::array<char, 1024> input_buffer_;
};

}

