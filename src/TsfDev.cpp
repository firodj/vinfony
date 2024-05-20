#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"
#include "TsfDev.hpp"

#include <SDL.h>
#include <tsf.h>

#include <iostream>
#include <thread>
#include <chrono>

#include "circularfifo1.h"

namespace vinfony
{

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

  void TinySoundFontAudioCallback(void* data, Uint8 *stream, int len);

  class TinyMIDIMessage {
  public:
    TinyMIDIMessage() {}
    void SetTicks() { ticks = SDL_GetTicks64(); }
    jdksmidi::MIDITimedBigMessage msg{};
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
        return false;
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

    void Shutdown() override {
      SDL_PauseAudio(1);
      SDL_CloseAudio();
    }

    bool HardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg ) override {
      if ( msg.IsChannelEvent() )
      {
        TinyMIDIMessage tinymsg;
        tinymsg.msg = msg;
        tinymsg.SetTicks();
        if (!buffer.push(tinymsg)) {
          // retry for 1 second
          fprintf(stderr, "WARNING: circular fifo buffer is full, retrying...\n");
          for ( int i = 1; i < 1000; i++ ) {
          if ( !buffer.isFull() ) {
              if (!buffer.push(tinymsg)) {
                fprintf(stderr, "circular fifo buffer is full\n");
                return false;
              }
              break;
            }
            SDL_Delay( 1 );
          }
        }
      }
      return true;
    }

    bool RealHardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg );

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
          if (processingMarker >= tinymsg.ticks) {
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

  bool TinySoundFontDevice::RealHardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg ) {
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
    } else if (msg.IsAllNotesOff()) {
      tsf_channel_note_off_all(g_TinySoundFont, msg.GetChannel());
    }

    else if ( msg.IsSystemExclusive() )
    {
      std::vector<unsigned char> sysexmessage((size_t)1+msg.GetSysEx()->GetLength());
      sysexmessage[0] = msg.GetStatus();
      memcpy( &sysexmessage[1], msg.GetSysEx()->GetBuf(), msg.GetSysEx()->GetLength() );
    }
    return true;
  }

  std::unique_ptr<BaseMidiOutDevice> CreateTsfDev() {
    return std::make_unique<TinySoundFontDevice>();
  }
}