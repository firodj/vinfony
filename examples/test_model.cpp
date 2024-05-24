#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"
using namespace jdksmidi;

#include <tml.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
using namespace std;

#include <fmt/core.h>

static void DumpMIDITimedBigMessage( const MIDITimedBigMessage *msg )
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

void DumpMIDIMultiTrack( MIDIMultiTrack *mlt )
{
  MIDIMultiTrackIterator i( mlt );

  fmt::println("Clocks per beat: {}", mlt->GetClksPerBeat() );
  fmt::println("Tracks with events: {}", mlt->GetNumTracksWithEvents() );
  fmt::println("Num Tracks {}", mlt->GetNumTracks());

  for (int trk_num = 0; trk_num < mlt->GetNumTracks(); trk_num++) {
    auto midi_track = mlt->GetTrack(trk_num);
    if (midi_track->IsTrackEmpty()) continue;

     for (int event_num = 0; event_num < midi_track->GetNumEvents(); ++event_num) {
        const jdksmidi::MIDITimedBigMessage * msg = midi_track->GetEvent(event_num);
        if (msg->IsTrackName()) {
          fmt::println(msg->GetSysExString());
        }
        fmt::println("time: {}", msg->GetTime());
      }
  }
#if 0
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

int LoadUsingTML(const char * def_midpath) {
  //Venture (Original WIP) by Ximon
	tml_message* g_MidiMessage = tml_load_filename(def_midpath);
	if (!g_MidiMessage)
	{
		fprintf(stderr, "Could not load MIDI file: %s\n", def_midpath);
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

int LoadUsingJDKSMIDI(const char * def_midpath) {
  MIDIFileReadStreamFile rs( def_midpath );

#if 0
  {
    MIDIFileShow shower( stdout, /* sqspecific_as_text */ true);
    MIDIFileRead reader( &rs, &shower );
    // load the midifile into the multitrack object
    if ( !reader.Parse() )
    {
      cerr << "\nError parse file " << def_midpath << endl;
      return -1;
    }
  }
#else
  {
    MIDIMultiTrack tracks;
    MIDIFileReadMultiTrack track_loader( &tracks );
    MIDIFileRead reader( &rs, &track_loader );

    // set amount of tracks equal to midifile
    tracks.Clear(); //AndResize( reader.ReadNumTracks() );

    // load the midifile into the multitrack object
    if ( !reader.Parse() )
    {
      cerr << "\nError parse file " << def_midpath << endl;
      return -1;
    }

    if ( tracks.GetNumTracksWithEvents() == 1 ) {//
      fmt::println("all events in one track: format 0, separated them!");
      // redistributes channel events in separate tracks
      tracks.AssignEventsToTracks(0);
    }

    //      MIDISequencerGUIEventNotifierText notifier( stdout );
    //      MIDISequencer seq( &tracks, &notifier );
    MIDISequencer seq( &tracks );

    DumpMIDIMultiTrack( &tracks );
    //      cout << MultiTrackAsText( tracks ); // new util fun

    double dt = seq.GetMusicDurationInSeconds();

    printf("Music duration = %.3f", dt);
  }
#endif
  return 0;
}

int main( int argc, char **argv )
{
  const char *app_name = argv[0];

  if (argc <= 1) {
    cerr << "\nusage:\n    " << app_name << " FILE.mid\n";
  }

  //Venture (Original WIP) by Ximon
	//https://musescore.com/user/2391686/scores/841451
	//License: Creative Commons copyright waiver (CC0)
  const char * infile_name = argc >= 2 ? argv[1] : "ext/tsf/examples/venture.mid";

  LoadUsingJDKSMIDI(infile_name);

  return 0;
}