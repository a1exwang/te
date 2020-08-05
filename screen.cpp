#include "screen.hpp"

#include <vector>
#include <string>
#include <iostream>

#include <SDL2/SDL.h> // For Events
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_ttf.h>

using std::cerr;
using std::endl;



namespace te {

Screen::Screen(ScreenConfig config) :config_(std::move(config)) {

  if (SDL_Init( SDL_INIT_EVERYTHING ) != 0) {
    std::cerr << "Error initializing SDL: " << SDL_GetError() << std::endl;
    abort();
  }

  window_ = SDL_CreateWindow( "a1ex's te", 100, 100, 1920, 1080, SDL_WINDOW_SHOWN );
  if (window_ == nullptr) {
    cerr << "Error creating window: " << SDL_GetError()  << endl;
    abort();
  }
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
}

void Screen::loop() {
  SDL_Event event;
  bool loop_continue = true;
  while (loop_continue) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          loop_continue = false;
          break;
        }
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

    {
//      std::unique_lock<std::mutex> _(lock_);
      for (int i = 0; i < buffer_.size(); i++) {
        auto &line = buffer_[i];
        SDL_Surface* text_surf = TTF_RenderText_Blended(font_, line.c_str(), foreground);
        auto text = SDL_CreateTextureFromSurface(renderer_, text_surf);

        SDL_Rect dest;
        dest.x = 0;
        dest.y = i * config_.font_size;
        dest.w = text_surf->w;
        dest.h = text_surf->h;
        SDL_RenderCopy(renderer_, text, NULL, &dest);

        SDL_DestroyTexture(text);
        SDL_FreeSurface(text_surf);
      }
    }


    // Update window
    SDL_RenderPresent(renderer_);
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

}

