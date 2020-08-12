#include <te/display.hpp>

#include <cassert>

#include <tuple>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_ttf.h>

#include <te/screen.hpp>

namespace std {
  template <>
  struct hash<te::Color> {
    size_t operator()(const te::Color &color) {
      return color.u32;
    }
  };

  template <>
  struct equal_to<te::Color> {
    size_t operator()(const te::Color &lhs, const te::Color &rhs) {
      return lhs.u32 == rhs.u32;
    }
  };
}

namespace te {

FontCache::FontCache(SDL_Renderer *renderer, TTF_Font *font) :renderer_(renderer), font_(font) {

  std::vector<uint32_t> styles = {
      TTF_STYLE_NORMAL,

      TTF_STYLE_UNDERLINE,
      TTF_STYLE_BOLD,
      TTF_STYLE_ITALIC,

      TTF_STYLE_UNDERLINE | TTF_STYLE_BOLD,
      TTF_STYLE_UNDERLINE | TTF_STYLE_ITALIC,
      TTF_STYLE_BOLD | TTF_STYLE_ITALIC,

      TTF_STYLE_UNDERLINE | TTF_STYLE_BOLD | TTF_STYLE_ITALIC,
  };

  for (auto style : styles) {
    auto [it, ok] = fc.insert({style, {128, nullptr}});
    assert(ok);

    TTF_SetFontStyle(font_, style);
    for (int i = 0; i < 128; i++) {
      if (std::isprint(i)) {
        SDL_Color white_color{0xff,0xff,0xff,0xff};
        SDL_Surface *text_surf = TTF_RenderGlyph_Blended(font_, i, white_color);
        if (!text_surf) {
          std::cerr << "Failed to TTF_RenderGlyph_Blended" << SDL_GetError() << std::endl;
          abort();
        }

        auto text = SDL_CreateTextureFromSurface(renderer_, text_surf);
        if (!text) {
          std::cerr << "Failed to SDL_CreateTextureFromSurface when creating fonts:" << SDL_GetError() << std::endl;
          abort();
        }

        it->second[i] = text;
      }
    }
  }
}


static SDL_Color to_sdl_color(Color color) {
  return SDL_Color{color.r, color.g, color.b, color.a};
}

void Display::render_chars() {
  for (int row = 0; row < max_rows_; row++) {
    auto &row_data = current_screen_->get_row(row);
    for (int col = 0; col < max_cols_; col++) {
      auto &c = row_data[col];
      SDL_Rect glyph_box{glyph_width_ * col, glyph_height_ * row, glyph_width_, glyph_height_};
      Color fg = c.fg_color, bg = c.bg_color;

      // draw cursor background
      if (row == current_screen_->cursor_row && col == current_screen_->cursor_col && current_screen_->cursor_show) {
        if (current_screen_->cursor_blink) {
          if (current_screen_->cursor_flip) {
            bg = current_screen_->cursor_color;
            fg = current_screen_->cursor_fg_color;
          }
          auto now = std::chrono::high_resolution_clock::now();
          if (now - current_screen_->cursor_last_time > current_screen_->blink_interval) {
            current_screen_->cursor_flip = !current_screen_->cursor_flip;
            current_screen_->cursor_last_time = now;
          }
        } else {
          bg = current_screen_->cursor_color;
          fg = current_screen_->cursor_fg_color;
        }
      } else if (has_selection) {
        auto start = std::make_tuple(selection_start_row, selection_start_col),
            end = std::make_tuple(selection_end_row, selection_end_col);
        if (in_range(start, end, std::make_tuple(row, col))) {
          bg = selection_bg_color;
          fg = selection_fg_color;
        }
      }

      fg = map_color(fg);
      bg = map_color(bg);

      // draw bg color
      SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, background_image_opaque);
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
        auto t0 = std::chrono::high_resolution_clock::now();
        SDL_Color white_color{0xff,0xff,0xff,0xff};
//        SDL_Surface *text_surf = TTF_RenderGlyph_Blended(font_, character, white_color);
//        if (!text_surf) {
//          std::cerr << "Failed to TTF_RenderGlyph_Blended" << SDL_GetError() << std::endl;
//          abort();
//        }
//        auto text = SDL_CreateTextureFromSurface(renderer_, text_surf);
        auto text = font_cache_->at(std::make_tuple(style, character));


        auto t1 = std::chrono::high_resolution_clock::now();

        SDL_SetTextureColorMod(text, fg.r, fg.g, fg.b);
        SDL_RenderCopy(renderer_, text, NULL, &glyph_box);
        auto t2 = std::chrono::high_resolution_clock::now();

//        SDL_DestroyTexture(text);
//        SDL_FreeSurface(text_surf);
//        auto t3 = std::chrono::high_resolution_clock::now();
//        std::cout << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms "
//                  << std::chrono::duration<double, std::milli>(t2 - t0).count() << "ms "
//                  << std::chrono::duration<double, std::milli>(t3 - t0).count() << "ms "
//                  << std::endl;
      }
    }
  }
}
}