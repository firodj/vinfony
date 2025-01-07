#pragma once

#include <functional>
#include <memory>

#include "IDawTrack.hpp"

namespace jdksmidi
{
	class MIDITrack;
	class MIDIBigMessage;
};

namespace vinfony
{

	class DawSeq;

	class DawTrack: public IDawTrack {
	public:
		int id{0};
		std::string name;
		int h{20};
		unsigned int ch{0};  // MIDI channel, 0=none, (1..16)=channel
		unsigned int pg{0};  // Program value, (1..128)=program
		unsigned int bank{0};
		int midiVolume{-1}, midiPan{-1}; // -1 unset, 0 .. 16383
		int midiFilterFc{-1}, midiFilterQ{-1}; // -1 unset, 0 .. 127
		std::unique_ptr<jdksmidi::MIDITrack> midi_track{};
		int viewcache_start_event_num{0};
		long viewcache_start_visible_clk{-1};

		DawTrack();
		~DawTrack();
		void SetBank(const jdksmidi::MIDIBigMessage * msg);
		void SetVolume(const jdksmidi::MIDIBigMessage * msg);
		void SetPan(const jdksmidi::MIDIBigMessage * msg);
		void SetFilter(const jdksmidi::MIDIBigMessage * msg);
		int GetDrumPart() override;
		void SetSeq(DawSeq * seq) { m_seq = seq; }

		unsigned int & GetCh() override { return ch; }
		unsigned int GetPg() override { return pg; }
		unsigned int GetBank() override { return bank; }
		const char * GetName() override { return name.c_str(); }
		int GetId() override { return id; }
		int & GetMidiVolume() override { return midiVolume; }
		int & GetMidiPan() override { return midiPan; }
		int & GetMidiFilterFc() override { return midiFilterFc; }
		int & GetMidiFilterQ() override { return midiFilterQ; }
		int & GetH() override { return h; }
		jdksmidi::MIDITrack * GetMidiTrack() override { return midi_track.get(); }

		int & GetViewcacheStartEventNum() override { return viewcache_start_event_num; }
		long & GetViewcacheStartVisibleClk() override { return viewcache_start_visible_clk; }

	protected:
		DawSeq * m_seq{nullptr};
	};


};