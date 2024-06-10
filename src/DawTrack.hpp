#pragma once

#include <string>
#include <functional>
#include <memory>

namespace jdksmidi {
  class MIDITrack;
  class MIDIBigMessage;
};

namespace vinfony {

  class DawSeq;

class DawTrack {
public:
  int id{0};
  std::string name;
  int h{20};
  unsigned int ch{0};  // MIDI channel, 0=none, (1..16)=channel
  unsigned int pg{0};  // Program value, (1..128)=program
  unsigned int bank{0};
  int midiVolume{-1}, midiPan{-1}; // -1 unset, 0 .. 16383
  int midiFilterFc{-1}, midiFilterQ{-1}; // -1 unset, 0 .. 127
  std::unique_ptr<jdksmidi::MIDITrack> midi_track{};
  int viewcache_start_event_num{0};
  long viewcache_start_visible_clk{-1};

  DawTrack();
  ~DawTrack();
  void SetBank(const jdksmidi::MIDIBigMessage * msg);
  void SetVolume(const jdksmidi::MIDIBigMessage * msg);
  void SetPan(const jdksmidi::MIDIBigMessage * msg);
  void SetFilter(const jdksmidi::MIDIBigMessage * msg);
  int GetGetDrumPart();
  void SetSeq(DawSeq * seq) { m_seq = seq; }

protected:
  DawSeq * m_seq{nullptr};
};

const char * GetStdProgramName(int pg);
const char * GetStdDrumName(int pg);

};