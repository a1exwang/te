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
#include <te/subprocess.hpp>
#include <te/tty_input.hpp>

struct SDL_Window;
struct SDL_Texture;
struct SDL_Renderer;
struct _TTF_Font;
typedef _TTF_Font TTF_Font;

namespace te {
class Screen;
class FontCache;
class Display {
 public:
  Display(std::ostream &log_stream,
          const std::vector<std::string> &args,
          const std::string &term_env,
          const std::string &font_file_path,
          int font_size,
          const std::string &background_image_path,
          const std::vector<std::string> &environment_variables,
          bool use_accleration);

  ~Display();

  void loop();

  // child process
  bool check_child_process();
  void process_input();
  void write_pending_input_data(std::vector<uint8_t> &input_buffer);
  void write_to_tty(std::string_view s) const;

  // screen
  void resize(int w, int h);

  // clipboard
  void clear_selection();
  std::string clipboard_copy();
  void clipboard_paste(std::string_view clipboard_text) const;

  // rendering
  void render_chars();
  void render_background_image();
  Color map_color(Color color) const;

  // utility functions

  void got_character(std::string c);

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

  void log_verbose_input_char(uint32_t c, bool has_color);

  void switch_screen(bool alternate_screen);


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
  TTYInput tty_input_;
  std::unique_ptr<Subprocess> subprocess_;

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
