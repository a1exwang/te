#include "screen.hpp"

#include <cassert>

#include <vector>
#include <string>
#include <span>
#include <iostream>
#include <functional>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <unordered_set>

#include <SDL2/SDL.h> // For Events
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_ttf.h>
#include "child.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>
#include <termio.h>

using std::cerr;
using std::endl;



namespace te {
void set_tty_window_size(int tty_fd, int cols, int rows, int res_w, int res_h) {
  winsize screen_size;
  screen_size.ws_col = cols;
  screen_size.ws_row = rows;
  screen_size.ws_xpixel = res_w;
  screen_size.ws_ypixel = res_h;

  if (ioctl(tty_fd, TIOCSWINSZ, &screen_size) < 0) {
    perror("ioctl TIOCSWINSZ");
    abort();
  }
}


Screen::Screen(ScreenConfig config, char **envp) : config_(std::move(config)) {

  log_stream.rdbuf()->pubsetbuf(0, 0);
  log_stream.open("a.log");

  if (SDL_Init( SDL_INIT_EVERYTHING ) != 0) {
    std::cerr << "Error initializing SDL: " << SDL_GetError() << std::endl;
    abort();
  }
  resolution_w_ = 1920;
  resolution_h_ = 1080;

  window_ = SDL_CreateWindow( "a1ex's te", 0, 0, resolution_w_, resolution_h_, SDL_WINDOW_SHOWN );
  if (window_ == nullptr) {
    cerr << "Error creating window: " << SDL_GetError()  << endl;
    abort();
  }

  SDL_SetWindowResizable(window_, SDL_TRUE);

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer_) {
    cerr << "Error creating renderer: " << SDL_GetError() << endl;
    abort();
  }

  if ( TTF_Init() < 0 ) {
    std::cerr << "Error intializing SDL_ttf: " << TTF_GetError() << std::endl;
    abort();
  }
  font_ = TTF_OpenFont(config_.font_file.c_str(), config_.font_size);
  if (!font_) {
    cerr << "Error loading font: " << TTF_GetError() << endl;
    abort();
  }

  if (!TTF_FontFaceIsFixedWidth(font_)) {
    cerr << "font is not Fixed width" << std::endl;
    abort();
  }

  int minx, maxx, miny, maxy, advance;
  if (TTF_GlyphMetrics(font_, 'a', &minx, &maxx, &miny, &maxy, &advance) != 0) {
    cerr << "Error TTF_GlyphMetrics " << TTF_GetError() << std::endl;
    abort();
  }

  glyph_width_ = advance;
  glyph_height_ = TTF_FontLineSkip(font_);

  max_rows_ = resolution_h_ / glyph_height_;
  max_cols_ = resolution_w_ / glyph_width_;

  std::string program = "/bin/bash";
  char *argv[] = {(char*)program.c_str(), nullptr};
  std::vector<char*> envps;
  std::vector<std::string> envs;
  for (int i = 0; envp[i]; i++) {
    if (std::string(envp[i]).starts_with("TERM=")) {
      envs.emplace_back("TERM=rxvt");
    } else {
      envs.emplace_back(envp[i]);
    }
  }
  for (auto &env : envs) {
    envps.push_back((char*)env.c_str());
  }
  envps.push_back(nullptr);
  std::tie(child_pid_, tty_fd_) = start_child(program, argv, envps.data());

  set_tty_window_size(tty_fd_, max_cols_, max_rows_, resolution_w_, resolution_h_);
  reset_tty_buffer();
}

SDL_Color to_sdl_color(Color color) {
  return SDL_Color{color.r, color.g, color.b, color.a};
}

