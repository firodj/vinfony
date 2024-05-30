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
#include <array>
using namespace std::chrono_literals;

#include <fmt/core.h>
#include <fmt/color.h>

#include "circularfifo1.h"
#include "DawSysEx.hpp"

#define SAMPLE_RATE 44100.0

#define USE_SDLTICK 1

namespace vinfony
{

void TinySoundFontAudioCallback(void* data, uint8_t *stream, int len);

class TinyMIDIMessage {
public:
  TinyMIDIMessage() {}
  jdksmidi::MIDITimedBigMessage msg{};

#if USE_SDLTICK == 1
  void SetTicks() { ticks = SDL_GetTicks64(); }
  Uint64 ticks{};
#else
  std::chrono::time_point<std::chrono::steady_clock> ticks{};
  void SetTicks() { ticks = std::chrono::high_resolution_clock::now(); };
#endif
};

struct TinySoundFontDevice::Impl {
  SDL_AudioSpec OutputAudioSpec;
  tsf* g_TinySoundFont;
  CircularFifo<TinyMIDIMessage, 8> buffer{};
#if USE_SDLTICK == 1
  Uint64 startingTicks{};
#else
  std::chrono::time_point<std::chrono::steady_clock> startingTicks{};
#endif
  Uint64 processingSamples{};
  std::string m_soundfontPath;
  std::array<unsigned char, 16> m_midiDrumParts;
#if USE_SDLTICK == 1
  Uint64 m_midiTicks{};
#else
   std::chrono::time_point<std::chrono::steady_clock> m_midiTicks{};
#endif
  TsfAudioCallback callback;
};

TinySoundFontDevice::TinySoundFontDevice(std::string soundfontPath) {
  m_impl = std::make_unique<Impl>();
  m_impl->m_soundfontPath = soundfontPath;
  m_impl->callback = nullptr;
}

TinySoundFontDevice::~TinySoundFontDevice() {
}

void TinySoundFontDevice::SetAudioCallback(TsfAudioCallback cb)
{
  m_impl->callback = cb;
}

void TinySoundFontDevice::UpdateMIDITicks()  {
#if USE_SDLTICK == 1
  m_impl->m_midiTicks = SDL_GetTicks64();
#else
  m_impl->m_midiTicks = std::chrono::high_resolution_clock::now();
#endif
};

bool TinySoundFontDevice::Init()  {
  // Define the desired audio output format we request
  m_impl->OutputAudioSpec.freq = SAMPLE_RATE;
  m_impl->OutputAudioSpec.format = AUDIO_F32;
  m_impl->OutputAudioSpec.channels = 2;
  m_impl->OutputAudioSpec.samples = 4096 >> 2;
  m_impl->OutputAudioSpec.userdata = this;
  m_impl->OutputAudioSpec.callback = TinySoundFontAudioCallback;

  // Load the SoundFont from a file
  m_impl->g_TinySoundFont = tsf_load_filename(m_impl->m_soundfontPath.c_str());
  if (!m_impl->g_TinySoundFont)
  {
    fprintf(stderr, "Could not load SoundFont: %s\n", m_impl->m_soundfontPath.c_str());
    return false;
  }

  Reset();

  // Request the desired audio output format
  if (SDL_OpenAudio(&m_impl->OutputAudioSpec, TSF_NULL) < 0)
  {
    fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
    return false;
  } else {
    fprintf(stdout, "Audio Buffer = %d\n", m_impl->OutputAudioSpec.samples);
  }

  // Start the actual audio playback here
  // The audio thread will begin to call our AudioCallback function
  int bufferDurationTicks = m_impl->OutputAudioSpec.samples * (1000.0 / m_impl->OutputAudioSpec.freq);
#if USE_SDLTICK == 1
  m_impl->startingTicks = SDL_GetTicks64(); // - bufferDurationTicks;
#else
  m_impl->startingTicks = std::chrono::high_resolution_clock::now();
#endif
  SDL_PauseAudio(0);

  return true;
}

void TinySoundFontDevice::Shutdown()  {
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}

bool TinySoundFontDevice::HardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg, double * msgTimeShiftMs )  {
  if ( msg.IsChannelEvent() || msg.IsSystemExclusive())
  {
    TinyMIDIMessage tinymsg;
    tinymsg.msg = msg;
    if (!msgTimeShiftMs) {
      tinymsg.SetTicks();
    } else {
#if USE_SDLTICK == 1
      tinymsg.ticks = m_impl->m_midiTicks + *msgTimeShiftMs;
#else
      tinymsg.ticks = m_impl->m_midiTicks + std::chrono::milliseconds((long)*msgTimeShiftMs);
#endif
      ;
    }
    if ( !m_impl->buffer.push(tinymsg) ) {
      // retry for 1 second
      fprintf(stderr, "WARNING: circular fifo buffer is full, retrying...\n");
      for ( int i = 1; i < 1000; i++ ) {
      if ( !m_impl->buffer.isFull() ) {
          if (!m_impl->buffer.push(tinymsg)) {
            fprintf(stderr, "circular fifo buffer is full\n");
            return false;
          }
          break;
        }
#if USE_SDLTICK == 1
        SDL_Delay( 1 );
#else
        std::this_thread::sleep_for(1s);
#endif
      }
    }
  }
  return true;
}

