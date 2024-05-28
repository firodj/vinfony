#include "jdksmidi/world.h"
#include "jdksmidi/midi.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <memory>
#include <map>

#include <fmt/core.h>
#include <fmt/color.h>

#include <libassert/assert.hpp>

#include "DawTrack.hpp"
#include "DawTrackNotes.hpp"
#include "DawSysEx.hpp"
#include "DawDoc.hpp"

// float m_uiDefaultTrackHeight = (float)(((int)ImGui::GetFrameHeightWithSpacing()*3/2) & ~1);

namespace vinfony {

DawDoc::DawDoc() {
  m_uiDefaultTrackHeight = 128;
  //m_lastTrackNum = 0;
}

DawDoc::~DawDoc() {

}

DawTrack * DawDoc::AddNewTrack(int midi_track_id, jdksmidi::MIDITrack * midi_track)
{
  m_tracks[midi_track_id] = std::make_unique<DawTrack>();

  DawTrack * track = m_tracks[midi_track_id].get();
  track->id = midi_track_id;

  track->h = m_uiDefaultTrackHeight;
  track->midi_track = std::make_unique<jdksmidi::MIDITrack>(*midi_track);

  m_trackNums.push_back(track->id);

  m_midiMultiTrack->SetTrack( midi_track_id, track->midi_track.get() );
  //m_lastTrackNum++;

  return track;
}

bool DawDoc::LoadFromMIDIMultiTrack( jdksmidi::MIDIMultiTrack *mlt ) {
  m_midiMultiTrack = std::make_unique<jdksmidi::MIDIMultiTrack>(64, false);

  if ( mlt->GetNumTracksWithEvents() == 1 ) {//
    fmt::println("all events in one track: format 0, separated them!");
    // redistributes channel events in separate tracks
    mlt->AssignEventsToTracks(0);
  }

  // Set PPQN
  m_midiMultiTrack->SetClksPerBeat(mlt->GetClksPerBeat());

  // Detect Track MIDI channels
  for (int trk_num=0; trk_num<mlt->GetNumTracks(); ++trk_num) {
    auto midi_track = mlt->GetTrack(trk_num);

    int ch = 0;
    std::string track_name;
    for (int event_num = 0; event_num < midi_track->GetNumEvents(); ++event_num) {
      const jdksmidi::MIDITimedBigMessage * msg = midi_track->GetEvent(event_num);
      if (msg->IsNoteOn()) {
        if (ch == 0) ch = msg->GetChannel()+1;
#if 0
        char msgbuf[1024];
        fmt::println("TRACK {} CH {} EVENT: {}", trk_num, channel, msg->MsgToText(msgbuf, 1024));
      } else if (ch != msg->GetChannel()+1) {
          fmt::println("WARNING: track {} has more than channels, was {} want {}", trk_num, channel,
            msg->GetChannel());
      }
#endif
      }

      if (msg->IsTrackName()) {
        if (track_name.empty()) track_name = msg->GetSysExString();
      }
    }
    DawTrack * track = AddNewTrack(trk_num, midi_track);
    track->ch = ch;
    track->name = track_name.empty() ?fmt::format("Track {}", trk_num) : track_name;
  }

  // Display
  fmt::println("Clocks per beat: {}", m_midiMultiTrack->GetClksPerBeat() );
  fmt::println("Tracks with events: {}", m_midiMultiTrack->GetNumTracksWithEvents() );
  fmt::println("Num Tracks {}", m_midiMultiTrack->GetNumTracks());