char shift_table[] = {
    // 0
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

    // digit
    ')','!','@','#', '$','%','^','&', '*','(',0,':', 0,'+',0,0,

    // 0x40 upper case
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,'{', '|',']',0,0,
    '~',0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

static std::unordered_set<char> SDLInputLiterals = {
    0x09/* \t */, 0x0d/* Enter */,
    0x1b /* ESC */,
    0x20/* SPACE */
};

void Screen::loop() {

  SDL_Event event;
  std::chrono::high_resolution_clock::time_point last_t;
  std::vector<uint8_t> input_buffer;
  bool loop_continue = true;

  while (loop_continue) {
    bool has_input = false;
    auto t0 = std::chrono::high_resolution_clock::now();
    // Process SDL events
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          loop_continue = false;
          break;
        case SDL_WINDOWEVENT: {
          if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            int new_width = event.window.data1;
            int new_height = event.window.data2;
            resize(new_width, new_height);
          }
          break;
        }
        case SDL_MOUSEBUTTONDOWN: {
          if (event.button.button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
            has_selection = true;
            mouse_left_button_down = true;
            std::tie(selection_start_row, selection_start_col) = window_to_console(event.button.x, event.button.y);
            selection_end_row = selection_start_row;
            selection_end_col = selection_start_col;
          }
          break;
        }
        case SDL_MOUSEMOTION: {
          if (mouse_left_button_down) {
            std::tie(selection_end_row, selection_end_col) = window_to_console(event.button.x, event.button.y);
          }
          break;
        }
        case SDL_MOUSEBUTTONUP: {
          if (event.button.button == SDL_BUTTON(SDL_BUTTON_LEFT)) {
            mouse_left_button_down = false;
          }
          break;
        }
        case SDL_KEYDOWN:
          if (event.key.type == SDL_KEYDOWN) {
            has_input = true;
            auto c = event.key.keysym.sym;
            if (event.key.keysym.sym == SDL_KeyCode::SDLK_BACKSPACE) {
              // delete key
              input_buffer.push_back('\x7f');
              clear_selection();
            } else if (SDLInputLiterals.find(c) != SDLInputLiterals.end()) {
              clear_selection();
              input_buffer.push_back(c);
            } else if (c < 0x100 /*NOTE: if c >=0x100, isprint is UB*/ && std::isprint(c)) {
              auto mod = event.key.keysym.mod;
              if (mod == KMOD_LSHIFT || mod == KMOD_RSHIFT) {
                if (std::isalpha(c)) {
                  input_buffer.push_back(std::toupper(c));
                } else if (c < 0x80 && shift_table[c] != 0) {
                  input_buffer.push_back(shift_table[c]);
                } else {
                  input_buffer.push_back(c);
                }
              } else if (mod == KMOD_LCTRL || mod == KMOD_RCTRL) {
                if (std::isalpha(c)) {
                  input_buffer.push_back(c - 'a' + 1);
                } else if (0x5b <= c && c < 0x60) {
                  input_buffer.push_back(c - 0x40);
                } else {
                  input_buffer.push_back(c);
                }
              } else if (mod == (KMOD_LCTRL | KMOD_LSHIFT)) {
                // clipboard
                if (c == 'c' && has_selection) {
                  std::stringstream ss;
                  for (int i = selection_start_row; i <= selection_end_row; i++) {
                    for (int j = selection_start_col; j <= selection_end_col; j++) {
                      ss << lines_[i][j].c;
                    }
                    ss << std::endl;
                  }
                  SDL_SetClipboardText(ss.str().c_str());
                  clear_selection();
                } else if (c == 'v') {
                  auto clipboard_text = SDL_GetClipboardText();
                  if (clipboard_text) {
                    if (current_attrs.test(CHAR_ATTR_XTERM_BLOCK_PASTE)) {
                      write_to_tty("\1b[200~");
                    }
                    // UTF8
                    write_to_tty(clipboard_text);
                    if (current_attrs.test(CHAR_ATTR_XTERM_BLOCK_PASTE)) {
                      write_to_tty("\1b[201~");
                    }
                  }

                } else {
                  input_buffer.push_back(c);
                }
              } else {
                // pressing any normal key will disable to selection
                clear_selection();
                input_buffer.push_back(c);
              }
//              std::cout << "key '" << c << std::endl;
            } else {
//              cerr << "unknown key " << c << std::endl;
            }
          }
          break;
        }
      }
    }

    if (child_pid_) {
      loop_continue = check_child_process();
      write_pending_input_data(input_buffer);
      process_input();
    }

    // draw console
    SDL_SetRenderDrawColor(renderer_, 0,0,0,0xff);
    SDL_RenderClear(renderer_);

    for (int i = 0; i < lines_.size(); i++) {
      auto &line = lines_[i];
      for (int j = 0; j < line.size(); j++) {
        auto &c = line[j];
        SDL_Rect glyph_box{glyph_width_*j, glyph_height_*i, glyph_width_, glyph_height_};
        Color fg = c.fg_color, bg = c.bg_color;

        // draw cursor background
        if (i == cursor_row && j == cursor_col && cursor_show) {
          if (cursor_blink) {
            if (cursor_flip) {
              bg = cursor_color;
              fg = cursor_fg_color;
            }
            auto now = std::chrono::high_resolution_clock::now();
            if (now - cursor_last_time > blink_interval) {
              cursor_flip = !cursor_flip;
              cursor_last_time = now;
            }
          } else {
            bg = cursor_color;
            fg = cursor_fg_color;
          }
        } else if (has_selection) {
          auto start = std::make_tuple(selection_start_row, selection_start_col), end = std::make_tuple(selection_end_row, selection_end_col);
          if (in_range(start, end, std::make_tuple(i, j))) {
            bg = selection_bg_color;
            fg = selection_fg_color;
          }
        }


        // draw bg color
        SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(renderer_, &glyph_box);

        auto character = c.c;
        if (!std::isprint(c.c) && character != 0) {
          character = '?';
        }

        if (character != ' ' && character != 0) {
          uint32_t style = TTF_STYLE_NORMAL;
          if (c.attr.test(CHAR_ATTR_UNDERLINE)) {
            style |= TTF_STYLE_UNDERLINE;
          }
          if (c.attr.test(CHAR_ATTR_BOLD)) {
            style |= TTF_STYLE_BOLD;
          }
          if (c.attr.test(CHAR_ATTR_ITALIC)) {
            style |= TTF_STYLE_ITALIC;
          }
          if (TTF_GetFontStyle(font_) != style) {
            TTF_SetFontStyle(font_, style);
          }
          SDL_Surface* text_surf = TTF_RenderGlyph_Blended(font_, character, to_sdl_color(fg));
          if (!text_surf) {
            std::cerr << "Failed to TTF_RenderGlyph_Blended" << SDL_GetError() << std::endl;
            abort();
          }
          auto text = SDL_CreateTextureFromSurface(renderer_, text_surf);

          SDL_RenderCopy(renderer_, text, NULL, &glyph_box);
          SDL_DestroyTexture(text);
          SDL_FreeSurface(text_surf);
        }
      }
    }

    // Update window
    SDL_RenderPresent(renderer_);
    auto now = std::chrono::high_resolution_clock::now();
    if (has_input) {
      std::cout << "Input latency " << std::chrono::duration<float, std::milli>(now - t0).count() << "ms" << std::endl;
    }

    last_t = now;
  }
}

