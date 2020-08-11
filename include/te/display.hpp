#pragma once
#include <bitset>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <te/basic.hpp>

struct SDL_Window;
struct SDL_Texture;
struct SDL_Renderer;
struct _TTF_Font;
typedef _TTF_Font TTF_Font;

namespace te {

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


class Screen;
class Display {
 public:
  Display(std::ostream &log_stream,
          const std::string &font_file_path,
          int font_size,
          const std::vector<std::string> &environment_variables);

  ~Display();

  void loop();

  // child process
  bool check_child_process();
  void process_input();
  void write_pending_input_data(std::vector<uint8_t> &input_buffer);
  void write_to_tty(std::string_view s);

  // screen
  void resize(int w, int h);

  // clipboard
  void clear_selection();
  std::string clipboard_copy();
  void clipboard_paste(std::string_view clipboard_text);

  // rendering
  void render_chars();
  void render_background_image();
  Color map_color(Color color) const;

  // utility functions

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
  // x,  y -> row, col
  std::tuple<int, int> window_to_console(int x, int y) const {
    return {y / glyph_height_, x / glyph_width_};
  }


// private:

  // screens
  Screen *current_screen_;
  std::unique_ptr<Screen> default_screen_, alternate_screen_;

  // rendering
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  TTF_Font *font_ = nullptr;
  std::unique_ptr<FontCache> font_cache_;

  // child process
  int tty_fd_ = -1;
  int child_pid_ = 0;
  TTYInput tty_input_;

  // window title
  std::string window_title_ = "alex's te";
  std::vector<std::string> xterm_title_stack_;

  // background image
  SDL_Texture *background_image_texture = nullptr;
  int background_image_width = 0, background_image_height = 0;
  // 0 - 255
  int background_image_opaque = 128;

  // display sizes
  int glyph_height_, glyph_width_;
  int resolution_w_, resolution_h_;
  int max_rows_, max_cols_;

  // clipboard
  bool has_selection = false;
  bool mouse_left_button_down = false;
  int selection_start_row = 0, selection_start_col = 0;
  int selection_end_row = 0, selection_end_col = 0;
  Color selection_bg_color = Color{0xff666666}, selection_fg_color = Color{0xff111111};

  // misc
  std::ostream &log_stream_;
};

}