  // Assign Initial Track Values
  jdksmidi::MIDIMultiTrackIterator it( m_midiMultiTrack.get() );
  it.GoToTime( 0 );
  do {
    int trk_num;
    const jdksmidi::MIDITimedBigMessage *msg;

    if ( it.GetCurEvent( &trk_num, &msg ) )
    {
      auto track_it = m_tracks.find(trk_num);
      if (track_it == m_tracks.end()) continue;
      DawTrack * track = track_it->second.get();

      if (msg->IsProgramChange()) {
        for (const auto & kv: m_tracks) {
          if (kv.second->ch == msg->GetChannel() + 1) {
            if (kv.second->pg == 0) {
              kv.second->pg = msg->GetPGValue() + 1;
#if 1
              char msgbuf[1024];
              fmt::println("TRACK {} CH {} EVENT: {}", kv.second->id, track->ch, msg->MsgToText(msgbuf, 1024));
#endif
            }
          }
        }
      }

      if (msg->IsControlChange()) {
        for (const auto & kv: m_tracks) {
          if (kv.second->ch == msg->GetChannel() + 1) {
            if (kv.second->pg == 0) {
              kv.second->SetBank(msg);
            }
          }
        }
      }

      //fprintf( stdout, "#%2d - ", trk_num );
      //DumpMIDITimedBigMessage( msg );
    }
  }
  while ( it.GoToNextEvent() );

  for (auto trk_num: m_trackNums) {
    auto & track = m_tracks[trk_num];
    auto & midi_track = track->midi_track; // m_midiMultiTrack->GetTrack(trk_num);
    if (midi_track->IsTrackEmpty()) continue;
    vinfony::DawTrackNotes trackNotes{};

    bool eot = false;
    long stop_time = 0;
    int bank_msb = 0, bank_lsb = 0;

    for (int event_num = 0; event_num < midi_track->GetNumEvents(); ++event_num) {
      const jdksmidi::MIDITimedBigMessage * msg = midi_track->GetEvent(event_num);
      if (eot) {
        fmt::print(fmt::fg(fmt::color::wheat), "WARNING: events exist after eot\n");
      } else {
        stop_time = msg->GetTime();
      }

      if (msg->IsTrackName()) {
        fmt::println(msg->GetSysExString());
      } else if (msg->IsControlChange()) {
        switch (msg->GetController()) {
          case jdksmidi::C_GM_BANK:
            bank_msb = msg->GetControllerValue();
            break;
          case jdksmidi::C_GM_BANK_LSB:
            bank_lsb = msg->GetControllerValue();
            break;
        }
      } else if (msg->IsNoteOn()) {
        trackNotes.NoteOn( msg->GetTime(), msg->GetNote(), msg->GetVelocity() );
      } else if (msg->IsNoteOff()) {
        trackNotes.NoteOff( msg->GetTime(), msg->GetNote() );
      } else if (msg->IsProgramChange()) {
        fmt::print("Time:{} ", msg->GetTime());
        fmt::println("Program Change CH {} {}:{}:{}", msg->GetChannel()+1, bank_msb, bank_lsb, msg->GetPGValue());
      } else if (msg->IsEndOfTrack()) {
        eot = true;
      } else if (msg->IsSystemExclusive()) {
        fmt::print("Time:{} ", msg->GetTime());
        fmt::print(fmt::fg(fmt::color::aqua), "SYSEX CH{}, Data {} =", msg->GetChannel()+1, msg->GetSysEx()->GetLength());

        unsigned char * buf = (unsigned char *)msg->GetSysEx()->GetBuf();

        std::string description;

        for (int i=0; i<msg->GetSysEx()->GetLength(); i++, buf++) {
          fmt::print(fmt::fg(fmt::color::aqua), " {:02X}", *buf);
        }

        {
          std::unique_ptr<vinfony::GMSysEx> gmsysex(vinfony::GMSysEx::Create(msg->GetSysEx()));
          if (gmsysex) description += gmsysex->Info();
        }
        {
          std::unique_ptr<vinfony::GSSysEx> gssysex(vinfony::GSSysEx::Create(msg->GetSysEx()));
          if (gssysex) description += gssysex->Info();
        }
        fmt::print("\n=> {}\n", description);
      }
      //fmt::println("time: {}", msg->GetTime());
    }

    if (!eot) {
      fmt::print(fmt::fg(fmt::color::wheat), "WARNING: track without eot\n");
    }
    trackNotes.ClipOff(stop_time);
    fmt::println("notes process {}", trackNotes.notes_processed);
  }

  return true;
}

}