#include "BaseMidiOut.hpp"
#include <string>
namespace vinfony {

class BassMidiDevice: public BaseMidiOutDevice {
protected:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
public:
  BassMidiDevice(std::string soundfontPath);
  virtual ~BassMidiDevice();
  bool Init() override;
  bool HardwareMsgOut( const jdksmidi::MIDITimedBigMessage &msg, double * msgTimeShiftMs ) override;
  void Reset() override;
  void UpdateMIDITicks() override;
};

};
