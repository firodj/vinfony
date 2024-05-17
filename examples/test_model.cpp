#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"
using namespace jdksmidi;

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
using namespace std;

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
      fprintf( stdout, "%8ld : %s <------------------>", msg->GetTime(), msg->MsgToText( msgbuf ) );
    }
    else
    {
      fprintf( stdout, "%8ld : %s", msg->GetTime(), msg->MsgToText( msgbuf ) );
    }

    if ( msg->IsSystemExclusive() )
    {
      fprintf( stdout, "SYSEX length: %d", msg->GetSysEx()->GetLengthSE() );
    }

    fprintf( stdout, "\n" );
  }
}

void DumpMIDIMultiTrack( MIDIMultiTrack *mlt )
{
  MIDIMultiTrackIterator i( mlt );
  const MIDITimedBigMessage *msg;
  fprintf( stdout, "Clocks per beat: %d\n\n", mlt->GetClksPerBeat() );
  i.GoToTime( 0 );

  do
  {
    int trk_num;

    if ( i.GetCurEvent( &trk_num, &msg ) )
    {
      fprintf( stdout, "#%2d - ", trk_num );
      DumpMIDITimedBigMessage( msg );
    }
  }
  while ( i.GoToNextEvent() );
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

  MIDIFileReadStreamFile rs( infile_name );
  MIDIMultiTrack tracks;
  MIDIFileReadMultiTrack track_loader( &tracks );

  MIDIFileShow shower( stdout, /* sqspecific_as_text */ true);

  MIDIFileRead reader( &rs, &shower );

  // set amount of tracks equal to midifile
  tracks.ClearAndResize( reader.ReadNumTracks() );

  //      MIDISequencerGUIEventNotifierText notifier( stdout );
  //      MIDISequencer seq( &tracks, &notifier );
  MIDISequencer seq( &tracks );

  // load the midifile into the multitrack object
  if ( !reader.Parse() )
  {
    cerr << "\nError parse file " << infile_name << endl;
    return -1;
  }

  DumpMIDIMultiTrack( &tracks );

  //      cout << MultiTrackAsText( tracks ); // new util fun

  double dt = seq.GetMisicDurationInSeconds();

  cout << "\nMusic duration = " << dt << endl;

  return 0;
}