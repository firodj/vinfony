#pragma once

#include "BaseMidiOut.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace jdksmidi {
  class MIDITimedBigMessage;
};

namespace vinfony
{

using TsfAudioCallback = std::function<void(uint8_t *stream, int len)>;

class TinySoundFontDevice: public BaseMidiOutDevice {
protected:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

public:
  TinySoundFontDevice(std::string soundfontPath);
  ~TinySoundFontDevice();
  void UpdateMIDITicks() override;
  bool Init() override;
  void Shutdown() override;
  bool HardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg, double * msgTimeShiftMs ) override;

  bool RealHardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg );

  void SetAudioCallback(TsfAudioCallback cb);
  void Reset() override;

  void StdAudioCallback(uint8_t *stream, int len);
  int GetAudioSampleRate();
  void RenderStereoFloat(float* stream, int samples);
  void FlushToRealMsgOut();
};

};
