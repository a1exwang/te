#define _POSIX_SOURCE
#include <te/subprocess.hpp>
#include <string>
#include <tuple>
#include <iostream>
#include <vector>
#include <string_view>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <wait.h>

namespace te {


// return slave name;;
std::tuple<std::string, int> setup_tty() {
  int ptyfd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (ptyfd < 0) {
    perror("posix_openpt");
    abort();
  }

  if (grantpt(ptyfd) < 0) {
    perror("grantpt");
    abort();
  }

  if (unlockpt(ptyfd) < 0) {
    perror("unlockpt");
    abort();
  }
  auto slave_tty = ptsname(ptyfd);
  if (!slave_tty) {
    perror("ptsname");
    abort();
  }
  return {slave_tty, ptyfd};
}

Subprocess::Subprocess(std::string command_line,
                       std::vector<std::string> args,
                       std::vector<std::string> envs)
    :command_line_(std::move(command_line)), args_(std::move(args)), envs_(std::move(envs)) {

  std::vector<char*> argv;
  for (const auto &arg : args_) {
    argv.emplace_back(const_cast<char*>(arg.c_str()));
  }
  argv.emplace_back(nullptr);

  std::vector<char*> envp;
  for (const auto &env : envs_) {
    envp.emplace_back(const_cast<char*>(env.c_str()));
  }
  envp.emplace_back(nullptr);


  auto [slave_pty, master_pty] = setup_tty();

  int pid = fork();
  if (pid < 0) {
    perror("fork");
    abort();
  } else if (pid == 0) {
    // child

    // create a new process group and assume leader
    if (setsid() < 0) {
      perror("setsid");
      abort();
    }

    int slave_fd = ioctl(master_pty, TIOCGPTPEER, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (slave_fd < 0) {
      perror("ioctl TIOCGPTPEER");
      abort();
    }

    if (ioctl(slave_fd, TIOCSCTTY, slave_fd) < 0) {
      perror("ioctl TIOCSCTTY");
      abort();
    }

    if (dup2(slave_fd, STDIN_FILENO) < 0) {
      perror("dup2 stdin");
      abort();
    }
    if (dup2(slave_fd, STDOUT_FILENO) < 0) {
      perror("dup2 stdout");
      abort();
    }
    if (dup2(slave_fd, STDERR_FILENO) < 0) {
      perror("dup2 stderr");
      abort();
    }
    // map stdin/stdout to tty fd

    int ret = execve(command_line.c_str(), argv.data(), envp.data());
    if (ret < 0) {
      perror("execve");
      abort();
    }
  } else {
    child_pid_ = pid;
    tty_fd_ = master_pty;
  }
}

bool Subprocess::check_exited() const {
  siginfo_t siginfo;
  siginfo.si_pid = 0;
  if (waitid(P_PID, child_pid_, &siginfo, WEXITED | WNOHANG) < 0) {
    perror("waitid");
    abort();
  }
  // when si_pid == 0, the child process has not exited
  if (siginfo.si_pid == 0) {
    return false;
  } else {
    return true;
  }
}

}
