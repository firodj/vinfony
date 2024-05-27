#include "jdksmidi/world.h"
#include "jdksmidi/midi.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"

#include <tml.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <memory>
#include <map>

#include <fmt/core.h>
#include <fmt/color.h>

#include <libassert/assert.hpp>

#include "DawSeq.hpp"
#include "DawTrackNotes.hpp"
#include "DawSysEx.hpp"

namespace vinfony {
  struct DawDoc {
    std::map<int, std::unique_ptr<DawTrack>> tracks;
    int last_tracks_id{0};
    std::vector<int> track_nums;

    DawTrack * AddNewTrack(std::string name, jdksmidi::MIDITrack * midi_track);
  };

  DawTrack * DawDoc::AddNewTrack(std::string name, jdksmidi::MIDITrack * midi_track)
  {
    last_tracks_id++;
    tracks[last_tracks_id] = std::make_unique<DawTrack>();

    DawTrack * track = tracks[last_tracks_id].get();
    track->id = last_tracks_id;
    track->name = name;
    float ht = 120; // FIXME: view/style
    track->h = ht;
    track->midi_track = midi_track;

    track_nums.push_back(track->id);

    return track;
  }
};

int LoadUsingJDKSMIDI(const char * def_midipath);
void DumpMIDIMultiTrack( jdksmidi::MIDIMultiTrack *mlt );
void DumpMIDITimedBigMessage( const jdksmidi::MIDITimedBigMessage *msg );

int main( int argc, char **argv )
{
  const char *app_name = argv[0];

  if (argc <= 1) {
    fmt::print(fmt::fg(fmt::color::crimson), "\nusage:\n    {} FILE.mid\n", app_name);
  }

  //Venture (Original WIP) by Ximon
	//https://musescore.com/user/2391686/scores/841451
	//License: Creative Commons copyright waiver (CC0)
  const char * infile_name = argc >= 2 ? argv[1] : "ext/tsf/examples/venture.mid";

  LoadUsingJDKSMIDI(infile_name);

  return 0;
}

int LoadUsingJDKSMIDI(const char * def_midipath) {
  jdksmidi::MIDIFileReadStreamFile rs( def_midipath );

#if 0
  {
    MIDIFileShow shower( stdout, /* sqspecific_as_text */ true);
    MIDIFileRead reader( &rs, &shower );
    // load the midifile into the multitrack object
    if ( !reader.Parse() )
    {
      fmt::print(fmt::fg(fmt::color::crimson), "\nError parse file {}", def_midipath);
      return -1;
    }
  }
#else
  {
    jdksmidi::MIDIMultiTrack tracks;
    jdksmidi::MIDIFileReadMultiTrack track_loader( &tracks );
    jdksmidi::MIDIFileRead reader( &rs, &track_loader );

    // set amount of tracks equal to midifile
    tracks.Clear(); //AndResize( reader.ReadNumTracks() );

    // load the midifile into the multitrack object
    if ( !reader.Parse() )
    {
      fmt::print(fmt::fg(fmt::color::crimson), "\nError parse file {}", def_midipath);
      return -1;
    }

    if ( tracks.GetNumTracksWithEvents() == 1 ) {//
      fmt::println("all events in one track: format 0, separated them!");
      // redistributes channel events in separate tracks
      tracks.AssignEventsToTracks(0);
    }

    //      MIDISequencerGUIEventNotifierText notifier( stdout );
    //      MIDISequencer seq( &tracks, &notifier );
    jdksmidi::MIDISequencer seq( &tracks );

    DumpMIDIMultiTrack( &tracks );
    //      fmt::print(fmt::fg(fmt::color::floral_white), "Dump {}\n", MultiTrackAsText( tracks ));

    double dt = seq.GetMusicDurationInSeconds();

    printf("Music duration = %.3f", dt);
  }
#endif
  return 0;
}

