#pragma once

#include <string>
#include <functional>
#include <memory>

#include "circularfifo1.h"
#include "IDawSeq.hpp"

// Config
#define USE_BASSMIDI 1

namespace jdksmidi {
  class MIDITrack;
  class MIDIBigMessage;
};

namespace vinfony {
  class TinySoundFontDevice;
  class BassMidiDevice;
  class IDawTrack;

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

  class DawSeq: public IDawSeq {
  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl{};

  protected:
    bool ReadMIDIFile(std::string filename);

  public:
    DawSeq();
    ~DawSeq();

    void AsyncReadMIDIFile(std::string filename);
    bool IsFileLoaded() override;
    void AsyncPlayMIDI() override;
    void AsyncPlayMIDIStopped() override;
    void StopMIDI() override;
    void CloseMIDIFile();
    void CalcCurrentMIDITimeBeat(uint64_t now_ms);
    void SetMIDITimeBeat(float time_beat) override;
    void ProcessMessage(std::function<bool(SeqMsg&)> proc);
    void CalcDuration();
    IDawTrack * GetTrack(int track_num) override;
    int GetNumTracks() override;
    void SetPlayClockTime(unsigned long clk_time);
    bool IsRewinding() override;
    bool IsPlaying() override;
    void AllMIDINoteOff();
    void SetTSFDevice(TinySoundFontDevice *dev);
    void SetBASSDevice(BassMidiDevice *dev);
    void Reset();
    void RenderMIDICallback(uint8_t * stream, int len);
    float GetTempoBPM() override;
    void GetCurrentMBT(int &m, int &b, int &t) override;
    void SendVolume(int chan, unsigned short value) override;
    void SendPan(int chan, unsigned short value) override;
    void SendFilter(int chan, unsigned short valFc, unsigned short valQ) override;
    TinySoundFontDevice * GetTSFDevice();
    BassMidiDevice * GetBASSDevice();
    DawDisplayState *GetDisplayState() override;

    const char * GetStdProgramName(int pg) override;
    const char * GetStdDrumName(int pg) override;

    std::unique_ptr<IDawTrackNotes> CreateDawTrackNotes() override;
  };
};
