#define _POSIX_SOURCE
#include <string>
#include <tuple>
#include <iostream>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
//#include <io.h>

namespace te {


// return slave name;;
std::tuple<std::string, int> setup_tty() {
  int ptyfd = posix_openpt(O_RDWR);
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
  int slave_fd = open(slave_pty.c_str(), O_RDWR);
  if (slave_fd < 0) {
    perror("open slave fd");
    abort();
  }

  int pid = fork();
  if (pid < 0) {
    perror("fork");
    abort();
    return {-1, -1};
  } else if (pid == 0) {
    // child
    close(master_fd);

    // Save the default parameters of the slave side of the PTY
//    termios old_term_settings;
//    if (tcgetattr(slave_fd, &old_term_settings)) {
//      perror("tcgetattr");
//      abort();
//    }

    // Set raw mode on the slave side of the PTY
//    auto new_term_settings = old_term_settings;
//    cfmakeraw (&new_term_settings);
//    tcsetattr (slave_fd, TCSANOW, &new_term_settings);

    if (dup2(slave_fd, STDIN_FILENO) < 0) {
      perror("dup2 stdin");
      abort();
    }
    dup2(slave_fd, STDOUT_FILENO);
    dup2(slave_fd, STDERR_FILENO);
    // map stdin/stdout to tty fd
    if (!isatty(STDIN_FILENO)) {
      std::cout << "stdin is not a tty" << std::endl;
    }

    int ret = execve(command_line.c_str(), argv, envp);
    if (ret < 0) {
      perror("execve");
      abort();
    }
    // unreachable
    return {-1, -1};
  } else {
    close(slave_fd);
    fcntl(master_fd, F_SETFL, fcntl(master_fd, F_GETFL) | O_NONBLOCK);
    return {pid, master_fd};
  }
}

}
