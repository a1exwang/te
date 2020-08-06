#include "screen.hpp"

#include <vector>
#include <string>
#include <iostream>
#include <chrono>

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

void Screen::loop() {

  SDL_Event event;
  std::chrono::high_resolution_clock::time_point last_t;
  std::vector<uint8_t> input_buffer;
  bool loop_continue = true;
  while (loop_continue) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          loop_continue = false;
          break;
        case SDL_KEYDOWN:
          if (event.key.type == SDL_KEYDOWN) {
            auto c = event.key.keysym.sym;
            if (c == '\r') {
              input_buffer.push_back('\n');
//              std::cout << "enter" << std::endl;
            } else if (c < 0x100 /*NOTE: if c >=0x100, isprint is UB*/ && std::isprint(c)) {
              input_buffer.push_back(c);
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

        if (std::isprint(c.c) && c.c != ' ') {
//          std::cout << "draw " << c.c << " at " << i << " " << j << std::endl;
          SDL_Surface* text_surf = TTF_RenderGlyph_Blended(font_, c.c, to_sdl_color(c.fg_color));
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
//    std::cout << "latency " << std::chrono::duration<float, std::milli>(now - last_t).count() << "ms" << std::endl;
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
  screen_size.ws_xpixel = 1920;
  screen_size.ws_ypixel = 1080;

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
      csi_buffer_.clear();
      input_state = InputState::CSI;
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
//      std::cout << "receive char " << std::hex << (int)c;
//      if (std::isprint(c)) {
//        std::cout << " (" << c <<")";
//      }
//      std::cout << std::endl;

      if (c == '\n' || std::isprint(c)) {
        last_char_ = c;
        return TTYInputType::Char;
      } else {
        return TTYInputType::Intermediate;
      }
    }
  } else {
    // CSI:
    csi_buffer_.push_back(c);

    if (is_final_byte(c)) {
      input_state = InputState::Idle;
      return TTYInputType::CSI;
    } else {
      return TTYInputType::Intermediate;
    }
  }

}
void Screen::process_csi(const std::vector<uint8_t> &seq) {

  std::cout << "csi seq ESC [ ";
  for (auto c : seq) {
    std::cout << c;
  }
  std::cout << std::endl;

}
void Screen::process_input() {
  int nread = read(tty_fd_, input_buffer_.data(), input_buffer_.size());
  for (int i = 0; i < nread; i++) {
    auto input_type = tty_input_.receive_char(input_buffer_[i]);
    if (input_type == TTYInputType::Char) {
      auto c = tty_input_.last_char_;
      if (c == '\n') {
        new_line();
      } else {

//        std::cout << "get char " << c << " at " << cursor_row << " " << cursor_col << std::endl;
        lines_[cursor_row][cursor_col].c = c;
        lines_[cursor_row][cursor_col].bg_color = current_bg_color;
        lines_[cursor_row][cursor_col].fg_color = current_fg_color;
        next_cursor();
      }
    } else if (input_type == TTYInputType::CSI) {
      process_csi(tty_input_.csi_buffer_);
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
