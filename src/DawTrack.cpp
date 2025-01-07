#include "DawTrack.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
using namespace std::chrono_literals;

#include <map>
#include <memory>
#include <thread>

#include <SDL.h>
#include <imgui.h>

#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"

#include <fmt/core.h>
#include "DawSeq.hpp"
#include "TsfDev.hpp"

namespace vinfony
{

	DawTrack::DawTrack()
	{

	}

	DawTrack::~DawTrack()
	{

	}

	void DawTrack::SetBank(const jdksmidi::MIDIBigMessage * msg)
	{
		auto control_value = msg->GetControllerValue();
		switch (msg->GetController()) {
			case jdksmidi::C_GM_BANK:
				bank = (0x8000 | control_value);
				break;
			case jdksmidi::C_GM_BANK_LSB:
				bank = (
					(bank & 0x8000 ? ((bank & 0x7F) << 7) : 0) | control_value);
				break;
		}
	}

	void DawTrack::SetVolume(const jdksmidi::MIDIBigMessage * msg)
	{
		auto control_value = msg->GetControllerValue();
		switch (msg->GetController()) {
			case jdksmidi::C_MAIN_VOLUME:
				midiVolume = (unsigned short)((midiVolume & 0x7F  ) | (control_value << 7));
				break;
			case jdksmidi::C_MAIN_VOLUME_LSB:
				midiVolume = (unsigned short)((midiVolume & 0x3F80) | control_value);
				break;
			default:
				return;
		}
	}

	void DawTrack::SetPan(const jdksmidi::MIDIBigMessage * msg)
	{
		auto control_value = msg->GetControllerValue();
		switch (msg->GetController()) {
			case jdksmidi::C_PAN:
				midiPan = (unsigned short)((midiPan & 0x7F  ) | (control_value << 7));
				break;
			case jdksmidi::C_PAN_LSB:
				midiPan = (unsigned short)((midiPan & 0x3F80) | control_value);
				break;
			default:
				return;
		}
	}

	void DawTrack::SetFilter(const jdksmidi::MIDIBigMessage * msg)
	{
		auto control_value = msg->GetControllerValue();
		switch (msg->GetController()) {
			case jdksmidi::C_HARMONIC:
				midiFilterQ = (unsigned short)control_value;
				break;
			case jdksmidi::C_BRIGHTNESS:
				midiFilterFc = (unsigned short)control_value;
				break;
			default:
				return;
		}
	}

	int DawTrack::GetDrumPart()
	{
		if (m_seq) return m_seq->GetTSFDevice()->GetDrumPart(ch ? (ch-1) & 0xF : 0);
		return 0;
	}

}