void DumpMIDITimedBigMessage( const jdksmidi::MIDITimedBigMessage *msg )
{
  if ( msg )
  {
    char msgbuf[1024];

    // note that Sequencer generate SERVICE_BEAT_MARKER in files dump,
    // but files themselves not contain this meta event...
    // see MIDISequencer::beat_marker_msg.SetBeatMarker()
    if ( msg->IsBeatMarker() )
    {
      fprintf( stdout, "%8ld : %s <------------------>", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );
    }
    else
    {
      fprintf( stdout, "%8ld : %s", msg->GetTime(), msg->MsgToText( msgbuf, 1024 ) );
    }

    if ( msg->IsSystemExclusive() )
    {
      fprintf( stdout, "SYSEX length: %d", msg->GetSysEx()->GetLengthSE() );
    }
    else if ( msg->IsMetaEvent() ) {
      if ( msg->IsKeySig() ) {
        fmt::print("Key Signature  ");
        msg->GetKeySigSharpFlats();
        const char * keynmes[] = {
          "F", "G♭", "G", "A♭", "A", "B♭", "B" ,"C", "C♯", "D", "D♯", "E", "F", "F#", "G",
        };
        int i = msg->GetKeySigSharpFlats();
        if (i >= -7 && i <= 7) {
          fmt::print(keynmes[i+7]);
        }
        if (msg->GetKeySigMajorMinor() == 1)
          fmt::print(" Minor");
        else
          fmt::print(" Major");
      } else if ( msg->IsTimeSig() ) {

        fmt::print("Time Signature  {}/{}  Clks/Metro.={} 32nd/Quarter={}",
          (int)msg->GetTimeSigNumerator(),
          (int)msg->GetTimeSigDenominator(),
          (int)msg->GetTimeSigMidiClocksPerMetronome(),
          (int)msg->GetTimeSigNum32ndPerMidiQuarterNote()
        );

      } else if (msg->IsTempo()) {
        fmt::print("Tempo    {} BPM ({} usec/beat)", msg->GetTempo32()/32.0, msg->GetTempo());
      } else if (msg->IsEndOfTrack()) {
        fmt::print("End of Track");
      }

      else
      if (msg->GetSysEx()) {
        const unsigned char *buf = msg->GetSysEx()->GetBuf();
        int len = msg->GetSysEx()->GetLengthSE();
        std::string str;
        for ( int i = 0; i < len; ++i ) {
          if (buf[i] >= 0x20 && buf[i] <= 0x7F)
            str.push_back( (char)buf[i] );
        }
        fmt::print(" Data: {}", str);
      }
    }

    fprintf( stdout, "\n" );
  }
}

void DumpMIDIMultiTrack( jdksmidi::MIDIMultiTrack *mlt )
{
  fmt::println("Clocks per beat: {}", mlt->GetClksPerBeat() );
  fmt::println("Tracks with events: {}", mlt->GetNumTracksWithEvents() );
  fmt::println("Num Tracks {}", mlt->GetNumTracks());

  for (int trk_num = 0; trk_num < mlt->GetNumTracks(); trk_num++) {
    auto midi_track = mlt->GetTrack(trk_num);
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
#if 0
  jdksmidi::MIDIMultiTrackIterator i( mlt );
  i.GoToTime( 0 );
  do
  {
    int trk_num;
    const MIDITimedBigMessage *msg;

    if ( i.GetCurEvent( &trk_num, &msg ) )
    {
      fprintf( stdout, "#%2d - ", trk_num );
      DumpMIDITimedBigMessage( msg );
    }
  }
  while ( i.GoToNextEvent() );
#endif
}

int LoadUsingTML(const char * def_midipath) {
  //Venture (Original WIP) by Ximon
	tml_message* g_MidiMessage = tml_load_filename(def_midipath);
	if (!g_MidiMessage)
	{
		fprintf(stderr, "Could not load MIDI file: %s\n", def_midipath);
		return 1;
	}

	//Set up the global MidiMessage pointer to the first MIDI message
  for (; g_MidiMessage; g_MidiMessage = g_MidiMessage->next)
  {
    switch (g_MidiMessage->type)
    {
    default:
      case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
					printf( "evtm=%06d : Ch %2d PROG CHANGE    PG %3d\n", g_MidiMessage->time, g_MidiMessage->channel, g_MidiMessage->program);
					break;
    }
  }

  return 0;
}
