#include "DawTrack.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
using namespace std::chrono_literals;

#include <map>
#include <memory>
#include <thread>

#include <SDL.h>
#include <imgui.h>

#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"

#include <fmt/core.h>

namespace vinfony {
  DawTrack::DawTrack() {}

  DawTrack::~DawTrack() {}

  void DawTrack::SetBank(const jdksmidi::MIDIBigMessage * msg) {
    auto control_value = msg->GetControllerValue();
    switch (msg->GetController()) {
      case jdksmidi::C_GM_BANK:
        bank = (0x8000 | control_value);
        break;
      case jdksmidi::C_GM_BANK_LSB:
        bank = (
          (bank & 0x8000 ? ((bank & 0x7F) << 7) : 0) | control_value);
        break;
    }
  }
}