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
  int SampleRate;
};

TinySoundFontDevice::TinySoundFontDevice(std::string soundfontPath) {
  m_impl = std::make_unique<Impl>();
  m_impl->m_soundfontPath = soundfontPath;
  m_impl->SampleRate = 44100;
}

TinySoundFontDevice::~TinySoundFontDevice() {
  if (m_impl->g_TinySoundFont)
    tsf_close(m_impl->g_TinySoundFont);

  SDL_PauseAudio(1);
  SDL_CloseAudio();
}

void TinySoundFontDevice::SetSampleRate(int sampleRate) { m_impl->SampleRate = sampleRate; }
void TinySoundFontDevice::UpdateMIDITicks()  {
#if USE_SDLTICK == 1
  m_impl->m_midiTicks = SDL_GetTicks64();
#else
  m_impl->m_midiTicks = std::chrono::high_resolution_clock::now();
#endif
};

bool TinySoundFontDevice::Init() {
  // Initialize lookup tables;
  tsf_init_lut();

  // Load the SoundFont from a file
  m_impl->g_TinySoundFont = tsf_load_filename(m_impl->m_soundfontPath.c_str());
  if (!m_impl->g_TinySoundFont)
  {
    fprintf(stderr, "Could not load SoundFont: %s\n", m_impl->m_soundfontPath.c_str());
  }

  Reset();

  // Start the actual audio playback here
  // The audio thread will begin to call our AudioCallback function
  //int bufferDurationTicks = m_impl->OutputAudioSpec.samples * (1000.0 / m_impl->OutputAudioSpec.freq);
#if USE_SDLTICK == 1
  m_impl->startingTicks = SDL_GetTicks64(); // - bufferDurationTicks;
#else
  m_impl->startingTicks = std::chrono::high_resolution_clock::now();
#endif

  return true;
}

