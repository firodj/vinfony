#include "BaseMidiOut.hpp"
#include "Misc.hpp"

namespace vinfony
{
    BaseMidiOutDevice::~BaseMidiOutDevice()
    {

    }

    bool BaseMidiOutDevice::Init()
    {
        return true;
    }

    bool BaseMidiOutDevice::HardwareMsgOut(
        const jdksmidi::MIDITimedBigMessage &msg,
        double * msgTimeShiftMs
    )
    {
        (void)msgTimeShiftMs;
		DumpMIDITimedBigMessage( &msg );
		return true;
	}

    void BaseMidiOutDevice::Reset()
    {

    }

    void BaseMidiOutDevice::UpdateMIDITicks()
    {

    }

    int BaseMidiOutDevice::GetAudioSampleRate()
    {
        return 44100;
    }
}