#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <climits>
#include <sys/inotify.h>
#include <unistd.h>

#include <array>
#include <iostream>

// Implement a simple tail -f, but without line buffering,
//  for debugging our terminal emulator
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Invalid arguments" << std::endl;
    abort();
  }
  const char *path = argv[1];
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    std::cerr << "Failed to open file '" << path << "'" << std::endl;
    abort();
  }

  // set non blocking
  int opt = fcntl(fd, F_GETFL);
  if (opt < 0) {
    perror("fcntl(F_GETFL)");
    abort();
  }
  opt |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, opt) < 0) {
    perror("fcntl(F_SETFL)");
    abort();
  }

  // seek to end
  lseek(fd, 0, SEEK_END);

  int inotify_fd = inotify_init();
  if (inotify_fd < 0) {
    perror("inotify_init");
    abort();
  }

  if (inotify_add_watch(inotify_fd, path, IN_MODIFY) < 0) {
    perror("inotify_add_watch");
    abort();
  }

  char buf[sizeof(inotify_event) + NAME_MAX + 1];
  std::array<char, 1024> file_buf{};
  while (true) {
    int nread = read(inotify_fd, buf, sizeof(buf));
    if (nread < 0) {
      perror("pread inotify_fd");
      abort();
    }

    nread = read(fd, file_buf.data(), file_buf.size());
    if (nread < 0) {
      perror("read(fd)");
      abort();
    }
    int nwrite = write(STDOUT_FILENO, file_buf.data(), nread);
    assert(nread == nwrite);
  }
}