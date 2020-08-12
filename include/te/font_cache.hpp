#pragma once

#include <tuple>
#include <unordered_map>
#include <cinttypes>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Texture;
struct SDL_Renderer;
struct _TTF_Font;
typedef _TTF_Font TTF_Font;

namespace te {

class FontCache {
 public:
  FontCache(SDL_Renderer *renderer, TTF_Font *font);
  SDL_Texture *at(std::tuple<uint32_t, std::string> pos) const {
    auto &item = fc.at(std::get<0>(pos));
    auto it = item.find(std::get<1>(pos));
    if (it == item.end()) {
      return nullptr;
    } else {
      return it->second;
    }
  }
 private:
  // style -> utf8 char -> texture
  std::unordered_map<uint32_t, std::unordered_map<std::string, SDL_Texture*>> fc;
  SDL_Renderer *renderer_;
  TTF_Font *font_;
};

}