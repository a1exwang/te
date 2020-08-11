#include <te/screen.hpp>

#include <cassert>

#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <te/display.hpp>

namespace te {
Screen::Screen(Display *display) :display_(display) {
  resize(display_->max_rows_, display_->max_cols_);
  normal_mode();
}

// 256 colors
Color ANSIStandardColors[] = {
    0xff000000, 0xff800000, 0xff008000, 0xff808000, 0xff000080, 0xff800080, 0xff008080, 0xffc0c0c0, 0xff808080,
    0xffff0000, 0xff00ff00, 0xffffff00, 0xff0000ff, 0xffff00ff, 0xff00ffff, 0xffffffff, 0xff000000, 0xff00005f,
    0xff000087, 0xff0000af, 0xff0000d7, 0xff0000ff, 0xff005f00, 0xff005f5f, 0xff005f87, 0xff005faf, 0xff005fd7,
    0xff005fff, 0xff008700, 0xff00875f, 0xff008787, 0xff0087af, 0xff0087d7, 0xff0087ff, 0xff00af00, 0xff00af5f,
    0xff00af87, 0xff00afaf, 0xff00afd7, 0xff00afff, 0xff00d700, 0xff00d75f, 0xff00d787, 0xff00d7af, 0xff00d7d7,
    0xff00d7ff, 0xff00ff00, 0xff00ff5f, 0xff00ff87, 0xff00ffaf, 0xff00ffd7, 0xff00ffff, 0xff5f0000, 0xff5f005f,
    0xff5f0087, 0xff5f00af, 0xff5f00d7, 0xff5f00ff, 0xff5f5f00, 0xff5f5f5f, 0xff5f5f87, 0xff5f5faf, 0xff5f5fd7,
    0xff5f5fff, 0xff5f8700, 0xff5f875f, 0xff5f8787, 0xff5f87af, 0xff5f87d7, 0xff5f87ff, 0xff5faf00, 0xff5faf5f,
    0xff5faf87, 0xff5fafaf, 0xff5fafd7, 0xff5fafff, 0xff5fd700, 0xff5fd75f, 0xff5fd787, 0xff5fd7af, 0xff5fd7d7,
    0xff5fd7ff, 0xff5fff00, 0xff5fff5f, 0xff5fff87, 0xff5fffaf, 0xff5fffd7, 0xff5fffff, 0xff870000, 0xff87005f,
    0xff870087, 0xff8700af, 0xff8700d7, 0xff8700ff, 0xff875f00, 0xff875f5f, 0xff875f87, 0xff875faf, 0xff875fd7,
    0xff875fff, 0xff878700, 0xff87875f, 0xff878787, 0xff8787af, 0xff8787d7, 0xff8787ff, 0xff87af00, 0xff87af5f,
    0xff87af87, 0xff87afaf, 0xff87afd7, 0xff87afff, 0xff87d700, 0xff87d75f, 0xff87d787, 0xff87d7af, 0xff87d7d7,
    0xff87d7ff, 0xff87ff00, 0xff87ff5f, 0xff87ff87, 0xff87ffaf, 0xff87ffd7, 0xff87ffff, 0xffaf0000, 0xffaf005f,
    0xffaf0087, 0xffaf00af, 0xffaf00d7, 0xffaf00ff, 0xffaf5f00, 0xffaf5f5f, 0xffaf5f87, 0xffaf5faf, 0xffaf5fd7,
    0xffaf5fff, 0xffaf8700, 0xffaf875f, 0xffaf8787, 0xffaf87af, 0xffaf87d7, 0xffaf87ff, 0xffafaf00, 0xffafaf5f,
    0xffafaf87, 0xffafafaf, 0xffafafd7, 0xffafafff, 0xffafd700, 0xffafd75f, 0xffafd787, 0xffafd7af, 0xffafd7d7,
    0xffafd7ff, 0xffafff00, 0xffafff5f, 0xffafff87, 0xffafffaf, 0xffafffd7, 0xffafffff, 0xffd70000, 0xffd7005f,
    0xffd70087, 0xffd700af, 0xffd700d7, 0xffd700ff, 0xffd75f00, 0xffd75f5f, 0xffd75f87, 0xffd75faf, 0xffd75fd7,
    0xffd75fff, 0xffd78700, 0xffd7875f, 0xffd78787, 0xffd787af, 0xffd787d7, 0xffd787ff, 0xffd7af00, 0xffd7af5f,
    0xffd7af87, 0xffd7afaf, 0xffd7afd7, 0xffd7afff, 0xffd7d700, 0xffd7d75f, 0xffd7d787, 0xffd7d7af, 0xffd7d7d7,
    0xffd7d7ff, 0xffd7ff00, 0xffd7ff5f, 0xffd7ff87, 0xffd7ffaf, 0xffd7ffd7, 0xffd7ffff, 0xffff0000, 0xffff005f,
    0xffff0087, 0xffff00af, 0xffff00d7, 0xffff00ff, 0xffff5f00, 0xffff5f5f, 0xffff5f87, 0xffff5faf, 0xffff5fd7,
    0xffff5fff, 0xffff8700, 0xffff875f, 0xffff8787, 0xffff87af, 0xffff87d7, 0xffff87ff, 0xffffaf00, 0xffffaf5f,
    0xffffaf87, 0xffffafaf, 0xffffafd7, 0xffffafff, 0xffffd700, 0xffffd75f, 0xffffd787, 0xffffd7af, 0xffffd7d7,
    0xffffd7ff, 0xffffff00, 0xffffff5f, 0xffffff87, 0xffffffaf, 0xffffffd7, 0xffffffff, 0xff080808, 0xff121212,
    0xff1c1c1c, 0xff262626, 0xff303030, 0xff3a3a3a, 0xff444444, 0xff4e4e4e, 0xff585858, 0xff626262, 0xff6c6c6c,
    0xff767676, 0xff808080, 0xff8a8a8a, 0xff949494, 0xff9e9e9e, 0xffa8a8a8, 0xffb2b2b2, 0xffbcbcbc, 0xffc6c6c6,
    0xffd0d0d0, 0xffdadada, 0xffe4e4e4, 0xffeeeeee
};

/**
 * ECMA-035
 *
 * 13.2 Types of escape sequences
 *
 * ESC TYPE ... F
 *
 * TYPE:
 * 0x0x/0x1x    .       Shall not be used
 * 0x2x         nF      (see table 3.b)
 * 0x3x         Fp      Private control function (see 6.5.3)
 * 0x4x/0x5x    Fe      Control function in the C1 set (see 6.4.3)
 * 0x6x/0x7x(except 0x7f)Fe Standardized single control function (see 6.5.1)
 *
 *
 * 13.2.2 Escape Sequences of types nF
 * ESC I .. F where the notation ".." indicates that more than one Intermediate Byte may appear in the sequence.
 *
 */

// 0123456789:;<=>?
bool csi_is_parameter_byte(uint8_t c) {
  return 0x30u == (c & 0xf0u);
}

// !"#$%&'()*+,-./
bool csi_is_intermediate_byte(uint8_t c) {
  return 0x20u == (c & 0xf0u);
}

bool csi_is_final_byte(uint8_t c) {
  return c >= 0x40 && c <= 0x7E;
}

std::vector<std::function<void()>> CSI_FinalBytes = {
    nullptr,
};

// ST: String Terminator 0x9c or ESC 0x5c, could also be 0x07 in xterm

TTYInputType TTYInput::receive_char(uint8_t c) {
  if (input_state == InputState::Escape) {
    if (c == '[') {
      // CSI
      buffer_.clear();
      input_state = InputState::CSI;
      return TTYInputType::Intermediate;
    } else if (c == 0x1b) {
      // multiple ESC
      input_state = InputState::Escape;
      return TTYInputType::Intermediate;
    } else if (c == 'P' || c == ']') {
      buffer_.clear();
      buffer_.push_back(c);
      input_state = InputState::WaitForST;
      return TTYInputType::Intermediate;
    } else {
      input_state = InputState::Idle;
      return TTYInputType::Char;
    }
  } else if (input_state == InputState::Idle) {
    if (c == 0x1b) {
      input_state = InputState::Escape;
      return TTYInputType::Intermediate;
    } else {
      return TTYInputType::Char;
    }
  } else if (input_state == InputState::CSI) {
    // CSI:
    buffer_.push_back(c);

    if (csi_is_final_byte(c)) {
      input_state = InputState::Idle;
      return TTYInputType::CSI;
    } else {
      return TTYInputType::Intermediate;
    }
  } else if (input_state == InputState::WaitForST) {
    if (c == '\a') {
      // xterm ST
      input_state = InputState::Idle;
      return TTYInputType::TerminatedByST;
    } else if (c == 0x9c) {
      input_state = InputState::Idle;
      return TTYInputType::TerminatedByST;
    } else if (c == '\\') {
      if (!buffer_.empty() && buffer_.back() == ESC) {
        // delete the previous ESC
        input_state = InputState::Idle;
        buffer_.pop_back();
        return TTYInputType::TerminatedByST;
      } else {
        buffer_.push_back(c);
        return TTYInputType::Intermediate;
      }
    } else {
      buffer_.push_back(c);
      return TTYInputType::Intermediate;
    }
  } else {
    assert(0);
    return TTYInputType::Unknown;
  }
}

// parse format 123;444;;22
std::vector<int> parse_csi_colon_ints(const std::vector<uint8_t> &seq, int start, int end, std::optional<int> default_value = 0) {
  if (start >= end) {
    if (default_value.has_value()) {
      return std::vector<int>(1, default_value.value());
    } else {
      return {};
    }
  }
  std::string s;
  std::vector<int> result;
  for (int i = start; i < end; i++) {
    if (std::isdigit(seq[i])) {
      s.push_back(seq[i]);
    } else if (seq[i] == ';') {
      if (s.empty()) {
        if (default_value.has_value()) {
          result.push_back(default_value.value());
        } else {
          result.push_back(0);
        }
      } else {
        result.push_back(std::stoi(s));
      }
      s.clear();
    }
  }
  if (s.empty()) {
    result.push_back(0);
  } else {
    result.push_back(std::stoi(s));
  }
  return result;
}

template <typename>
struct TupleAddOne;

template <typename... First>
struct TupleAddOne<std::tuple<First...>> {
  using type = std::tuple<First..., int>;
};

template <int N>
struct NTuple {
  using type = typename TupleAddOne<typename NTuple<N-1>::type>::type;
};

template <>
struct NTuple<0> {
  using type = std::tuple<>;
};

struct InvalidCSISeq :public std::exception {};


#include <type_traits>

template <typename F, size_t... Is>
auto gen_tuple_impl(F func, std::index_sequence<Is...> ) {
  return std::make_tuple(func(Is)...);
}

template <size_t N, typename F>
auto gen_tuple(F func) {
  return gen_tuple_impl(func, std::make_index_sequence<N>{} );
}

// Tp must be tuple<int, int ...>
template <typename Tp>
void copy_vector_to_tuple(const std::vector<int> &vec, Tp &tp) {
  constexpr int N = std::tuple_size_v<Tp>;
  tp = gen_tuple<N>([&vec](size_t i) { return vec[i]; });
}


template <int N>
typename NTuple<N>::type parse_csi_n(const std::vector<uint8_t> &seq, int start, int end, int default_value) {
  auto ints = parse_csi_colon_ints(seq, start, end, default_value);
  if (ints.size() != N) {
    throw InvalidCSISeq();
  }

  typename NTuple<N>::type tp;
  copy_vector_to_tuple(ints, tp);
  return tp;
}

template <int N>
typename NTuple<N>::type csi_n(const std::vector<uint8_t> &seq, int default_value) {
  return parse_csi_n<N>(seq, 0, seq.size() - 1, default_value);
}

Color ColorTable16[] = {
    ColorBlack,
    ColorRed,
    ColorGreen,
    ColorYellow,
    ColorBlue,
    ColorMagenta,
    ColorCyan,
    ColorWhite,
    ColorBrightBlack,
    ColorBrightRed,
    ColorBrightGreen,
    ColorBrightYellow,
    ColorBrightBlue,
    ColorBrightMagenta,
    ColorBrightCyan,
    ColorBrightWhite,
};

bool Screen::process_csi(const std::vector<uint8_t> &seq) {
  try {
    if (seq.empty()) {
      return false;
    }

    auto op = seq.back();
    if ('A' <= op && op <= 'D') {
      // CSI A up
      // CSI B down
      // CSI C forward
      // CSI D backward
      auto[n] = csi_n<1>(seq, 1);
      switch (op) {
        // TODO: report out of range
        case 'A':
          // up
          cursor_row -= n;
          if (cursor_row < 0) {
            cursor_row = 0;
          }
          break;
        case 'B':cursor_row += n;
          if (cursor_row > max_rows_ - 1) {
            cursor_row = max_rows_ - 1;
          }
          // down
          break;
        case 'C':cursor_col += n;
          if (cursor_col > max_cols_ - 1) {
            cursor_col = max_cols_ - 1;
          }
          // forward
          break;
        case 'D':
          // backward
          cursor_col -= n;
          if (cursor_col < 0) {
            cursor_col = 0;
          }
          break;
        default:assert(0);
      }
      return true;
    } else if (op == 'G') {
      auto [n] = csi_n<1>(seq, 1);
      cursor_col = n - 1;
      return true;
    } else if (op == 'H') {
      // move cursor to row:col
      auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
      if (ints.size() == 0) {
        cursor_row = cursor_col = 0;
        return true;
      } else if (ints.size() == 1) {
        if (ints[0] < 0 || ints[0] > max_cols_) {
          std::cerr << "Invalid CSI col H format: col out of range for: CSI " << ints[0] << std::endl;
          return false;
        }
        if (ints[0] == 0) {
          cursor_col = 0;
        } else {
          cursor_col = ints[1] - 1;
        }
        return true;
      } else if (ints.size() != 2) {
        std::cerr << "Invalid CSI row;col H format: CSI ";
        bool first = true;
        for (auto i : ints) {
          if (first) {
            first = false;
            std::cerr << i;
          } else {
            std::cerr << ";" << i;
          }
        }
        std::cerr << std::endl;
        return false;
      } else {
        // index are 1 based
        if (ints[0] <= 0 || ints[0] > max_rows_) {
          std::cerr << "Invalid CSI row:col, row overflow" << std::endl;
          return false;
        }
        if (ints[1] <= 0 || ints[1] > max_cols_) {
          std::cerr << "Invalid CSI row:col, col overflow" << std::endl;
          return false;
        }
        cursor_row = ints[0] - 1;
        cursor_col = ints[1] - 1;
        return true;
      }
    } else if (op == 'J') {
      // clear part of screen
      auto [code] = csi_n<1>(seq, 0);
      if (code == 0) {
        // to end of screen
        clear_screen(cursor_row, cursor_col, max_rows_ - 1, max_cols_ - 1);
        return true;
      } else if (code == 1) {
        // to begin of screen
        clear_screen(0, 0, cursor_row, cursor_col);
        return true;
      } else if (code == 2) {
        clear_screen(0, 0, cursor_row, cursor_col - 1);
        return true;
      } else {
        std::cerr << "Invalid clear screen code " << code << std::endl;
        return false;
      }
    } else if (op == 'K') {
      /**
       * CSI Ps K  Erase in Line (EL), VT100.
            Ps = 0  ⇒  Erase to Right (default).
            Ps = 1  ⇒  Erase to Left.
            Ps = 2  ⇒  Erase All.
       */
      auto [code] = csi_n<1>(seq, 0);
      if (code == 0) {
        clear_screen(cursor_row, cursor_col, cursor_row, max_cols_ - 1);
        return true;
      } else if (code == 1) {
        clear_screen(cursor_row, 0, cursor_row, cursor_col);
        return true;
      } else if (code == 2) {
        clear_screen(cursor_row, 0, cursor_row, max_cols_ - 1);
        return true;
      }

    } else if (op == 'L') {
      // TODO: insert lines
      // Should consider DCSM and VEM
//      auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1, 1);
//      if (ints.size() == 1) {
//        int n = ints[0];
//        clear_screen(cursor_row, 0, cursor_row + n - 1, max_cols_ - 1);
//        return true;
//      }

    } else if (op == 'S' || op == 'T') {
      // scroll
      auto [code] = csi_n<1>(seq, 1);
      int scroll_diff = ((op == 'S') ? -1 : 1) * code;
      if (current_screen_start_row + scroll_diff < 0) {
        current_screen_start_row = 0;
      } else if (current_screen_start_row + scroll_diff + max_rows_ > rows_.size()) {
        auto new_lines = rows_.size() - (current_screen_start_row + scroll_diff + max_rows_);
        for (int i = 0; i < new_lines; i++) {
          rows_.emplace_back(max_cols_);
        }

        current_screen_start_row += scroll_diff;
      } else {
        current_screen_start_row += scroll_diff;
      }
      return true;

    } else if (op == 'X') {
      // erase n chars from current
      auto [n] = csi_n<1>(seq, 1);
      int row = cursor_row;
      int col = cursor_col;
      int count = n;
      while (count > 0) {
        get_row(row)[col].reset();
        count--;
        col++;
        if (col == max_cols_) {
          row++;
        }
        if (row == max_cols_) {
          break;
        }
      }
      return true;

    } else if (op == 'c') {
      if (seq.front() == '>') {
        /**
         *
         CSI > Ps c
          Send Device Attributes (Secondary DA).
          Ps = 0  or omitted ⇒  request the terminal's identification
          code.  The response depends on the decTerminalID resource set-
          ting.  It should apply only to VT220 and up, but xterm extends
          this to VT100.
            ⇒  CSI  > Pp ; Pv ; Pc c
          where Pp denotes the terminal type
            Pp = 0  ⇒  "VT100".
            Pp = 1  ⇒  "VT220".
            Pp = 2  ⇒  "VT240" or "VT241".
            Pp = 1 8  ⇒  "VT330".
            Pp = 1 9  ⇒  "VT340".
            Pp = 2 4  ⇒  "VT320".
            Pp = 3 2  ⇒  "VT382".
            Pp = 4 1  ⇒  "VT420".
            Pp = 6 1  ⇒  "VT510".
            Pp = 6 4  ⇒  "VT520".
            Pp = 6 5  ⇒  "VT525".

          and Pv is the firmware version (for xterm, this was originally
          the XFree86 patch number, starting with 95).  In a DEC termi-
          nal, Pc indicates the ROM cartridge registration number and is
          always zero.
         */

        auto [code] = parse_csi_n<1>(seq, 1, seq.size()-1, 0);
        if (code == 0) {
          // Request terminal ID
          std::stringstream ss;
          // VT100 xterm95
          ss << CSI << ">0;95;0c";
          display_->write_to_tty(ss.str());
          return true;
        }

      } else {
        auto [code] = csi_n<1>(seq, 0);
        // CSI ? 1 ; 2 c
        display_->write_to_tty("\x1b[?1;2c");
        return true;
      }
    } else if (op == 'd') {
      auto [code] = csi_n<1>(seq, 0);
      cursor_row = code - 1;
      return true;
    } else if (op == 'h' || op == 'l') {
      // h: DEC Private Mode Set (DECSET).
      // l: DEC Private Mode Reet (DECRET).
      bool enable = op == 'h';
      if (seq.front() == '?') {
        // xterm mouse tracking in https://mudhalla.net/tintin/info/xterm/
        auto ints = parse_csi_colon_ints(seq, 1, seq.size() - 1);
        bool has_unknown = false;
        for (auto code : ints) {
          switch (code) {
            case 1:
              // Application Cursor Keys (DECCKM), VT100.
              current_attrs.set(CHAR_ATTR_CURSOR_APPLICATION_MODE, enable);
              break;
            case 3:
              // DECCOLM
              // set column mode
              clear_screen(0, 0, max_rows_ - 1, max_cols_ - 1);
              break;
            case 4:
              // scrolling mode, DECSCLM, we only support fast scrolling currently
              if (enable) {
                std::cerr << "Warning, does not support slow scrolling" << std::endl;
              }
              break;
            case 5:
              // reverse video DECSCNM
              current_attrs.set(CHAR_ATTR_REVERSE_VIDEO, enable);
              break;
            case 6:
              // DECCOM
              if (enable) {
                std::cerr << "Warning, does not support cursor origin mode";
              } else {
                cursor_col = 0;
                cursor_row = 0;
              }
              break;
            case 7:current_attrs.set(CHAR_ATTR_AUTO_WRAP_MODE, enable);
              break;
            case 12:
              // Start Blinking Cursor (AT&T 610).
              cursor_blink = enable;
              break;
            case 25:cursor_show = enable;
              break;
            case 1000:// mouse tracker: send/don't send Mouse X & Y on button press and release.
              // NOTE: we don't support mousing tracking
              break;
              // xterm extensions
            case 1004:current_attrs.set(CHAR_ATTR_XTERM_WINDOW_FOCUS_TRACKING, enable);
              break;
            case 47:
            case 1049:
              // switch to alternate buffer and save cursor
              // https://invisible-island.net/xterm/xterm.log.html#xterm_90
              // https://gitlab.gnome.org/GNOME/vte/-/blob/master/src/vteseq.cc#L527
              display_->switch_screen(enable);
              break;
            case 2004:
              // When you are in bracketed paste mode and you paste into your terminal the content will be wrapped by the sequences \e[200~ and  \e[201~.
              current_attrs.set(CHAR_ATTR_XTERM_BLOCK_PASTE, enable);
              break;
            default:has_unknown = true;
              break;
          }
        }
        return !has_unknown;
      } else {
        auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
        bool has_unknown = false;
        for (auto code : ints) {
          switch (code) {
            case 1:
              // Guarded area transfer GATM
              if (enable) {
                std::cerr << "Warning, we don't support GATM currently" << std::endl;
              }
              break;
            case 2:
              // Keyboard action KAM
              if (enable) {
                std::cerr << "Warning, we don't support KAM currently" << std::endl;
              }
              break;
            case 3:
              //  CONTROL REPRESENTATION MODE CRM
              if (enable) {
                std::cerr << "Warning, we don't support CRM control mode currently" << std::endl;
              }
              break;
            case 4:
              // Insert Mode/Replace Mode
              if (enable) {
                std::cerr << "Warning, we don't support insert mode currently" << std::endl;
              }
              break;
            case 5:
              // STATUS REPORT TRANSFER MODE
              if (enable) {
                std::cerr << "Warning, we don't support SRTM currently" << std::endl;
              }
              break;
            case 6:
              // Erasure Mode
              if (enable) {
                std::cerr << "Warning, we don't support erasure mode currently" << std::endl;
              }
              break;
            default:has_unknown = true;
              break;
          }
        }
        return !has_unknown;
      }
    } else if (op == 'm') {
      if (seq.front() == '>') {
        // Set/reset key modifier options (XTMODKEYS), xterm
        return true;
      } else {
        // set attributes
        auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
        if (ints.size() == 3) {
          // 256 colors
          if (ints[0] == 38 && ints[1] == 5) {
            // set foreground color
            current_fg_color = ANSIStandardColors[ints[2]];
            return true;
          } else if (ints[0] == 48 && ints[1] == 5) {
            current_bg_color = ANSIStandardColors[ints[2]];
            return true;
          }
        }

        bool found_unknown = false;
        for (auto i : ints) {
          switch (i) {
            case 0:
              //            std::cout << "CSI reset attributes" << std::endl;
              current_fg_color = get_default_fg_color();
              current_bg_color = get_default_bg_color();
              current_attrs.reset();
              break;
            case 1: // bold
              current_attrs.set(CHAR_ATTR_BOLD);
              break;
            case 2: // faint
              current_attrs.set(CHAR_ATTR_FAINT);
              break;
            case 3: // italic
              current_attrs.set(CHAR_ATTR_ITALIC);
              break;
            case 4:current_attrs.set(CHAR_ATTR_UNDERLINE);
              break;
            case 7:current_attrs.set(CHAR_ATTR_INVERT);
              break;
            case 9:current_attrs.set(CHAR_ATTR_CROSSED_OUT);
              break;
            case 27:current_attrs.reset(CHAR_ATTR_INVERT);
              break;
            case 29:current_attrs.reset(CHAR_ATTR_CROSSED_OUT);
              break;
            case 39:current_fg_color = get_default_fg_color();
              break;
            case 49:current_bg_color = get_default_bg_color();
              break;
            default:
              if (30 <= i && i < 38) {
                current_fg_color = ColorTable16[i - 30];
                //              std::cout << "CSI set fg color" << std::endl;
              } else if (40 <= i && i < 48) {
                current_bg_color = ColorTable16[i - 30];
                //              std::cout << "CSI set bg color" << std::endl;
              } else if (90 <= i && i < 98) {
                current_fg_color = ColorTable16[i - 90 + 8];
              } else if (100 <= i && i < 108) {
                current_fg_color = ColorTable16[i - 100 + 8];

              } else {
                found_unknown = true;
              }
          }
        }
        return !found_unknown;

      }
    } else if (op == 'n') {
      auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
      if (ints.size() == 1) {
        // ESC [6n
        if (ints[0] == 6) {
          // Reports the cursor position (CPR) to the application as
          // (as though typed at the keyboard) ESC[n;mR
          std::stringstream ss;
          ss << ESC << '[' << (cursor_row + 1) << ';' << (cursor_col + 1) << 'R';
          auto s = ss.str();
          display_->write_to_tty(s);
          return true;
        }
      }

    } else if (op == 'p') {
      if (seq.front() == '?') {
        // Set resource value pointerMode (XTSMPOINTER)
        return true;
      }
    } else if (op == 'r') {
      // Set Scrolling Region [top;bottom] (default = full size of window) (DECSTBM), VT100.
      return true;
    } else if (op == 't') {
      auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
      if (ints == std::vector<int>{22, 1}) {
        // push xterm icon title on stack
        return true;
      } else if (ints == std::vector<int>{22, 2}) {
        // push xterm window title on stack
        display_->xterm_title_stack_.push_back(display_->window_title_);
        return true;
      } else if (ints == std::vector<int>{23, 1}) {
        // pop xterm icon title from stack
        return true;
      } else if (ints == std::vector<int>{23, 2}) {
        // push xterm window title on stack
        if (!display_->xterm_title_stack_.empty()) {
          display_->window_title_ = display_->xterm_title_stack_.back();
          display_->xterm_title_stack_.pop_back();
        } else {
          std::cerr << "Warning, title stack empty" << std::endl;
        }
        return true;
      }
    }
    return false;
  } catch (const InvalidCSISeq &e) {
    std::cerr << "Unknown CSI Seq: " << e.what() << std::endl;
    return false;
  }
}


}
