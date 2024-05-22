#pragma once

#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "jdksmidi/msg.h"

namespace vinfony {

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

}