void TinySoundFontDevice::StdAudioCallback(uint8_t *stream, int len) {
  if (m_impl->callback) return m_impl->callback(stream, len);

  //Number of samples to process
  int SampleCount  = (len / (2 * sizeof(float))); //2 output channels
  int BlockCount   = (SampleCount / TSF_RENDER_EFFECTSAMPLEBLOCK);
  int SampleMarker = SampleCount - ((BlockCount-1)*TSF_RENDER_EFFECTSAMPLEBLOCK);
  int LastSampleMarker = 0;

  for (;BlockCount;--BlockCount) {
    // NOTE: if startingTicks too close with current event, we may reduce it with bufferDurationTicks
#if USE_SDLTICK == 1
    Uint64 processingMarker = m_impl->startingTicks + (m_impl->processingSamples * (1000.0 / SAMPLE_RATE));
#else
    std::chrono::time_point<std::chrono::steady_clock> processingMarker = m_impl->startingTicks + std::chrono::milliseconds((long)(m_impl->processingSamples * (1000.0 / SAMPLE_RATE)));
#endif

    TinyMIDIMessage tinymsg;
    while (m_impl->buffer.peek(tinymsg)) {
      if (processingMarker >= tinymsg.ticks) {
        RealHardwareMsgOut(tinymsg.msg);
        m_impl->buffer.skip();
      } else break;
    }

    int SampleBlock = SampleMarker - LastSampleMarker;

    tsf_render_float(m_impl->g_TinySoundFont, (float*)stream, SampleBlock, 0);

    stream += (SampleBlock * (2 * sizeof(float)));
    m_impl->processingSamples += SampleBlock;

    LastSampleMarker = SampleMarker;
    SampleMarker += TSF_RENDER_EFFECTSAMPLEBLOCK;
  }
}

/**
 * RenderStereoFloat, call this inside Audio Callback thread,
 */
void TinySoundFontDevice::RenderStereoFloat(float* stream, int samples) {
  tsf_render_float(m_impl->g_TinySoundFont, stream, samples, /* mixing */ 0);
}

/**
 * FlushToRealMsgOut, only call inside Audio Callback thread,
 */
void TinySoundFontDevice::FlushToRealMsgOut() {
  TinyMIDIMessage tinymsg;
  while (m_impl->buffer.pop(tinymsg)) {
    RealHardwareMsgOut(tinymsg.msg);
  }
}

void TinySoundFontDevice::Reset() {
  tsf_reset(m_impl->g_TinySoundFont);

  // Set the SoundFont rendering output mode
  tsf_set_output(m_impl->g_TinySoundFont, TSF_STEREO_INTERLEAVED, m_impl->OutputAudioSpec.freq, 0.0f);

  // Set Midi Drums
  for (auto &d : m_impl->m_midiDrumParts) d = 0;
  m_impl->m_midiDrumParts[9] = 1;

  //Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
  tsf_channel_set_bank_preset(m_impl->g_TinySoundFont, 9, 128, 0);
  tsf_channel_set_bank_preset(m_impl->g_TinySoundFont, 10, 128, 0);
};

int TinySoundFontDevice::GetAudioSampleRate() {
  return m_impl->OutputAudioSpec.freq;
}

void TinySoundFontAudioCallback(void* data, uint8_t *stream, int len)
{
  TinySoundFontDevice *dev = static_cast<TinySoundFontDevice*>(data);
  dev->StdAudioCallback(stream, len);
}

/**
 * RealHardwareMsgOut, only call inside Audio Callback thread,
 */
bool TinySoundFontDevice::RealHardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg ) {
  if (msg.IsProgramChange()) { //channel program (preset) change (special handling for 10th MIDI channel with drums)
    int drumgrp = m_impl->m_midiDrumParts[msg.GetChannel()];
    tsf_channel_set_presetnumber(m_impl->g_TinySoundFont, msg.GetChannel(), msg.GetPGValue(), drumgrp != 0);
  } else if (msg.IsControlChange()) { //MIDI controller messages
    tsf_channel_midi_control(m_impl->g_TinySoundFont, msg.GetChannel(), msg.GetController(), msg.GetControllerValue());
  } else if (msg.IsNoteOn()) {
    tsf_channel_note_on(m_impl->g_TinySoundFont, msg.GetChannel(), msg.GetNote(), msg.GetVelocity() / 127.0f);
  } else if (msg.IsNoteOff()) {
    tsf_channel_note_off(m_impl->g_TinySoundFont, msg.GetChannel(), msg.GetNote());
  } else if (msg.IsPitchBend()) {
    tsf_channel_set_pitchwheel(m_impl->g_TinySoundFont, msg.GetChannel(), msg.GetBenderValue()+8192);
  } else if (msg.IsAllNotesOff()) {
    tsf_channel_note_off_all(m_impl->g_TinySoundFont, msg.GetChannel());
  }

  else if ( msg.IsSystemExclusive() )
  {
    {
      std::unique_ptr<GMSysEx> gmsyx(GMSysEx::Create(msg.GetSysEx()));
      if (gmsyx)
        fmt::print(fmt::fg(fmt::color::wheat), "GM: {}\n", gmsyx->Info());
    }

    {
      std::unique_ptr<GSSysEx> gssyx(GSSysEx::Create(msg.GetSysEx()));
      if (gssyx) {
        fmt::print(fmt::fg(fmt::color::wheat), "GS: {}\n", gssyx->Info());
        if (gssyx->IsUseRhythmPart()) {
          int channel = gssyx->GetPart()-1;
          int drumgrp = gssyx->GetUseRhythmPart();
          int preset_number = tsf_channel_get_preset_number(m_impl->g_TinySoundFont, channel);
          m_impl->m_midiDrumParts[channel] = drumgrp;
          tsf_channel_set_bank(m_impl->g_TinySoundFont, channel, drumgrp == 0 ? 0 : 128);
          tsf_channel_set_presetnumber(m_impl->g_TinySoundFont, channel, preset_number, drumgrp != 0);
        }
      }
    }
  }
  return true;
}

}