#include "screen.hpp"

int main() {
  te::ScreenConfig config;
  te::Screen screen(config);
  screen.loop();

}
