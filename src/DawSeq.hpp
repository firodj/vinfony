#pragma once

#include <string>
#include <functional>
#include <memory>

#include "circularfifo1.h"
#include "DawDisplay.hpp"

namespace jdksmidi {
  class MIDITrack;
};

namespace vinfony {
  class BaseMidiOutDevice;

  struct DawTrack {
    int id;
    std::string name;
    int h;
    jdksmidi::MIDITrack * midi_track;
  };

  struct SeqMsg {
    int type{};

    static SeqMsg ThreadTerminate() {
      return SeqMsg{1};
    }
  };

  class DawSeq {
  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl{};

  protected:
    bool ReadMIDIFile(std::string filename);

  public:
    DawSeq();
    ~DawSeq();

    void AsyncReadMIDIFile(std::string filename);
    bool IsFileLoaded();
    void AsyncPlayMIDI();
    void StopMIDI();
    void CloseMIDIFile();
    void CalcCurrentMIDITimeBeat(uint64_t now);
    void ProcessMessage(std::function<bool(SeqMsg&)> proc);
    void CalcDuration();
    DawTrack * GetTrack(int track_num);
    int GetNumTracks();
    void SetPlayClockTime(unsigned long clk_time);

    DawDisplayState displayState;

    void SetDevice(BaseMidiOutDevice *dev);
  };
};
