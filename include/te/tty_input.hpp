#pragma once

#include <cinttypes>
#include <vector>

namespace te {

enum class TTYInputType {
  Char,
  CSI,
  Unknown,
  Intermediate,
  TerminatedByST,
};

class TTYInput {
 public:
  TTYInputType receive_char(uint8_t c);

  enum class InputState {
    Idle,
    Escape,
    CSI,
    WaitForST,
  };

  InputState input_state = InputState::Idle;
  std::vector<std::uint8_t> buffer_;
};


};
