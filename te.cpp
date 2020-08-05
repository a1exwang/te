#include "screen.hpp"

int main(int argc, char **argv, char **envp) {
  te::ScreenConfig config;
  te::Screen screen(config, envp);
  screen.loop();

}