bool TinySoundFontDevice::HardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg, double * msgTimeShiftMs )  {
  if ( msg.IsChannelEvent() || msg.IsSystemExclusive())
  {
    TinyMIDIMessage tinymsg;
    tinymsg.msg = msg;
    if (!msgTimeShiftMs) {  // immediate
      tinymsg.SetTicks(); // maybe not used ?
      RealHardwareMsgOut(tinymsg.msg);
      return true;
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

void TinySoundFontDevice::Advance(int sampleCount) {
  m_impl->processingSamples += sampleCount;

#if USE_SDLTICK == 1
  Uint64 processingMarker = m_impl->processingSamples * (1000.0 / m_impl->SampleRate);
#else
  std::chrono::time_point<std::chrono::steady_clock> processingMarker = std::chrono::milliseconds((long)(m_impl->processingSamples * (1000.0 / m_impl->SampleRate)));
#endif

  TinyMIDIMessage tinymsg;
  while (m_impl->buffer.peek(tinymsg)) {
    if (processingMarker >= tinymsg.ticks) {
      RealHardwareMsgOut(tinymsg.msg);
      m_impl->buffer.skip();
    } else break;
  }
}

void TinySoundFontDevice::StdAudioCallback(uint8_t *stream, int len) {
  //Number of samples to process
  int SampleCount  = (len / (2 * sizeof(float))); //2 output channels
  int BlockCount   = (SampleCount / TSF_RENDER_EFFECTSAMPLEBLOCK);
  int SampleMarker = SampleCount - ((BlockCount-1)*TSF_RENDER_EFFECTSAMPLEBLOCK);
  int LastSampleMarker = 0;

  for (;BlockCount;--BlockCount) {
    int SampleBlock = SampleMarker - LastSampleMarker;
    LastSampleMarker = SampleMarker;
    SampleMarker += TSF_RENDER_EFFECTSAMPLEBLOCK;

    Advance(SampleBlock);
    RenderStereoFloat((float *)stream, SampleBlock);
    stream += (SampleBlock * (2 * sizeof(float)));
  }
}

/**
 * RenderStereoFloat, call this inside Audio Callback thread,
 */
void TinySoundFontDevice::RenderStereoFloat(float* stream, int samples) {
  FlushToRealMsgOut(); //if any
  if (m_impl->g_TinySoundFont)
    tsf_render_float(m_impl->g_TinySoundFont, stream, samples, /* mixing */ 0);
  else
    memset(stream, 0, samples);
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

void AudioCallback(void* data, uint8_t *stream, int len)
{
  TinySoundFontDevice *tsfdev = static_cast<TinySoundFontDevice*>(data);
  tsfdev->StdAudioCallback(stream, len);
}


void TinySoundFontDevice::Reset() {
  if (!m_impl->g_TinySoundFont) return;

  tsf_reset(m_impl->g_TinySoundFont);

  // Set the SoundFont rendering output mode
  tsf_set_output(m_impl->g_TinySoundFont, TSF_STEREO_INTERLEAVED, m_impl->SampleRate, 0.0f);

  // Set Midi Drums
  for (auto &d : m_impl->m_midiDrumParts) d = 0;
  m_impl->m_midiDrumParts[9] = 1;

  //Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
  tsf_channel_set_bank_preset(m_impl->g_TinySoundFont, 9, 128, 0);

  m_impl->processingSamples = 0;

  // Define the desired audio output format we request
  m_impl->OutputAudioSpec.freq = m_impl->SampleRate;
  m_impl->OutputAudioSpec.format = AUDIO_F32;
  m_impl->OutputAudioSpec.channels = 2;
  m_impl->OutputAudioSpec.samples = 4096 >> 2;
  m_impl->OutputAudioSpec.userdata = this;

  // AudioCallback needed for TSF, otherwise BASSMIDI null
  m_impl->OutputAudioSpec.callback = AudioCallback;

  // Request the desired audio output format
  if (SDL_OpenAudio(&m_impl->OutputAudioSpec, nullptr) < 0)
  {
    fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
  } else {
    fprintf(stdout, "Audio Buffer = %d\n", m_impl->OutputAudioSpec.samples);
  }

  SDL_PauseAudio(0);
};

int TinySoundFontDevice::GetAudioSampleRate() {
  return m_impl->SampleRate;
}

int TinySoundFontDevice::GetDrumPart(int ch) {
  return m_impl->m_midiDrumParts[ch & 0xF];
}

/**
 * RealHardwareMsgOut, only call inside Audio Callback thread,
 */
bool TinySoundFontDevice::RealHardwareMsgOut ( const jdksmidi::MIDITimedBigMessage &msg ) {
  if (!m_impl->g_TinySoundFont) return false;

  if (msg.IsProgramChange()) { //channel program (preset) change (special handling for 10th MIDI channel with drums)
    int drumgrp = m_impl->m_midiDrumParts[msg.GetChannel()];
    int flag;
    int bank = tsf_channel_get_preset_bank(m_impl->g_TinySoundFont, msg.GetChannel(), &flag);
    if ((bank == 0x3F80 || (bank == 0x7F && flag)) && !drumgrp) { m_impl->m_midiDrumParts[msg.GetChannel()] = drumgrp = 1; }
    fmt::println("DEBUG: MIDI Msg CHANNEL: {} BANK: {:04X}h PG:{} DRUMGRP:{}", msg.GetChannel()+1,
                    bank, msg.GetPGValue()+1, drumgrp);
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
      std::unique_ptr<BaseSysEx> gmsyx(BaseSysEx::Create(msg.GetSysEx()));
      if (gmsyx && gmsyx->m_sysxVendor == SYSX_GM)
        fmt::print(fmt::fg(fmt::color::wheat), "GM: {}\n", gmsyx->Info());
      if  (gmsyx && gmsyx->m_sysxVendor == SYSX_GS) {
        GSSysEx * gssyx = (GSSysEx*)gmsyx.get();
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
  }
  return true;
}

tsf * TinySoundFontDevice::GetTSF() { return m_impl->g_TinySoundFont; }

const char * TinySoundFontDevice::GetName() { return "TinySoundFont"; }

}
