#pragma once

namespace vinfony {
  // DawDisplayState
  // Update by Main Thread by Msg Bus thru calling ProcessMsg()
  // Read by Main Thread directly.
  struct DawDisplayState {
    float play_cursor{};
    float duration_ms{};
    float play_duration{};
  };
};