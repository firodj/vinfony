#pragma once

#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "jdksmidi/msg.h"

namespace vinfony
{
	class BaseMidiOutDevice
	{
	public:
		virtual ~BaseMidiOutDevice();
		virtual bool Init();
		virtual bool HardwareMsgOut(
			const jdksmidi::MIDITimedBigMessage &msg,
			double * msgTimeShiftMs
		);
		virtual void Reset();
		virtual void UpdateMIDITicks();
		virtual int	GetAudioSampleRate();
		virtual const char * GetName() = 0;
	};

}