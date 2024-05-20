#pragma once

#include <string>
#include <functional>
#include "circularfifo1.h"
#include "DawDisplay.hpp"

namespace vinfony {
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
    void CalcCurrentMIDITimeBeat(uint64_t now);
    void ProcessMessage(std::function<bool(SeqMsg&)> proc);
    void CalcDuration();

    DawDisplayState displayState;
  };
};
