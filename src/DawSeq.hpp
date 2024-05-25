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

  enum {
    IsAsyncPlayMIDITerminated = 1,
  };
  struct SeqMsg {
    int type{};

    static SeqMsg OnAsyncPlayMIDITerminated() {
      return SeqMsg{IsAsyncPlayMIDITerminated};
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
    void AsyncPlayMIDIStopped();
    void StopMIDI();
    void CloseMIDIFile();
    void CalcCurrentMIDITimeBeat(uint64_t now_ms);
    void SetMIDITimeBeat(float time_beat);
    void ProcessMessage(std::function<bool(SeqMsg&)> proc);
    void CalcDuration();
    DawTrack * GetTrack(int track_num);
    int GetNumTracks();
    void SetPlayClockTime(unsigned long clk_time);
    bool IsPlaying();
    void AllMIDINoteOff();
    void Reset();

    DawDisplayState displayState;

    void SetDevice(BaseMidiOutDevice *dev);
  };
};
