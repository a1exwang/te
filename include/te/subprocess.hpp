#pragma once

#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace te {
class Subprocess {
 public:
  Subprocess(
      std::string command_line,
      std::vector<std::string> argv,
      std::vector<std::string> envs
  );

  bool check_exited() const;

  int tty_fd() const {
    return tty_fd_;
  }

 private:
  int tty_fd_ = -1;
  int child_pid_ = - 1;
  std::string command_line_;
  std::vector<std::string> args_;
  std::vector<std::string> envs_;

};
}
