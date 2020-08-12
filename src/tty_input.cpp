#include <te/tty_input.hpp>

#include <cassert>

#include <te/basic.hpp>

namespace te {

/**
 * ECMA-035
 *
 * 13.2 Types of escape sequences
 *
 * ESC TYPE ... F
 *
 * TYPE:
 * 0x0x/0x1x    .       Shall not be used
 * 0x2x         nF      (see table 3.b)
 * 0x3x         Fp      Private control function (see 6.5.3)
 * 0x4x/0x5x    Fe      Control function in the C1 set (see 6.4.3)
 * 0x6x/0x7x(except 0x7f)Fe Standardized single control function (see 6.5.1)
 *
 *
 * 13.2.2 Escape Sequences of types nF
 * ESC I .. F where the notation ".." indicates that more than one Intermediate Byte may appear in the sequence.
 *
 */

// 0123456789:;<=>?
bool csi_is_parameter_byte(uint8_t c) {
  return 0x30u == (c & 0xf0u);
}

// !"#$%&'()*+,-./
bool csi_is_intermediate_byte(uint8_t c) {
  return 0x20u == (c & 0xf0u);
}

bool csi_is_final_byte(uint8_t c) {
  return c >= 0x40 && c <= 0x7E;
}
// ST: String Terminator 0x9c or ESC 0x5c, could also be 0x07 in xterm
TTYInputType TTYInput::receive_char(uint8_t c) {
  if (input_state == InputState::Escape) {
    if (c == '[') {
      // CSI
      buffer_.clear();
      input_state = InputState::CSI;
      return TTYInputType::Intermediate;
    } else if (c == 0x1b) {
      // multiple ESC
      input_state = InputState::Escape;
      return TTYInputType::Intermediate;
    } else if (c == 'P' || c == ']') {
      buffer_.clear();
      buffer_.push_back(c);
      input_state = InputState::WaitForST;
      return TTYInputType::Intermediate;
    } else {
      input_state = InputState::Idle;
      return TTYInputType::Char;
    }
  } else if (input_state == InputState::Idle) {
    if (c == 0x1b) {
      input_state = InputState::Escape;
      return TTYInputType::Intermediate;
    } else {
      return TTYInputType::Char;
    }
  } else if (input_state == InputState::CSI) {
    // CSI:
    buffer_.push_back(c);

    if (csi_is_final_byte(c)) {
      input_state = InputState::Idle;
      return TTYInputType::CSI;
    } else {
      return TTYInputType::Intermediate;
    }
  } else if (input_state == InputState::WaitForST) {
    if (c == '\a') {
      // xterm ST
      input_state = InputState::Idle;
      return TTYInputType::TerminatedByST;
    } else if (c == 0x9c) {
      input_state = InputState::Idle;
      return TTYInputType::TerminatedByST;
    } else if (c == '\\') {
      if (!buffer_.empty() && buffer_.back() == ESC) {
        // delete the previous ESC
        input_state = InputState::Idle;
        buffer_.pop_back();
        return TTYInputType::TerminatedByST;
      } else {
        buffer_.push_back(c);
        return TTYInputType::Intermediate;
      }
    } else {
      buffer_.push_back(c);
      return TTYInputType::Intermediate;
    }
  } else {
    assert(0);
    return TTYInputType::Unknown;
  }
}


}