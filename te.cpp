#include <string>
#include <vector>

#include <te/display.hpp>

int main(int argc, char **argv, char **envp) {
  std::vector<std::string> environments;
  for (int i = 0; envp[i]; i++) {
    environments.emplace_back(envp[i]);
  }

  std::string font_file = "/usr/share/fonts/TTF/DejaVuSansMono.ttf";
  int font_size = 36;
  std::ofstream log_stream("a.log");
  te::Display display(log_stream, font_file, font_size, environments);
  display.loop();
}
