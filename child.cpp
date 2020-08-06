#define _POSIX_SOURCE
#include <string>
#include <tuple>
#include <iostream>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
//#include <io.h>

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

std::tuple<int, int> start_child(std::string command_line, char **argv, char **envp) {
  auto [slave_pty, master_fd] = setup_tty();

  int pid = fork();
  if (pid < 0) {
    perror("fork");
    abort();
    return {-1, -1};
  } else if (pid == 0) {
    // child

    // create a new process group and assume leader
    if (setsid() < 0) {
      perror("setsid");
      abort();
    }

    int slave_fd = ioctl(master_fd, TIOCGPTPEER, O_RDWR | O_NOCTTY | O_CLOEXEC);
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

    int ret = execve(command_line.c_str(), argv, envp);
    if (ret < 0) {
      perror("execve");
      abort();
    }
    // unreachable
    return {-1, -1};
  } else {
    return {pid, master_fd};
  }
}

}
