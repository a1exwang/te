#pragma once

#include <cinttypes>
#include <bitset>
#include <vector>

namespace te {
constexpr uint8_t ESC = 0x1bu;
static const char *CSI = "\x1b[";

#pragma pack(push, 1)
union Color {
  uint32_t u32;
  struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
  };
};
#pragma pack(pop)
constexpr Color ColorWhite = Color{0xffcccccc};
constexpr Color ColorBlack = Color{0xff000000};
constexpr Color ColorRed = Color{0xffcc0000};
constexpr Color ColorGreen = Color{0xff00cc00};
constexpr Color ColorBlue = Color{0xff0000cc};
constexpr Color ColorYellow = Color{0xff888800};
constexpr Color ColorCyan = Color{0xff008888};
constexpr Color ColorMagenta = Color{0xff880088};

constexpr Color ColorBrightWhite = Color{0xffeeeeee};
constexpr Color ColorBrightBlack = Color{0xff444444};
constexpr Color ColorBrightRed = Color{0xffee0000};
constexpr Color ColorBrightGreen = Color{0xff00ee00};
constexpr Color ColorBrightBlue = Color{0xff0000ee};
constexpr Color ColorBrightYellow = Color{0xffaaaa00};
constexpr Color ColorBrightCyan = Color{0xff00aaaa};
constexpr Color ColorBrightMagenta = Color{0xffaa00aa};


enum class TTYInputType {
  Char,
  CSI,
  Unknown,
  Intermediate,
  TerminatedByST,
};

enum {
  CHAR_ATTR_BOLD = 0,
  CHAR_ATTR_FAINT,
  CHAR_ATTR_ITALIC,
  CHAR_ATTR_UNDERLINE,
  CHAR_ATTR_INVERT,
  CHAR_ATTR_CROSSED_OUT,


  // CSI ?0h
  CHAR_ATTR_CURSOR_APPLICATION_MODE,
  // CSI ?5h
  CHAR_ATTR_REVERSE_VIDEO,
  // CSI ?7h
  CHAR_ATTR_AUTO_WRAP_MODE,

  // CSI ?1049h
  CHAR_ATTR_XTERM_WINDOW_FOCUS_TRACKING,
  // CSI ?2004h
  CHAR_ATTR_XTERM_BLOCK_PASTE,

  CHAR_ATTR_COUNT
};

struct Char {
  void reset() {
    *this = Char();
  }
  char c = 0;
  Color fg_color = ColorWhite;
  Color bg_color = ColorBlack; // TODO

  std::bitset<CHAR_ATTR_COUNT> attr;
};


}