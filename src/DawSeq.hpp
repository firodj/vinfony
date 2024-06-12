#pragma once

#include <string>
#include <functional>
#include <memory>

#include "circularfifo1.h"
#include "DawDisplay.hpp"

namespace jdksmidi {
  class MIDITrack;
  class MIDIBigMessage;
};

namespace vinfony {
  class TinySoundFontDevice;
  class BassMidiDevice;
  class DawTrack;

  enum {
    IsAsyncPlayMIDITerminated = 1,
    IsMIDIFileLoaded,
  };
  struct SeqMsg {
    int type{};
    std::string str{};

    static SeqMsg OnAsyncPlayMIDITerminated() {
      return SeqMsg{IsAsyncPlayMIDITerminated};
    }
    static SeqMsg OnMIDIFileLoaded(std::string midifile) {
      auto seqMsg = SeqMsg{IsMIDIFileLoaded};
      seqMsg.str = midifile;
      return seqMsg;
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
    //void AsyncPlayMIDI();
    //void AsyncPlayMIDIStopped();
    void PlayMIDI();
    void StopMIDI();
    void CloseMIDIFile();
    void CalcCurrentMIDITimeBeat(uint64_t now_ms);
    void SetMIDITimeBeat(float time_beat);
    void ProcessMessage(std::function<bool(SeqMsg&)> proc);
    void CalcDuration();
    DawTrack * GetTrack(int track_num);
    int GetNumTracks();
    void SetPlayClockTime(unsigned long clk_time);
    bool IsRewinding();
    bool IsPlaying();
    void AllMIDINoteOff();
    void SetTSFDevice(TinySoundFontDevice *dev);
    void SetBASSDevice(BassMidiDevice *dev);
    void Reset();
    void RenderMIDICallback(uint8_t * stream, int len);
    float GetTempoBPM();
    void GetCurrentMBT(int &m, int &b, int &t);
    void SendVolume(int chan, unsigned short value);
    void SendPan(int chan, unsigned short value);
    void SendFilter(int chan, unsigned short valFc, unsigned short valQ);
    TinySoundFontDevice * GetTSFDevice();
    BassMidiDevice * GetBASSDevice();

    DawDisplayState displayState;
  };
};
