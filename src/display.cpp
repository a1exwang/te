#include <te/display.hpp>

#include <cmath>
#include <csignal>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

#include <unistd.h>
#include <wait.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_clipboard.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <te/screen.hpp>
#include "../child.hpp"

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


Color Display::map_color(Color color) const {
  if (current_screen_->current_attrs.test(CHAR_ATTR_REVERSE_VIDEO)) {
    color.r = 0x100 - color.r;
    color.g = 0x100 - color.g;
    color.b = 0x100 - color.b;
  }

  return color;
}

bool Display::check_child_process() {
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
void Display::write_pending_input_data(std::vector<uint8_t> &input_buffer) {

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

void hexdump(std::ostream &os, std::span<const uint8_t> data) {
  int width = 16;
  std::stringstream ss_hex, ss_s;
  int addr_width = ceil(log2(data.size()) / 4);
  os << "0x" << std::hex << std::setw(addr_width) << std::setfill('0') << 0 << " ";
  for (size_t i = 0; i < data.size(); i++) {
    ss_hex << std::hex << std::setw(2) << std::setfill('0') << (int) data[i] << ' ';
    if (std::isprint(data[i])) {
      ss_s << data[i];
    } else {
      ss_s << '.';
    }

    if (i % width == width - 1) {
      os << ss_hex.str() << " | " << ss_s.str() << std::endl;
      os << "0x" << std::hex << std::setw(addr_width) << std::setfill('0') << i << " ";
      ss_hex.str("");
      ss_hex.clear();
      ss_s.str("");
      ss_s.clear();
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

void Display::process_input() {
  std::array<char, 1024> input_buffer; // NOLINT(cppcoreguidelines-pro-type-member-init)

  int nread = read(tty_fd_, input_buffer.data(), input_buffer.size());
  for (int i = 0; i < nread; i++) {
    bool verbose = true;
    if (verbose) {
      auto c = input_buffer[i];
      log_stream_.put(c);
      log_stream_.flush();
//      std::cout << "read() at (" << std::dec << cursor_row << "," << cursor_col << "): 0x" <<
//                std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)c;
//      if (std::isprint(c) && c != '\n') {
//        std::cout << " '" << c << '\'';
//      }
//      std::cout << std::endl;
    }

    auto input_type = tty_input_.receive_char(input_buffer[i]);

    bool verbose_read = false;

    if (input_type == TTYInputType::Char) {
      auto c = tty_input_.last_char_;
      if (verbose_read) {
        std::cout << "char at (" << std::dec << current_screen_->cursor_row << "," << current_screen_->cursor_col << "): 0x" <<
                  std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)c;
        if (std::isprint(c) && c != '\n') {
          std::cout << " '" << c << '\'';
        }
        std::cout << std::endl;
      }
      if (c == '\n') {
        current_screen_->new_line();
      } else if (c == 0x0f) {
        // switch to standard char set
      } else if (c == '\r') {
        // carriage return
        current_screen_->cursor_col = 0;
      } else if (c == '\a') {
//        std::cout << "alarm" << std::endl;
      } else if (c == '\b') {
//        std::cout << "back space" << std::endl;
        if (current_screen_->cursor_col == 0) {
          if (current_screen_->cursor_row == 0) {
            // nothing
          } else {
            current_screen_->cursor_col = max_cols_ - 1;
            current_screen_->cursor_row--;
          }
        } else {
          current_screen_->cursor_col--;
        }
      } else {
        if (current_screen_->current_attrs.test(CHAR_ATTR_AUTO_WRAP_MODE)) {
          // https://www.vt100.net/docs/vt510-rm/DECAWM.html
          // If the DECAWM function is set,
          // then graphic characters received when the cursor is at the right border of the page
          //  appear at the beginning of the next line.
          // Any text on the page scrolls up if the cursor is at the end of the scrolling region.
          if (current_screen_->cursor_col == max_cols_) {
            current_screen_->new_line();
            current_screen_->carriage_return();
          }
          current_screen_->fill_current_cursor(c);
          current_screen_->cursor_col++;
        } else {
          // If the DECAWM function is reset,
          // then graphic characters received when the cursor is at the right border of the page
          //  replace characters already on the page.
          current_screen_->fill_current_cursor(c);
          if (current_screen_->cursor_col < max_cols_ - 1) {
            current_screen_->cursor_col++;
          }
        }
      }
    } else if (input_type == TTYInputType::CSI) {
      auto ok = current_screen_->process_csi(tty_input_.buffer_);
      if (!ok) {
        std::cout << "unknown csi seq ESC [ ";
        for (auto c : tty_input_.buffer_) {
          std::cout << c;
        }
        std::cout << std::endl;
        hexdump(std::cout, tty_input_.buffer_);
      } else {
        if (verbose_read) {
          std::cout << "csi seq ESC [ ";
          for (auto c : tty_input_.buffer_) {
            std::cout << c;
          }
          std::cout << std::endl;
          hexdump(std::cout, tty_input_.buffer_);
        }
      }
    } else if (input_type == TTYInputType::TerminatedByST) {
      if (verbose_read) {
        std::cout << "TerminatedByST: " << std::endl;
        hexdump(std::cout, tty_input_.buffer_);
      }
      const auto &b = tty_input_.buffer_;
      if (!b.empty()) {
        if (b[0] == ']') {
          // OSC: Operating System Control
          if (b.size() >= 3 && b[1] == '0' && b[2] == ';') {
            // set title
            window_title_ = std::string(reinterpret_cast<const char *>(b.data() + 3), b.size() - 3);
            SDL_SetWindowTitle(window_, window_title_.c_str());
          }
        }
      }
    }
  }
}
void Display::render_background_image() {
  // default tiling mode
  int cols = ceil((double) resolution_w_ / background_image_width),
      rows = ceil((double) resolution_h_ / background_image_height);

  int pad_w = 0, pad_h = 0;
  // if we have enough space for one complete image, add padding
  if (resolution_w_ >= background_image_width && resolution_h_ >= background_image_height) {
    pad_h = resolution_h_ % background_image_height / 2;
    pad_w = resolution_w_ % background_image_width / 2;
  }
  for (int row = -1; row < rows; row++) {
    for (int col = -1; col < cols; col++) {
      SDL_Rect dst{
          background_image_width * col + pad_w, background_image_height * row + pad_h, background_image_width,
          background_image_height
      };
      SDL_Rect src{0, 0, background_image_width, background_image_height};
      SDL_RenderCopy(renderer_, background_image_texture, &src, &dst);
    }
  }
}

char shift_table[] = {
    // 0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '"', 0, 0, 0, 0, '<', '_', '>', '?',

    // digit
    ')', '!', '@', '#', '$', '%', '^', '&', '*', '(', 0, ':', 0, '+', 0, 0,

    // 0x40 upper case
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '{', '|', '}', 0, 0,

    // 0x60 lower case
    '~', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static std::unordered_set<char> SDLInputLiterals = {
    0x09/* \t */, 0x0d/* Enter */,
    0x1b /* ESC */,
    0x20/* SPACE */
};

void Display::resize(int w, int h) {
  resolution_h_ = h;
  resolution_w_ = w;
  max_cols_ = resolution_w_ / glyph_width_;
  max_rows_ = resolution_h_ / glyph_height_;
  set_tty_window_size(tty_fd_, max_cols_, max_rows_, resolution_w_, resolution_h_);

  default_screen_->resize(w, h);
  alternate_screen_->resize(w, h);
}
void Display::loop() {

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
                  if (c == 'c') {
                    auto s = clipboard_copy();
                    SDL_SetClipboardText(s.c_str());
                  } else if (c == 'v') {
                    auto s = SDL_GetClipboardText();
                    if (s) {
                      clipboard_paste(s);
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
    auto t_input1 = std::chrono::high_resolution_clock::now();

    if (child_pid_) {
      loop_continue = check_child_process();
      write_pending_input_data(input_buffer);
      process_input();
    }

    auto t_shell = std::chrono::high_resolution_clock::now();

    // draw console
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0xff);
    SDL_RenderClear(renderer_);

    if (background_image_texture) {
      render_background_image();
    }

    auto t_bg = std::chrono::high_resolution_clock::now();

    render_chars();

    auto t_chars = std::chrono::high_resolution_clock::now();

    // Update window
    SDL_RenderPresent(renderer_);
    auto now = std::chrono::high_resolution_clock::now();
    if (has_input) {
//      std::cerr << "Input latency "
//                << "input " << std::chrono::duration<float, std::milli>(t_input1 - t0).count() << "ms" << std::endl
//                << "shell " << std::chrono::duration<float, std::milli>(t_shell - t0).count() << "ms" << std::endl
//                << "bg " << std::chrono::duration<float, std::milli>(t_bg - t0).count() << "ms" << std::endl
//                << "chars " << std::chrono::duration<float, std::milli>(t_chars - t0).count() << "ms" << std::endl
//                << "total " << std::chrono::duration<float, std::milli>(now - t0).count() << "ms" << std::endl;
    }

    last_t = now;
  }
}
Display::Display(
    std::ostream &log_stream,
    const std::string &font_file_path,
    int font_size,
    const std::vector<std::string> &environment_variables) : log_stream_(log_stream) {

  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    std::cerr << "Error initializing SDL: " << SDL_GetError() << std::endl;
    abort();
  }
  resolution_w_ = 1920;
  resolution_h_ = 1080;

  window_ = SDL_CreateWindow(window_title_.c_str(), 0, 0, resolution_w_, resolution_h_, SDL_WINDOW_SHOWN);
  if (window_ == nullptr) {
    std::cerr << "Error creating window: " << SDL_GetError() << std::endl;
    abort();
  }

  SDL_SetWindowResizable(window_, SDL_TRUE);

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer_) {
    std::cerr << "Error creating renderer: " << SDL_GetError() << std::endl;
    abort();
  }

  // Start TTF
  if (TTF_Init() < 0) {
    std::cerr << "Error intializing SDL_ttf: " << TTF_GetError() << std::endl;
    abort();
  }
  font_ = TTF_OpenFont(font_file_path.c_str(), font_size);
  if (!font_) {
    std::cerr << "Error loading font: " << TTF_GetError() << std::endl;
    abort();
  }

  if (!TTF_FontFaceIsFixedWidth(font_)) {
    std::cerr << "font is not Fixed width" << std::endl;
    abort();
  }

  int minx, maxx, miny, maxy, advance;
  if (TTF_GlyphMetrics(font_, 'a', &minx, &maxx, &miny, &maxy, &advance) != 0) {
    std::cerr << "Error TTF_GlyphMetrics " << TTF_GetError() << std::endl;
    abort();
  }
  glyph_width_ = advance;
  glyph_height_ = TTF_FontLineSkip(font_);

  max_rows_ = resolution_h_ / glyph_height_;
  max_cols_ = resolution_w_ / glyph_width_;

  font_cache_ = std::make_unique<FontCache>(renderer_, font_);

  // Start Background Image
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  std::string background_image_path("/home/alexwang/bg.jpg");
  if (std::filesystem::exists(background_image_path)) {
    background_image_texture = IMG_LoadTexture(renderer_, background_image_path.c_str());
    if (background_image_texture) {
      int access = 0;
      Uint32 format;
      if (SDL_QueryTexture(background_image_texture,
                           &format,
                           &access,
                           &background_image_width,
                           &background_image_height) == 0) {
        // ok

      } else {
        std::cerr << "Failed to query image texture" << SDL_GetError() << std::endl;
      }
    }
  } else {
    std::cerr << "Warning background image not found '" << background_image_path << "'" << std::endl;
  }

  // Make sure our image stays in the background using alpha blending
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

  std::string program = "/bin/bash";
  char *argv[] = {(char *) program.c_str(), nullptr};
  std::vector<char *> envps;
  std::vector<std::string> envs;
  for (const auto &env : environment_variables) {
    if (env.starts_with("TERM=")) {
      envs.emplace_back("TERM=rxvt");
    } else {
      envs.emplace_back(env);
    }
  }
  for (auto &env : envs) {
    envps.push_back((char *) env.c_str());
  }
  envps.push_back(nullptr);
  std::tie(child_pid_, tty_fd_) = start_child(program, argv, envps.data());

  default_screen_ = std::make_unique<Screen>(this);
  alternate_screen_ = std::make_unique<Screen>(this);
  current_screen_ = default_screen_.get();

  set_tty_window_size(tty_fd_, max_cols_, max_rows_, resolution_w_, resolution_h_);
//  reset_tty_buffer();
//
//  normal_mode();
}
void Display::write_to_tty(std::string_view s) {
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
Display::~Display() {
  if (font_) {
    TTF_CloseFont(font_);
  }
  if (window_) {
    SDL_DestroyWindow(window_);
  }

  IMG_Quit();
  TTF_Quit();
  SDL_Quit();
}
void Display::clipboard_paste(std::string_view clipboard_text) {
//  auto clipboard_text = SDL_GetClipboardText();
  if (current_screen_->current_attrs.test(CHAR_ATTR_XTERM_BLOCK_PASTE)) {
    write_to_tty("\1b[200~");
  }
  // UTF8
  write_to_tty(clipboard_text);
  if (current_screen_->current_attrs.test(CHAR_ATTR_XTERM_BLOCK_PASTE)) {
    write_to_tty("\1b[201~");
  }

}
std::string Display::clipboard_copy() {
  if (has_selection) {
    std::stringstream ss;
    for (int i = selection_start_row; i <= selection_end_row; i++) {
      for (int j = selection_start_col; j <= selection_end_col; j++) {
        ss << current_screen_->get_row(i)[j].c;
      }
      ss << std::endl;
    }
    clear_selection();
    return ss.str();
  } else {
    return "";
  }
}
void Display::clear_selection() {
  selection_start_row = 0;
  selection_start_col = 0;
  selection_end_row = 0;
  selection_end_col = 0;
  has_selection = false;
}

}