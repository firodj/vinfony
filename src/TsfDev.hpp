#pragma once

#include "BaseMidiOut.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>

struct tsf;
namespace jdksmidi {
  class MIDITimedBigMessage;
};
namespace vinfony {
class TinySoundFontDevice: public BaseMidiOutDevice {
protected:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

public:
  TinySoundFontDevice(std::string soundfontPath);
  ~TinySoundFontDevice();
  void UpdateMIDITicks() override;
  bool Init() override;
  bool HardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg, double * msgTimeShiftMs ) override;

  bool RealHardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg );

  void SetSampleRate(int sampleRate);
  void Advance(int sampleCount);
  void Reset() override;

  void StdAudioCallback(uint8_t *stream, int len);
  int GetAudioSampleRate() override;
  void RenderStereoFloat(float* stream, int samples);
  void FlushToRealMsgOut();
  int GetDrumPart(int ch);

  tsf * GetTSF();

  const char * GetName() override;
};

};
