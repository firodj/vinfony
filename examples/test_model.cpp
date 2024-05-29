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
#include "DawDoc.hpp"


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

    vinfony::DawDoc doc;
    doc.LoadFromMIDIMultiTrack( &tracks );
    //      fmt::print(fmt::fg(fmt::color::floral_white), "Dump {}\n", MultiTrackAsText( tracks ));

    //      MIDISequencerGUIEventNotifierText notifier( stdout );
    //      MIDISequencer seq( &tracks, &notifier );
    jdksmidi::MIDISequencer seq( doc.GetMIDIMultiTrack() );

    double dt = seq.GetMusicDurationInSeconds();

    printf("Music duration = %.3f", dt);
  }
#endif
  return 0;
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
