/*
 *  libjdksmidi-2004 C++ Class Library for MIDI
 *
 *  Copyright (C) 2004  J.D. Koftinoff Software, Ltd.
 *  www.jdkoftinoff.com
 *  jeffk@jdkoftinoff.com
 *
 *  *** RELEASED UNDER THE GNU GENERAL PUBLIC LICENSE (GPL) April 27, 2004 ***
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
//
// Copyright (C) 2010 V.R.Madgazin
// www.vmgames.com vrm@vmgames.com
//

#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"
using namespace jdksmidi;

#ifdef USE_BASS
#include "bass.h"
#include "bassmidi.h"
#endif

#ifdef USE_TSF
#include <SDL.h>
#include <tsf.h>
#endif

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
using namespace std;

#include "circularfifo1.h"

RtMidi::Api chooseMidiApi();
bool chooseMidiPort( RtMidiOut *rtmidi );
bool g_abort = false;

static void sigHandler(int sig) {
  printf("Signal %d received, aborting\n", sig);
  g_abort = true;
};

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

class BaseMidiOutDevice {
public:
  virtual ~BaseMidiOutDevice() {};
  virtual bool Init() { return true; };
  virtual bool HardwareMsgOut( const MIDITimedBigMessage &msg ) {
    DumpMIDITimedBigMessage( &msg );
    return true;
  }
};

#if 0
void DumpMIDITrack( MIDITrack *t )
{
  MIDITimedBigMessage *msg;

  for ( int i = 0; i < t->GetNumEvents(); ++i )
  {
    msg = t->GetEventAddress( i );
    DumpMIDITimedBigMessage( msg );
  }
}

void DumpAllTracks( MIDIMultiTrack *mlt )
{
  fprintf( stdout, "Clocks per beat: %d\n\n", mlt->GetClksPerBeat() );

  for ( int i = 0; i < mlt->GetNumTracks(); ++i )
  {
    if ( mlt->GetTrack( i )->GetNumEvents() > 0 )
    {
      fprintf( stdout, "DUMP OF TRACK #%2d:\n", i );
      DumpMIDITrack( mlt->GetTrack( i ) );
      fprintf( stdout, "\n" );
    }
  }
}
#endif

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

void PlaySequencer( MIDISequencer *seq, BaseMidiOutDevice *dev )
{
  float pretend_clock_time = 0.0;
  float next_event_time = 0.0;
  MIDITimedBigMessage msg;
  int ev_track;

  if (!dev->Init()) return;

  seq->GoToTimeMs( pretend_clock_time );

  if ( !seq->GetNextEventTimeMs( &next_event_time ) )
  {
    return;
  }

  // simulate a clock going forward with 10 ms resolution for 1 hour
  float max_time = 3600. * 1000.;
  const auto start = std::chrono::high_resolution_clock::now();
  for ( ; pretend_clock_time < max_time; pretend_clock_time = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count())
  {
    // find all events that came before or a the current time
    while ( next_event_time <= pretend_clock_time )
    {
      if ( seq->GetNextEvent( &ev_track, &msg ) )
      {
        // found the event!
        // show it to stdout
        fprintf( stdout, "tm=%06.0f : evtm=%06.0f :trk%02d : ", pretend_clock_time, next_event_time, ev_track );
        if (!dev->HardwareMsgOut( msg )) return;
        // now find the next message

        if ( !seq->GetNextEventTimeMs( &next_event_time ) )
        {
          // no events left so end
          fprintf( stdout, "End\n" );
          return;
        }
      }
    }
    std::this_thread::sleep_for(10ms);
    if (g_abort) break;
  }
}

class RtMidiDevice: public BaseMidiOutDevice {
protected:
  RtMidiOut * pMidiOut{};
public:
  bool Init() override {
    pMidiOut = new RtMidiOut(chooseMidiApi());
    // Call function to select port.
    try {
      if ( chooseMidiPort( pMidiOut ) == false ) return false;
    }
    catch ( RtMidiError &error ) {
      error.printMessage();
      return false;
    }
    return true;
  }
  bool HardwareMsgOut ( const MIDITimedBigMessage &msg ) override;
};

bool RtMidiDevice::HardwareMsgOut( const MIDITimedBigMessage &msg )
{
  std::vector<unsigned char> message((size_t)3);

  char msgbuf[1024];

  if ( pMidiOut != NULL )
  {
    try
    {
      if ( msg.IsBeatMarker() )
      {
        fprintf( stdout, "%8ld : %s <------------------>", msg.GetTime(), msg.MsgToText( msgbuf ) );
      }
      else if ( msg.IsChannelEvent() )
      {
        message[0] = msg.GetStatus();
        message[1] = msg.GetByte1();
        message[2] = msg.GetByte2();
        if (msg.GetLength() != message.size())
          message.resize(msg.GetLength());
        pMidiOut->sendMessage( &message );

        fprintf( stdout, "%8ld : %s", msg.GetTime(), msg.MsgToText( msgbuf ) );
      }
      else if ( msg.IsSystemExclusive() )
      {
        std::vector<unsigned char> sysexmessage((size_t)1+msg.GetSysEx()->GetLength());

        sysexmessage[0] = msg.GetStatus();
        memcpy( &sysexmessage[1], msg.GetSysEx()->GetBuf(), msg.GetSysEx()->GetLength() );
        pMidiOut->sendMessage( &sysexmessage );

        fprintf( stdout, "SYSEX length: %d", msg.GetSysEx()->GetLengthSE() );
      }
      else
      {
        fprintf( stdout, "%8ld : %s (Skipped)", msg.GetTime(), msg.MsgToText( msgbuf ) );
      }

      fprintf( stdout, "\n" );

      return true;
    }
    catch ( RtMidiError &error )
    {
      error.printMessage();
      return false;
    }
  }

  return false;
}

#ifdef USE_BASS
class BassMidiDevice: public BaseMidiOutDevice {
protected:
  std::vector<unsigned char> message;
  BASS_INFO info;
  HSTREAM stream;		// output stream
  HSOUNDFONT font;	// soundfont
  BASS_MIDI_FONTINFO fontinfo;

public:
  ~BassMidiDevice() override {
    BASS_Free();
	  BASS_PluginFree(0);
  }

  bool Init() override {
    uint16_t dllbassversion = BASS_GetVersion() >> 16;
    message.resize((size_t)3);

    if (HIWORD(BASS_GetVersion()) != BASSVERSION) {
      cerr << "An incorrect version of BASS.DLL was loaded expect: " << BASSVERSION << " got: " << dllbassversion << endl;
      return false;
    }
    if (!BASS_Init(-1, 44100, 0, nullptr, NULL)) {
      cerr << "Can't initialize device";
      return false;
    }

    BASS_GetInfo(&info);
    stream = BASS_MIDI_StreamCreate(17, BASS_MIDI_ASYNC | BASS_SAMPLE_FLOAT, 1); // create the MIDI stream with async processing and 16 MIDI channels for device input + 1 for keyboard input
    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0); // no buffering for minimum latency
    BASS_ChannelSetAttribute(stream, BASS_ATTRIB_MIDI_SRC, 0); // 2 point linear interpolation
    //BASS_ChannelSetSync(stream, BASS_SYNC_MIDI_EVENT | BASS_SYNC_MIXTIME, MIDI_EVENT_PROGRAM, ProgramEventSync, &stream); // catch program/preset changes
    BASS_MIDI_StreamEvent(stream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_GS); // send GS system reset event

    const char * sfpath;
    const char * homepath;
#ifdef _WIN32
    homepath = std::getenv("USERPROFILE");
    sfpath = "E:\\Games\\SoundFont2\\Arachno SoundFont - Version 1.0.sf2";
#else
    homepath = std::getenv("HOME");
    std::string sfpathstr = std::string(homepath) + std::string("/Documents/Arachno SoundFont - Version 1.0.sf2");
    sfpath = sfpathstr.c_str();
#endif

    font = BASS_MIDI_FontInit(sfpath, 0);
    if (font) {
      BASS_MIDI_FONT sf;
      sf.font = font;
      sf.preset = -1;
      sf.bank = 0;
      BASS_MIDI_StreamSetFonts(0, &sf, 1); // set default soundfont
      BASS_MIDI_StreamSetFonts(stream, &sf, 1); // set for current stream too
    } else {
      cerr << "unable to load soundfont: " << sfpath << endl;
      return false;
    }

    BASS_ChannelPlay(stream, 0); // start it

    return true;
  }
  bool HardwareMsgOut ( const MIDITimedBigMessage &msg ) override;
};

bool BassMidiDevice::HardwareMsgOut ( const MIDITimedBigMessage &msg ) {
  char msgbuf[1024];

  if ( msg.IsBeatMarker() )
  {
    fprintf( stdout, "%8ld : %s <------------------>", msg.GetTime(), msg.MsgToText( msgbuf ) );
  }
  else if ( msg.IsChannelEvent() )
  {
    message[0] = msg.GetStatus();
    message[1] = msg.GetByte1();
    message[2] = msg.GetByte2();

    BASS_MIDI_StreamEvents(stream, BASS_MIDI_EVENTS_RAW, message.data(), msg.GetLength());

    fprintf( stdout, "%8ld : %s", msg.GetTime(), msg.MsgToText( msgbuf ) );
  }
  else if ( msg.IsSystemExclusive() )
  {
    std::vector<unsigned char> sysexmessage((size_t)1+msg.GetSysEx()->GetLength());

    sysexmessage[0] = msg.GetStatus();
    memcpy( &sysexmessage[1], msg.GetSysEx()->GetBuf(), msg.GetSysEx()->GetLength() );
    BASS_MIDI_StreamEvents(stream, BASS_MIDI_EVENTS_RAW, sysexmessage.data(), sysexmessage.size());

    fprintf( stdout, "SYSEX length: %d", msg.GetSysEx()->GetLengthSE() );
  }
  else
  {
    fprintf( stdout, "%8ld : %s (Skipped)", msg.GetTime(), msg.MsgToText( msgbuf ) );
  }

  fprintf( stdout, "\n" );
  return true;
}
#endif

#ifdef USE_TSF
void TinySoundFontAudioCallback(void* data, Uint8 *stream, int len);

class TinyMIDIMessage {
public:
  TinyMIDIMessage() {}
  void SetTicks() { ticks = SDL_GetTicks64(); }
  MIDITimedBigMessage msg{};
  Uint64 ticks{};
};

class TinySoundFontDevice: public BaseMidiOutDevice {
protected:
  SDL_AudioSpec OutputAudioSpec;
  tsf* g_TinySoundFont;
  CircularFifo<TinyMIDIMessage, 8> buffer;
  Uint64 startingTicks{};
  Uint64 processingSamples{};

public:
  bool Init() override {
  // Define the desired audio output format we request
    OutputAudioSpec.freq = 44100;
    OutputAudioSpec.format = AUDIO_F32;
    OutputAudioSpec.channels = 2;
    OutputAudioSpec.samples = 4096 >> 2;
    OutputAudioSpec.userdata = this;
    OutputAudioSpec.callback = TinySoundFontAudioCallback;

    // Initialize the audio system
#ifdef WIN32
    //SDL_SetHint(SDL_HINT_AUDIODRIVER, "dsound");
#endif
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
    {
      fprintf(stderr, "Could not initialize audio hardware or driver: %s\n", SDL_GetError());
      return false;
    } else {
      fprintf(stdout, "SDL_GetCurrentAudioDriver = %s\n", SDL_GetCurrentAudioDriver());
    }

    // Load the SoundFont from a file
    const char * def_sfpath;
    const char * homepath;
  #ifdef _WIN32
    homepath = std::getenv("USERPROFILE");
    def_sfpath = "E:\\Games\\SoundFont2\\Arachno SoundFont - Version 1.0.sf2";
  #else
    homepath = std::getenv("HOME");
    std::string sfpathstr = std::string(homepath) + std::string("/Documents/Arachno SoundFont - Version 1.0.sf2");
    def_sfpath = sfpathstr.c_str();
  #endif
    g_TinySoundFont = tsf_load_filename(def_sfpath);
    if (!g_TinySoundFont)
    {
      fprintf(stderr, "Could not load SoundFont: %s\n", def_sfpath);
      return false;
    }

    //Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
    tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);

    // Set the SoundFont rendering output mode
    tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, OutputAudioSpec.freq, 0.0f);

    // Request the desired audio output format
    if (SDL_OpenAudio(&OutputAudioSpec, TSF_NULL) < 0)
    {
      fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
      return 1;
    } else {
      fprintf(stdout, "Audio Buffer = %d\n", OutputAudioSpec.samples);
    }

    // Start the actual audio playback here
    // The audio thread will begin to call our AudioCallback function
    int bufferDurationTicks = OutputAudioSpec.samples * (1000.0 / OutputAudioSpec.freq);
    startingTicks = SDL_GetTicks64(); // - bufferDurationTicks;
    SDL_PauseAudio(0);

    return true;
  }

  bool HardwareMsgOut ( const MIDITimedBigMessage &msg ) override {
    char msgbuf[1024];
    if ( msg.IsBeatMarker() )
    {
      fprintf( stdout, "%8ld : %s <------------------>", msg.GetTime(), msg.MsgToText( msgbuf ) );
    }
    else if ( msg.IsChannelEvent() )
    {
      TinyMIDIMessage tinymsg;
      tinymsg.msg = msg;
      tinymsg.SetTicks();
      if (!buffer.push(tinymsg)) {
        fprintf(stderr, "circular fifo buffer is full\n");
        return false;
      }

      fprintf( stdout, "%8ld : %s", msg.GetTime(), msg.MsgToText( msgbuf ) );
    }
    else if ( msg.IsSystemExclusive() )
    {
      fprintf( stdout, "SYSEX length: %d", msg.GetSysEx()->GetLengthSE() );
    }
    else
    {
      fprintf( stdout, "%8ld : %s (Skipped)", msg.GetTime(), msg.MsgToText( msgbuf ) );
    }

    fprintf( stdout, "\n" );
    return true;
  }

  bool RealHardwareMsgOut ( const MIDITimedBigMessage &msg );

  void AudioCallback(Uint8 *stream, int len) {
    //Number of samples to process
	  int SampleCount  = (len / (2 * sizeof(float))); //2 output channels
    int BlockCount   = (SampleCount / TSF_RENDER_EFFECTSAMPLEBLOCK);
    int SampleMarker = SampleCount - ((BlockCount-1)*TSF_RENDER_EFFECTSAMPLEBLOCK);
    int LastSampleMarker = 0;

    for (;BlockCount;--BlockCount) {
      // NOTE: if startingTicks too close with current event, we may reduce it with bufferDurationTicks
      Uint64 processingMarker = startingTicks + (processingSamples * (1000.0 / 44100.0));

      TinyMIDIMessage tinymsg;
      while (buffer.peek(tinymsg)) {
        if (processingMarker > tinymsg.ticks) {
          RealHardwareMsgOut(tinymsg.msg);
          buffer.skip();
        } else break;
      }

      int SampleBlock = SampleMarker - LastSampleMarker;

      tsf_render_float(g_TinySoundFont, (float*)stream, SampleBlock, 0);

      stream += (SampleBlock * (2 * sizeof(float)));
      processingSamples += SampleBlock;

      LastSampleMarker = SampleMarker;
      SampleMarker += TSF_RENDER_EFFECTSAMPLEBLOCK;
    }
  }
};

void TinySoundFontAudioCallback(void* data, Uint8 *stream, int len)
{
  TinySoundFontDevice *dev = static_cast<TinySoundFontDevice*>(data);
  dev->AudioCallback(stream, len);
}

bool TinySoundFontDevice::RealHardwareMsgOut ( const MIDITimedBigMessage &msg ) {
  if (msg.IsProgramChange()) { //channel program (preset) change (special handling for 10th MIDI channel with drums)
    tsf_channel_set_presetnumber(g_TinySoundFont, msg.GetChannel(), msg.GetPGValue(), (msg.GetChannel() == 9));
  } else if (msg.IsControlChange()) { //MIDI controller messages
    tsf_channel_midi_control(g_TinySoundFont, msg.GetChannel(), msg.GetController(), msg.GetControllerValue());
  } else if (msg.IsNoteOn()) {
    tsf_channel_note_on(g_TinySoundFont, msg.GetChannel(), msg.GetNote(), msg.GetVelocity() / 127.0f);
  } else if (msg.IsNoteOff()) {
    tsf_channel_note_off(g_TinySoundFont, msg.GetChannel(), msg.GetNote());
  } else if (msg.IsPitchBend()) {
    tsf_channel_set_pitchwheel(g_TinySoundFont, msg.GetChannel(), msg.GetBenderValue()+8192);
  }

  else if ( msg.IsSystemExclusive() )
  {
    std::vector<unsigned char> sysexmessage((size_t)1+msg.GetSysEx()->GetLength());
    sysexmessage[0] = msg.GetStatus();
    memcpy( &sysexmessage[1], msg.GetSysEx()->GetBuf(), msg.GetSysEx()->GetLength() );
  }
  return true;
}
#endif

int main( int argc, char **argv )
{
  signal(SIGINT, sigHandler);
#ifdef SIGBREAK
  signal(SIGBREAK, sigHandler); // handles Ctrl-Break on Win32
#endif
  signal(SIGABRT, sigHandler);
  signal(SIGTERM, sigHandler);

  const char *app_name = argv[0];
  if ( argc > 1 )
  {
    const char *infile_name = argv[1];

    MIDIFileReadStreamFile rs( infile_name );
    MIDIMultiTrack tracks;
    MIDIFileReadMultiTrack track_loader( &tracks );
    MIDIFileRead reader( &rs, &track_loader );

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

    std::unique_ptr<BaseMidiOutDevice> dev{};
    if ( argc > 2 )
    {
      cout << endl;
      int mode = atoi( argv[2] );
      switch ( mode )
      {
      case 1:
        dev = std::make_unique<BaseMidiOutDevice>();
        break;
      case 2:
        dev = std::make_unique<RtMidiDevice>();
        break;
#ifdef USE_BASS
      case 3:
        dev = std::make_unique<BassMidiDevice>();
        break;
#endif
#ifdef USE_TSF
      case 4:
        dev = std::make_unique<TinySoundFontDevice>();
        break;
#endif
      }

    }

    if (dev) {
      PlaySequencer( &seq, dev.get());
    } else {
      DumpMIDIMultiTrack( &tracks );
    }

    //      cout << MultiTrackAsText( tracks ); // new util fun

    double dt = seq.GetMisicDurationInSeconds();

    cout << "\nMusic duration = " << dt << endl;
  }
  else
  {
    cerr << "\nusage:\n    " << app_name << " FILE.mid [switch]\n";
    cerr << "\tswitch:\t[0 for DumpMIDIMultiTrack]\n";
    cerr << "\t\t[1 for PlayDumpSequencer]\n";
    cerr << "\t\t[2 for PlayRtMidiSequencer]\n";
#ifdef USE_BASS
    cerr << "\t\t[3 for PlayBassMidiSequencer]\n";
#endif
#ifdef USE_TSF
    cerr << "\t\t[4 for PlayTinySoundFontMidiSequencer]\n";
#endif
    return -1;
  }

  return 0;
}

RtMidi::Api chooseMidiApi()
{
  std::vector< RtMidi::Api > apis;
  RtMidi::getCompiledApi(apis);

  std::cout << "\nAPIs\n  API #0: unspecified / default\n";
  for (size_t n = 0; n < apis.size(); n++)
    std::cout << "  API #" << apis[n] << ": " << RtMidi::getApiDisplayName(apis[n]) << "\n";

  if (apis.size() <= 1)
    return RtMidi::Api::UNSPECIFIED;

  std::cout << "\nChoose an API number: ";
  unsigned int i;
  std::cin >> i;

  std::string dummy;
  std::getline(std::cin, dummy);  // used to clear out stdin

  return static_cast<RtMidi::Api>(i);
}

bool chooseMidiPort( RtMidiOut *rtmidi )
{
  std::cout << "\nWould you like to open a virtual output port? [y/N] ";

  std::string keyHit;
  std::getline( std::cin, keyHit );
  if ( keyHit == "y" ) {
    rtmidi->openVirtualPort();
    return true;
  }

  std::string portName;
  unsigned int i = 0, nPorts = rtmidi->getPortCount();
  if ( nPorts == 0 ) {
    std::cout << "No output ports available!" << std::endl;
    return false;
  }

  if ( nPorts == 1 ) {
    std::cout << "\nOpening " << rtmidi->getPortName() << std::endl;
  }
  else {
    for ( i=0; i<nPorts; i++ ) {
      portName = rtmidi->getPortName(i);
      std::cout << "  Output port #" << i << ": " << portName << '\n';
    }

    do {
      std::cout << "\nChoose a port number: ";
      std::cin >> i;
    } while ( i >= nPorts );
  }

  std::cout << "\n";
  rtmidi->openPort( i );

  return true;
}
