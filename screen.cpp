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

Screen::Screen(ScreenConfig config, char **envp) :config_(std::move(config)), buffer_(1) {

  if (SDL_Init( SDL_INIT_EVERYTHING ) != 0) {
    std::cerr << "Error initializing SDL: " << SDL_GetError() << std::endl;
    abort();
  }

  window_ = SDL_CreateWindow( "a1ex's te", 100, 100, 1920, 1080, SDL_WINDOW_SHOWN );
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

  std::string program = "/bin/sh";
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
              std::cout << "enter" << std::endl;
            } else if (c < 0x100 /*NOTE: if c >=0x100, isprint is UB*/ && std::isprint(c)) {
              input_buffer.push_back(c);
              std::cout << "key '" << c << std::endl;
            } else {
              cerr << "unknown key " << c << std::endl;
            }
          }
          break;
        }
      }
    }

    if (child_pid_) {
      // Check if child exited
      siginfo_t siginfo;
      siginfo.si_pid = 0;
      if (waitid(P_PID, child_pid_, &siginfo, WEXITED | WNOHANG) < 0) {
        perror("waitid");
        abort();
      }
      if (siginfo.si_pid != 0) {
        child_pid_ = 0;
        loop_continue = false;
      }

      // write pending input data
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

      char data[100];
      int nread = read(tty_fd_, data, 80);
      for (int i = 0; i < nread; i++) {
        receive_char(data[i]);
      }

    }

    SDL_SetRenderDrawColor(
        renderer_,
        config_.background_color.r,
        config_.background_color.g,
        config_.background_color.b,
        config_.background_color.a
    );
    SDL_RenderClear(renderer_);
    SDL_Color foreground = { config_.foreground_color.r, config_.foreground_color.g, config_.foreground_color.b };

    int start_line = 0;
    if (buffer_.size() > max_lines_) {
      start_line = buffer_.size() - max_lines_;
    }

    for (int i = start_line; i < buffer_.size(); i++) {
      int rows_to_top = i - start_line;
      auto &line = buffer_[i];
      if (!line.empty()) {
        SDL_Surface* text_surf = TTF_RenderText_Blended(font_, line.c_str(), foreground);
        auto text = SDL_CreateTextureFromSurface(renderer_, text_surf);

        SDL_Rect dest;
        dest.x = 0;
        dest.y = rows_to_top * config_.font_size;
        dest.w = text_surf->w;
        dest.h = text_surf->h;
        SDL_RenderCopy(renderer_, text, NULL, &dest);

        SDL_DestroyTexture(text);
        SDL_FreeSurface(text_surf);
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

  ioctl(tty_fd_, TIOCSWINSZ, &screen_size);
}

bool is_final_byte(uint8_t c) {
  return c >= 0x40 && c <= 0x7E;
}

void Screen::receive_char(uint8_t c) {
  if (input_state == InputState::Escape) {
    if (c == '[') {
      input_state = InputState::CSI;
    } else if (c == 0x1b) {
      input_state = InputState::Escape;
    } else {
      input_state = InputState::Idle;
    }
  } else if (input_state == InputState::Idle) {
    if (c == 0x1b) {
      input_state = InputState::Escape;
    } else {
      if (c == '\n') {
        buffer_.emplace_back();
      } else if (c == '\r') {
      } else {
        buffer_.back().push_back(c);
      }
      std::cout << "receive char " << std::hex << (int)c;
      if (std::isprint(c)) {
        std::cout << " (" << c <<")";
      }
      std::cout << std::endl;
    }
  } else {
    // CSI:
    csi_buffer_.push_back(c);

    process_csi();

    if (is_final_byte(c)) {
      csi_buffer_.clear();
    }
  }

}
void Screen::process_csi() {

}

}
