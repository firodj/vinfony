#pragma once

#include "jdksmidi/msg.h"
#include <memory>

namespace vinfony
{
  void DumpMIDITimedBigMessage( const jdksmidi::MIDITimedBigMessage *msg );

  class BaseMidiOutDevice {
  public:
    virtual ~BaseMidiOutDevice() {};
    virtual bool Init() { return true; };
    virtual void Shutdown() {};
    virtual bool HardwareMsgOut( const jdksmidi::MIDITimedBigMessage &msg ) {
      DumpMIDITimedBigMessage( &msg );
      return true;
    }
  };

  std::unique_ptr<BaseMidiOutDevice> CreateTsfDev();
};
