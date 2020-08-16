#include <string>
#include <vector>

#include <te/display.hpp>

int main(int argc, char **argv, char **envp) {
  std::vector<std::string> environments;
  for (int i = 0; envp[i]; i++) {
    environments.emplace_back(envp[i]);
  }

  std::string font_file = "/usr/share/fonts/TTF/DejaVuSansMono.ttf";
  int font_size = 34;
  std::string log_file_path = "a.log";
  std::ofstream log_stream(log_file_path, std::ios::trunc);
  if (!log_stream) {
    std::cerr << "Failed to open log file '" << log_file_path << "'" << std::endl;
    abort();
  }
  bool use_acceleration = true;
  te::Display display(
      log_stream,
      {"/bin/bash"},
      "rxvt",
      font_file,
      font_size,
      "/home/alexwang/bg.png",
      environments,
      use_acceleration);
  display.loop();
}
