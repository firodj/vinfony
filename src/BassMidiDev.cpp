#include "BassMidiDev.hpp"
#include "bass.h"
#include "bassmidi.h"
#include <fmt/core.h>
#include <fmt/color.h>
#include <string>
#include "DawSysEx.hpp"

namespace vinfony {

struct BassMidiDevice::Impl {
  std::vector<unsigned char> message;
  BASS_INFO info;
  HSTREAM stream;		// output stream
  HSOUNDFONT font;	// soundfont
  BASS_MIDI_FONTINFO fontinfo;
  std::string m_soundfontPath;
  int SampleRate;
};

BassMidiDevice::BassMidiDevice(std::string soundfontPath) {
  m_impl = std::make_unique<Impl>();
  m_impl->m_soundfontPath = soundfontPath;
  m_impl->SampleRate = 44100;
};

BassMidiDevice::~BassMidiDevice() {
  BASS_Free();
	BASS_PluginFree(0);
};

bool BassMidiDevice::Init() {
  uint16_t dllbassversion = BASS_GetVersion() >> 16;
  m_impl->message.resize((size_t)3);

  if (HIWORD(BASS_GetVersion()) != BASSVERSION) {
    fmt::println("An incorrect version of BASS.DLL was loaded expect: {} got: {}", BASSVERSION,  dllbassversion);
    return false;
  }
  if (!BASS_Init(-1, m_impl->SampleRate, 0, nullptr, NULL)) {
    fmt::println("Can't initialize device");
    return false;
  }

  BASS_GetInfo(&m_impl->info);
  m_impl->stream = BASS_MIDI_StreamCreate(17, BASS_MIDI_ASYNC | BASS_SAMPLE_FLOAT, 1); // create the MIDI stream with async processing and 16 MIDI channels for device input + 1 for keyboard input
  BASS_ChannelSetAttribute(m_impl->stream, BASS_ATTRIB_BUFFER, 0); // no buffering for minimum latency
  BASS_ChannelSetAttribute(m_impl->stream, BASS_ATTRIB_MIDI_SRC, 0); // 2 point linear interpolation
  //BASS_ChannelSetSync(stream, BASS_SYNC_MIDI_EVENT | BASS_SYNC_MIXTIME, MIDI_EVENT_PROGRAM, ProgramEventSync, &stream); // catch program/preset changes
  BASS_MIDI_StreamEvent(m_impl->stream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_DEFAULT); // send GS system reset event

  m_impl->font = BASS_MIDI_FontInit(m_impl->m_soundfontPath.c_str(), 0);
  if (m_impl->font) {
    BASS_MIDI_FONT sf;
    sf.font = m_impl->font;
    sf.preset = -1;
    sf.bank = 0;
    BASS_MIDI_StreamSetFonts(0, &sf, 1); // set default soundfont
    BASS_MIDI_StreamSetFonts(m_impl->stream, &sf, 1); // set for current stream too
  } else {
    fmt::println("Unable to load soundfont: {}", m_impl->m_soundfontPath);
    return false;
  }

  BASS_ChannelPlay(m_impl->stream, 0); // start it

  return true;
};
bool BassMidiDevice::HardwareMsgOut( const jdksmidi::MIDITimedBigMessage &msg, double * msgTimeShiftMs ) {
  if ( msg.IsChannelEvent() )
  {
    m_impl->message[0] = msg.GetStatus();
    m_impl->message[1] = msg.GetByte1();
    m_impl->message[2] = msg.GetByte2();

    if ( msg.IsPitchBend() ) {
      // fmt::println("Time:{} CH{}, Bend {}", msg.GetTime(), msg.GetChannel()+1, msg.GetBenderValue());
    } else if (msg.IsControlChange() ) {
      //
    }

    BASS_MIDI_StreamEvents(m_impl->stream, BASS_MIDI_EVENTS_RAW, m_impl->message.data(), msg.GetLength());
  }
  else if ( msg.IsSystemExclusive() )
  {
    fmt::print("Time:{} ", msg.GetTime());
    fmt::print(fmt::fg(fmt::color::aqua), "SYSEX CH{}, Data {} =", msg.GetChannel()+1, msg.GetSysEx()->GetLength());

    unsigned char * buf = (unsigned char *)msg.GetSysEx()->GetBuf();

    std::string description;

    fmt::print(fmt::fg(fmt::color::yellow_green), " {:02X}", msg.GetStatus());
    for (int i=0; i<msg.GetSysEx()->GetLength(); i++, buf++) {
      fmt::print(fmt::fg(fmt::color::aqua), " {:02X}", *buf);
    }

    {
      std::unique_ptr<vinfony::BaseSysEx> gmsysex(vinfony::BaseSysEx::Create(msg.GetSysEx()));
      if (gmsysex) description += gmsysex->Info();
    }

    fmt::print("\n=> {}\n", description);

    std::vector<unsigned char> sysexmessage((size_t)1+msg.GetSysEx()->GetLength());

    sysexmessage[0] = msg.GetStatus();
    memcpy( &sysexmessage[1], msg.GetSysEx()->GetBuf(), msg.GetSysEx()->GetLength() );

    BASS_MIDI_StreamEvents(m_impl->stream, BASS_MIDI_EVENTS_RAW, sysexmessage.data(), sysexmessage.size());
  }
  return true;
}
void BassMidiDevice::Reset() {
  BASS_MIDI_StreamEvent(m_impl->stream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_DEFAULT);
  BASS_MIDI_StreamEvent(m_impl->stream, 0, MIDI_EVENT_RESET, 0);
  BASS_MIDI_StreamEvent(m_impl->stream, 0, MIDI_EVENT_NOTESOFF, 0);
};

void BassMidiDevice::UpdateMIDITicks() {};
int BassMidiDevice::GetAudioSampleRate() { return m_impl->SampleRate; }

const char * BassMidiDevice::GetName() { return "BASSMidi"; }

}
