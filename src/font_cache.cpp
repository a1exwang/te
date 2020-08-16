#include <te/font_cache.hpp>

#include <cassert>

#include <tuple>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_ttf.h>

#include <te/screen.hpp>
#include <te/display.hpp>

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

std::vector<std::string> extra_chars = {
    "␣", "─",
};

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

  SDL_Color white_color{0xff,0xff,0xff,0xff};
  for (auto style : styles) {
    auto [it, ok] = fc.insert({style, {}});
    assert(ok);

    TTF_SetFontStyle(font_, style);
    for (int i = 0; i < 128; i++) {
      if (std::isprint(i)) {
        SDL_Surface *text_surf = TTF_RenderGlyph_Blended(font_, i, white_color);
        if (!text_surf) {
          std::cerr << "Failed to TTF_RenderGlyph_Blended" << SDL_GetError() << std::endl;
          abort();
        }

        auto texture = SDL_CreateTextureFromSurface(renderer_, text_surf);
        if (!texture) {
          std::cerr << "Failed to SDL_CreateTextureFromSurface when creating fonts:" << SDL_GetError() << std::endl;
          abort();
        }

        it->second[std::string(1, i)] = texture;
      }
    }
    for (const auto &extra_char : extra_chars) {
      SDL_Surface *text_surf = TTF_RenderUTF8_Blended(font_, extra_char.c_str(), white_color);
      if (!text_surf) {
        std::cerr << "Failed to TTF_RenderGlyph_Blended" << SDL_GetError() << std::endl;
        abort();
      }

      auto text = SDL_CreateTextureFromSurface(renderer_, text_surf);
      if (!text) {
        std::cerr << "Failed to SDL_CreateTextureFromSurface when creating fonts:" << SDL_GetError() << std::endl;
        abort();
      }

      it->second[extra_char] = text;
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

      // get to character's cached texture
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

      if (c.c.empty() || (c.c.size() == 1 && c.c[0] == 0 || c.c[0] == ' ')) {
        continue;
      }

      auto texture = font_cache_->at(std::make_tuple(style, c.c));
      if (!texture) {
        texture = font_cache_->at(std::make_tuple(style, "?"));
        assert(texture);
      }

      SDL_SetTextureColorMod(texture, fg.r, fg.g, fg.b);
      SDL_RenderCopy(renderer_, texture, NULL, &glyph_box);
    }
  }
}
}