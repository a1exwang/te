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
TTYInputType TTYInput::receive_char(char cc) {
  uint8_t b = cc;
  if (input_state == InputState::Escape) {
    if (b == '[') {
      // CSI
      buffer_.clear();
      input_state = InputState::CSI;
      return TTYInputType::Intermediate;
    } else if (b == 0x1b) {
      // multiple ESC
      input_state = InputState::Escape;
      return TTYInputType::Intermediate;
    } else if (b == 'P' || b == ']') {
      buffer_.clear();
      buffer_.push_back(b);
      input_state = InputState::WaitForST;
      return TTYInputType::Intermediate;
    } else {
      input_state = InputState::Idle;
      return TTYInputType::Char;
    }
  } else if (input_state == InputState::Idle) {
    if (b >= 0b11000000u) {
      buffer_.clear();
      // 2 bytes
      if ((b & 0xe0u) == 0xc0u) {
        utf8_total = 2;
      } else if ((b & 0xf0u) == 0xe0u) {
        utf8_total = 3;
      } else if ((b & 0xf8u) == 0xf0u) {
        utf8_total = 4;
      } else {
        return TTYInputType::Char;
      }
      buffer_.push_back(b);
      utf8_read = 1;
      input_state = InputState::UTF8;
      return TTYInputType::Intermediate;
    } else if (b == 0x1b) {
      input_state = InputState::Escape;
      return TTYInputType::Intermediate;
    } else {
      return TTYInputType::Char;
    }
  } else if (input_state == InputState::CSI) {
    // CSI:
    buffer_.push_back(b);

    if (csi_is_final_byte(b)) {
      input_state = InputState::Idle;
      return TTYInputType::CSI;
    } else {
      return TTYInputType::Intermediate;
    }
  } else if (input_state == InputState::WaitForST) {
    if (b == '\a') {
      // xterm ST
      input_state = InputState::Idle;
      return TTYInputType::TerminatedByST;
    } else if (b == 0x9c) {
      input_state = InputState::Idle;
      return TTYInputType::TerminatedByST;
    } else if (b == '\\') {
      if (!buffer_.empty() && buffer_.back() == ESC) {
        // delete the previous ESC
        input_state = InputState::Idle;
        buffer_.pop_back();
        return TTYInputType::TerminatedByST;
      } else {
        buffer_.push_back(b);
        return TTYInputType::Intermediate;
      }
    } else {
      buffer_.push_back(b);
      return TTYInputType::Intermediate;
    }
  } else if (input_state == InputState::UTF8) {
    buffer_.push_back(b);
    utf8_read++;
    if (utf8_read == utf8_total) {
      input_state = InputState::Idle;
      return TTYInputType::UTF8;
    } else {
      return TTYInputType::Intermediate;
    }
  } else {
    assert(0);
    return TTYInputType::Unknown;
  }
}


}