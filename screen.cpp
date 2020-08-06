#include "screen.hpp"

#include <cassert>

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

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

Screen::Screen(ScreenConfig config, char **envp) : config_(std::move(config)) {

  if (SDL_Init( SDL_INIT_EVERYTHING ) != 0) {
    std::cerr << "Error initializing SDL: " << SDL_GetError() << std::endl;
    abort();
  }

  window_ = SDL_CreateWindow( "a1ex's te", 0, 0, config_.resolution_w, config_.resolution_h, SDL_WINDOW_SHOWN );
  if (window_ == nullptr) {
    cerr << "Error creating window: " << SDL_GetError()  << endl;
    abort();
  }
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

  max_lines_ = config_.resolution_h / glyph_height_;
  max_cols_ = config_.resolution_w / glyph_width_;

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
  std::tie(child_pid_, tty_fd_) = start_child(program, argv, envps.data());

  set_window_size(max_cols_, max_lines_);

  resize_lines();
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

void Screen::loop() {

  SDL_Event event;
  std::chrono::high_resolution_clock::time_point last_t;
  std::vector<uint8_t> input_buffer;
  bool loop_continue = true;

  while (loop_continue) {
    bool has_input = false;
    auto t0 = std::chrono::high_resolution_clock::now();
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          loop_continue = false;
          break;
        case SDL_KEYDOWN:
          if (event.key.type == SDL_KEYDOWN) {
            has_input = true;
            auto c = event.key.keysym.sym;
            if (event.key.keysym.sym == SDL_KeyCode::SDLK_BACKSPACE) {
              // delete key
              input_buffer.push_back('\x7f');
            } else if (c == SDL_KeyCode::SDLK_RETURN) {
              input_buffer.push_back('\n');
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
              } else {
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

    SDL_SetRenderDrawColor(renderer_, 0,0,0,0xff);
    SDL_RenderClear(renderer_);

    for (int i = 0; i < lines_.size(); i++) {
      auto &line = lines_[i];
      for (int j = 0; j < line.size(); j++) {
        auto &c = line[j];

        SDL_SetRenderDrawColor(renderer_, c.bg_color.r, c.bg_color.g, c.bg_color.b, c.bg_color.a);
        SDL_Rect glyph_box{glyph_width_*j, glyph_height_*i, glyph_width_, glyph_height_};
        SDL_RenderFillRect(renderer_, &glyph_box);

        auto character = c.c;
        if (!std::isprint(c.c) && character != 0) {
          character = '?';
        }

        if (character != ' ' && character != 0) {
//          std::cout << "draw " << c.c << " at " << i << " " << j << std::endl;
          SDL_Surface* text_surf = TTF_RenderGlyph_Blended(font_, character, to_sdl_color(c.fg_color));
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
void Screen::set_window_size(int w, int h) {
  winsize screen_size;
  screen_size.ws_col = w;
  screen_size.ws_row = h;
  screen_size.ws_xpixel = config_.resolution_w;
  screen_size.ws_ypixel = config_.resolution_h;

  if (ioctl(tty_fd_, TIOCSWINSZ, &screen_size) < 0) {
    perror("ioctl TIOCSWINSZ");
    abort();
  }
}

bool is_final_byte(uint8_t c) {
  return c >= 0x40 && c <= 0x7E;
}

TTYInputType TTYInput::receive_char(uint8_t c) {
  last_char_ = '?';
  if (input_state == InputState::Escape) {
    if (c == '[') {
      // CSI
      buffer_.clear();
      input_state = InputState::CSI;
      return TTYInputType::Intermediate;
    } else if (c == ']') {
      // Operating System Control
      buffer_.clear();
      input_state = InputState::OSC;
      return TTYInputType::Intermediate;
    } else if (c == 0x1b) {
      input_state = InputState::Escape;
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

    if (is_final_byte(c)) {
      input_state = InputState::Idle;
      return TTYInputType::CSI;
    } else {
      return TTYInputType::Intermediate;
    }
  } else if (input_state == InputState::OSC) {
    if (c == '\a') {
      input_state = InputState::Idle;
      return TTYInputType::OSC;
    } else {
      buffer_.push_back(c);
      return TTYInputType::Intermediate;
    }
  } else {
    assert(0);
  }

}

// parse format 123;444;;22
std::vector<int> parse_csi_ints(const std::vector<uint8_t> &seq, int start, int end) {
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
    ColorWhite
};

void Screen::process_csi(const std::vector<uint8_t> &seq) {

  std::cout << "csi seq ESC [ ";
  for (auto c : seq) {
    std::cout << c;
  }
  std::cout << std::endl;

  if (!seq.empty()) {
    auto op = seq.back();
    if (op == 'm') {
      auto ints = parse_csi_ints(seq, 0, seq.size() - 1);
      assert(!ints.empty());
      for (auto i : ints) {
        if (i == 0) {
          std::cout << "CSI reset attributes" << std::endl;
          current_fg_color = get_default_fg_color();
          current_bg_color = get_default_bg_color();
        } else if (30 <= i && i < 38) {
          current_fg_color = pallete[i - 30];
          std::cout << "CSI set fg color" << std::endl;
        } else if (40 <= i && i < 48) {
          current_bg_color = pallete[i - 30];
          std::cout << "CSI set bg color" << std::endl;
        }
      }
    }

  }

}
void Screen::process_input() {
  int nread = read(tty_fd_, input_buffer_.data(), input_buffer_.size());
  for (int i = 0; i < nread; i++) {
    {
      auto c = input_buffer_[i];
      std::cout << "read() at (" << std::dec << cursor_row << "," << cursor_col << "): 0x" <<
                std::setw(2) << std::setfill('0') << std::hex << (int)c;
      if (std::isprint(c) && c != '\n') {
        std::cout << " '" << c << '\'';
      }
      std::cout << std::endl;
    }
    auto input_type = tty_input_.receive_char(input_buffer_[i]);

    if (input_type == TTYInputType::Char) {
      auto c = tty_input_.last_char_;
      if (c == '\n') {
        new_line();
      } else if (c == '\r') {
      } else if (c == '\a') {
        std::cout << "alarm" << std::endl;
      } else if (c == '\b') {
        std::cout << "back space" << std::endl;
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
        lines_[cursor_row][cursor_col] = Char();
      } else {
        lines_[cursor_row][cursor_col].c = c;
        lines_[cursor_row][cursor_col].bg_color = current_bg_color;
        lines_[cursor_row][cursor_col].fg_color = current_fg_color;
        next_cursor();
      }
    } else if (input_type == TTYInputType::CSI) {
      process_csi(tty_input_.buffer_);
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

}
