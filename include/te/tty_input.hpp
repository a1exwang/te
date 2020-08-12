#pragma once

#include <cinttypes>
#include <string>
#include <vector>

namespace te {

enum class TTYInputType {
  Char,
  CSI,
  Unknown,
  Intermediate,
  TerminatedByST,
  UTF8,
};

class TTYInput {
 public:
  TTYInputType receive_char(char c);

  enum class InputState {
    Idle,
    Escape,
    CSI,
    WaitForST,
    UTF8,
  };

  InputState input_state = InputState::Idle;
  int utf8_read = 0, utf8_total = 0;
  std::string buffer_;
};


};
