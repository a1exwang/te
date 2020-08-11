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

namespace te {

class Display;
class Screen {
 public:
  explicit Screen(Display *display);

  bool process_csi(const std::vector<uint8_t> &seq);

  void reset_tty_buffer() {
    rows_.clear();
    for (int i = 0; i < max_rows_; i++) {
      rows_.emplace_back(max_cols_);
    }
    current_screen_start_row = 0;
    cursor_row = 0;
    cursor_col = 0;
  }

  Color get_default_fg_color() const {
    return ColorWhite;
  }
  Color get_default_bg_color() const {
    return ColorBlack;
  }

  void resize(int rows, int cols) {
    max_rows_ = rows;
    max_cols_ = cols;
    reset_tty_buffer();
  }

  void normal_mode() {
    current_attrs.reset();
    current_attrs.set(CHAR_ATTR_AUTO_WRAP_MODE);
  }
// private:
  void new_line() {
    if (cursor_row == max_rows_ - 1) {
      rows_.emplace_back(max_cols_);
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

  // both including
  void clear_screen(int from_row, int from_col, int to_row, int to_col) {
    for (int i = from_row; i <= to_row ; i++) {
      for (int j = from_col; j <= to_col; j++) {
        get_row(i)[j].reset();
      }
    }
  }

  std::vector<Char> &get_row(int row) {
    return rows_[current_screen_start_row + row];
  }


// private:
  Display *display_;

  // screen buffer
  std::vector<std::vector<Char>> rows_;
  int max_cols_ = 80;
  int max_rows_ = 64;
  int current_screen_start_row = 0;

  // current status
  Color current_bg_color = Color{0xff000000}, current_fg_color = Color{0xffffffff};
  std::bitset<CHAR_ATTR_COUNT> current_attrs;

  // cursor position
  int cursor_row = 0, cursor_col = 0;

  // cursor rendering
  int cursor_flip = 0;
  std::chrono::high_resolution_clock::time_point cursor_last_time;
  Color cursor_color = ColorWhite, cursor_fg_color = ColorBrightBlack;
  bool cursor_blink = true;
  bool cursor_show = true;
  std::chrono::milliseconds blink_interval = std::chrono::milliseconds(300);
};

}
