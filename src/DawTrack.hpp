#pragma once

#include <string>
#include <functional>
#include <memory>

namespace jdksmidi {
  class MIDITrack;
  class MIDIBigMessage;
};

namespace vinfony {
class DawTrack {
public:
  int id{0};
  std::string name;
  int h{20};
  unsigned int ch{0};  // MIDI channel, 0=none, (1..16)=channel
  unsigned int pg{0};  // Program value, (1..128)=program
  unsigned int bank{0};
  int midiVolume{-1}, midiPan{-1}; // -1 unset, 0 .. 16383
  std::unique_ptr<jdksmidi::MIDITrack> midi_track{};

  DawTrack();
  ~DawTrack();
  void SetBank(const jdksmidi::MIDIBigMessage * msg);
  void SetVolume(const jdksmidi::MIDIBigMessage * msg);
  void SetPan(const jdksmidi::MIDIBigMessage * msg);
};

const char * GetStdProgramName(int pg);

};