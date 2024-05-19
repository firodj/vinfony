#pragma once

#include <string>

namespace vinfony {
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
    float GetMIDITimeBeat();
    void CalcCurrentMIDITimeBeat(uint64_t now);
  };
};