Screen::~Screen() {
  if (font_) {
    TTF_CloseFont(font_);
  }
  if (window_) {
    SDL_DestroyWindow(window_);
  }

  TTF_Quit();
  SDL_Quit();
}
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
  last_char_ = '?';
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
      last_char_ = c;
      input_state = InputState::Idle;
      return TTYInputType::Char;
    }
  } else if (input_state == InputState::Idle) {
    if (c == 0x1b) {
      input_state = InputState::Escape;
      return TTYInputType::Intermediate;
    } else {
      last_char_ = c;
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
    } else if (c =='\\') {
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
  }

}

// parse format 123;444;;22
std::vector<int> parse_csi_colon_ints(const std::vector<uint8_t> &seq, int start, int end) {
  if (start >= end) {
    return std::vector<int>(1, 0);
  }
  std::string s;
  std::vector<int> result;
  for (int i = start; i < end; i++) {
    if (std::isdigit(seq[i])) {
      s.push_back(seq[i]);
    } else if (seq[i] == ';') {
      if (s.empty()) {
        result.push_back(0);
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

Color pallete[] = {
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

  if (!seq.empty()) {
    auto op = seq.back();
    if ('A' <= op && op <= 'D') {
      auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
      if (ints.size() > 1) {
        std::cerr << "Invalid CSI n " << op << ": trailing ints" << std::endl;
        return false;
      }

      int n = ints[0];
      switch(op) {
        // TODO: report out of range
        case 'A':
          // up
          cursor_row -= n;
          if (cursor_row < 0) {
            cursor_row = 0;
          }
          break;
        case 'B':
          cursor_row += n;
          if (cursor_row > max_rows_ - 1) {
            cursor_row = max_rows_ - 1;
          }
          // down
          break;
        case 'C':
          cursor_col += n;
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
        default:
          assert(0);
      }
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
      auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
      if (ints.size() >= 2) {
        std::cerr << "Invalid clear screen sequence " << std::endl;
        return false;
      }
      auto code = ints[0];
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
      auto ints = parse_csi_colon_ints(seq, 1, seq.size() - 1);
      if (!ints.empty()) {
        if (ints[0] == 0) {
          clear_screen(cursor_row, cursor_col, cursor_row, max_cols_ - 1);
          return true;
        } else if (ints[0] == 1) {
          clear_screen(cursor_row, 0, cursor_row, cursor_col);
          return true;
        } else if (ints[0] == 2){
          clear_screen(cursor_row, 0, cursor_row, max_cols_ - 1);
          return true;
        }
      }

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

        auto ints = parse_csi_colon_ints(seq, 1, seq.size() - 1);
        if (!ints.empty()) {
          auto code = ints[0];
          if (code == 0) {
            // Request terminal ID
            std::stringstream ss;
            // VT100 xterm95
            ss << CSI << ">0;95;0c";
            write_to_tty(ss.str());
            return true;
          }
        }


      }
    } else if (op == 'h' || op == 'l') {
      if (seq.front() == '?') {
        // h: DEC Private Mode Set (DECSET).
        // l: DEC Private Mode Reet (DECRET).
        bool enable = op == 'h';

        // xterm mouse tracking in https://mudhalla.net/tintin/info/xterm/
        auto ints = parse_csi_colon_ints(seq, 1, seq.size() - 1);
        if (!ints.empty()) {
          auto code = ints[0];
          switch (code) {
            case 1:
              // Application Cursor Keys (DECCKM), VT100.
              current_attrs.set(CHAR_ATTR_CURSOR_APPLICATION_MODE);
              return false;
            case 12:
              // Start Blinking Cursor (AT&T 610).
              cursor_blink = enable;
              return true;
            case 25:
              cursor_show = enable;
              return true;

            // xterm extensions
            case 1004:
              current_attrs.set(CHAR_ATTR_XTERM_WINDOW_FOCUS_TRACKING, enable);
              return true;
            case 1049:
              // https://invisible-island.net/xterm/xterm.log.html#xterm_90
              return false;
            case 2004:
              // When you are in bracketed paste mode and you paste into your terminal the content will be wrapped by the sequences \e[200~ and  \e[201~.
              current_attrs.set(CHAR_ATTR_XTERM_BLOCK_PASTE, enable);
              return true;
          }
        }
      }
    } else if (op == 'm') {
      if (seq.front() == '>') {
        // Set/reset key modifier options (XTMODKEYS), xterm
        return true;
      } else {
        // set attributes
        auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
        assert(!ints.empty());
        bool has_unknown = false;
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
            case 4:
              current_attrs.set(CHAR_ATTR_UNDERLINE);
              break;
            case 7:
              current_attrs.set(CHAR_ATTR_INVERT);
              break;
            case 9:
              current_attrs.set(CHAR_ATTR_CROSSED_OUT);
              break;
            case 27:
              current_attrs.reset(CHAR_ATTR_INVERT);
              break;
            case 29:
              current_attrs.reset(CHAR_ATTR_CROSSED_OUT);
              break;
            default:
              if (30 <= i && i < 38) {
                current_fg_color = pallete[i - 30];
                //              std::cout << "CSI set fg color" << std::endl;
              } else if (40 <= i && i < 48) {
                current_bg_color = pallete[i - 30];
                //              std::cout << "CSI set bg color" << std::endl;
              } else if (90 <= i && i < 98) {
                current_fg_color = pallete[i - 90 + 8];
              } else if (100 <= i && i < 108) {
                current_fg_color = pallete[i - 100 + 8];

              } else {
                has_unknown = true;
              }
          }
        }
        return !has_unknown;

      }
    } else if (op == 'n') {
      auto ints = parse_csi_colon_ints(seq, 0, seq.size() - 1);
      if (ints.size() == 1) {
        // ESC [6n
        if (ints[0] == 6) {
          // Reports the cursor position (CPR) to the application as
          // (as though typed at the keyboard) ESC[n;mR
          std::stringstream ss;
          ss << ESC << '[' << cursor_row << ';' << cursor_col << 'R';
          auto s = ss.str();
          write_to_tty(s);
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
        return true;
      }
    }
  }
  return false;
}

void hexdump(std::ostream &os, std::span<const uint8_t> data) {
  int width = 16;
  std::stringstream ss_hex, ss_s;
  int addr_width = ceil(log2(data.size()) / 4);
  os << "0x" << std::hex << std::setw(addr_width) << std::setfill('0') << 0 << " ";
  for (size_t i = 0; i < data.size(); i++) {
    ss_hex << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << ' ';
    if (std::isprint(data[i])) {
      ss_s << data[i];
    } else {
      ss_s << '.';
    }

    if (i % width == width - 1) {
      os << ss_hex.str() << " | " << ss_s.str() << std::endl;
      os << "0x" << std::hex << std::setw(addr_width) << std::setfill('0') << i << " ";
      ss_hex.str(""); ss_hex.clear();
      ss_s.str(""); ss_s.clear();
    }
  }

  auto rest = data.size() % width;
  if (rest > 0) {
    os << ss_hex.str();
    for (int i = 0; i < width - rest; i++) {
      os << "   ";
    }
    os << " | " << ss_s.str();
    for (int i = 0; i < width - rest; i++) {
      os << '.';
    }
    os << std::endl;
  }
}

void Screen::process_input() {
  int nread = read(tty_fd_, input_buffer_.data(), input_buffer_.size());
  for (int i = 0; i < nread; i++) {
    bool verbose = true;
    if (verbose) {
      auto c = input_buffer_[i];
      log_stream.put(c);
      log_stream.flush();
//      std::cout << "read() at (" << std::dec << cursor_row << "," << cursor_col << "): 0x" <<
//                std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)c;
//      if (std::isprint(c) && c != '\n') {
//        std::cout << " '" << c << '\'';
//      }
//      std::cout << std::endl;
    }

    auto input_type = tty_input_.receive_char(input_buffer_[i]);

    if (input_type == TTYInputType::Char) {
      auto c = tty_input_.last_char_;
      if (c == '\n') {
        new_line();
      } else if (c == 0x0f) {
        // switch to standard char set
      } else if (c == '\r') {
        // carriage return
        cursor_col = 0;
      } else if (c == '\a') {
//        std::cout << "alarm" << std::endl;
      } else if (c == '\b') {
//        std::cout << "back space" << std::endl;
        if (cursor_col == 0) {
          if (cursor_row == 0) {
            // nothing
          } else {
            cursor_col = max_cols_ - 1;
            cursor_row--;
          }
        } else {
          cursor_col--;
        }
      } else {
        if (cursor_row < max_rows_ && cursor_col < max_cols_) {
          lines_[cursor_row][cursor_col].c = c;
          lines_[cursor_row][cursor_col].bg_color = current_bg_color;
          lines_[cursor_row][cursor_col].fg_color = current_fg_color;
          next_cursor();
        } else {
          std::cerr << "Warning: cursor out of range: " << cursor_row << ":" << cursor_col << std::endl;
        }
      }
    } else if (input_type == TTYInputType::CSI) {
      auto ok = process_csi(tty_input_.buffer_);
      if (!ok) {
        std::cout << "unknown csi seq ESC [ ";
        for (auto c : tty_input_.buffer_) {
          std::cout << c;
        }
        std::cout << std::endl;
        hexdump(std::cout, tty_input_.buffer_);
      } else {
//        std::cout << "csi seq ESC [ ";
//        for (auto c : tty_input_.buffer_) {
//          std::cout << c;
//        }
//        std::cout << std::endl;
//        hexdump(std::cout, tty_input_.buffer_);
      }
    } else if (input_type == TTYInputType::TerminatedByST) {
      const auto &b = tty_input_.buffer_;
      if (!b.empty()) {
        if (b[0] == ']') {
          // OSC: Operating System Control
          if (b.size() >= 3 && b[1] == '0' && b[2] == ';') {
            // set title
            std::string title(reinterpret_cast<const char*>(b.data() + 3), b.size() - 3);
            SDL_SetWindowTitle(window_, title.c_str());
          }
        }
      }

    }

  }
}
bool Screen::check_child_process() {
  siginfo_t siginfo;
  siginfo.si_pid = 0;
  if (waitid(P_PID, child_pid_, &siginfo, WEXITED | WNOHANG) < 0) {
    perror("waitid");
    abort();
  }
  if (siginfo.si_pid != 0) {
    child_pid_ = 0;
    return false;
  } else {
    return true;
  }

}
void Screen::write_pending_input_data(std::vector<uint8_t> &input_buffer) {

  if (!input_buffer.empty()) {
    int total_write = 0;
    while (total_write < input_buffer.size()) {
      int nwrite = write(tty_fd_, input_buffer.data(), input_buffer.size());
      if (nwrite < 0) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
          perror("write");
          abort();
        }
      } else {
        total_write += nwrite;
      }
    }
    input_buffer.clear();
  }
}
void Screen::resize(int w, int h) {
  resolution_h_ = h;
  resolution_w_ = w;
  max_cols_ = resolution_w_ / glyph_width_;
  max_rows_ = resolution_h_ / glyph_height_;
  set_tty_window_size(tty_fd_, max_cols_, max_rows_, resolution_w_, resolution_h_);
  reset_tty_buffer();
}
void Screen::write_to_tty(const std::string &s) {
  int offset = 0;
  while (offset < s.size()) {
    int nread = write(tty_fd_, s.data() + offset, s.size() - offset);
    if (nread < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "Failed to write to tty_fd: " << strerror(errno) << std::endl;
        break;
      }
    } else {
      offset += nread;
    }
  }
}
void Screen::clear_selection() {
  selection_start_row = 0;
  selection_start_col = 0;
  selection_end_row = 0;
  selection_end_col = 0;
  has_selection = false;
}

}
