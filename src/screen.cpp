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
#include "csi_helper.hpp"

namespace te {
Screen::Screen(Display *display) :display_(display) {
  resize(display_->max_rows_, display_->max_cols_);
  normal_mode();
}


// parse CSI sequence and act correctly
bool Screen::process_csi(const std::string &seq) {
  try {
    if (seq.empty()) {
      return false;
    }

    uint8_t op = seq.back();
    if ('A' <= op && op <= 'D') {
      // CSI A up
      // CSI B down
      // CSI C forward
      // CSI D backward
      auto[n] = csi_n<1>(seq, 1);
      switch (op) {
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
            current_fg_color = ColorTable256[ints[2]];
            return true;
          } else if (ints[0] == 48 && ints[1] == 5) {
            current_bg_color = ColorTable256[ints[2]];
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
