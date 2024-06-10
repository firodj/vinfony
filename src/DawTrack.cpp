#include "DawTrack.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
using namespace std::chrono_literals;

#include <map>
#include <memory>
#include <thread>

#include <SDL.h>
#include <imgui.h>

#include "jdksmidi/world.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"

#include <fmt/core.h>
#include "DawSeq.hpp"
#include "TsfDev.hpp"

namespace vinfony {

DawTrack::DawTrack() {}

DawTrack::~DawTrack() {}

void DawTrack::SetBank(const jdksmidi::MIDIBigMessage * msg) {
  auto control_value = msg->GetControllerValue();
  switch (msg->GetController()) {
    case jdksmidi::C_GM_BANK:
      bank = (0x8000 | control_value);
      break;
    case jdksmidi::C_GM_BANK_LSB:
      bank = (
        (bank & 0x8000 ? ((bank & 0x7F) << 7) : 0) | control_value);
      break;
  }
}

void DawTrack::SetVolume(const jdksmidi::MIDIBigMessage * msg) {
  auto control_value = msg->GetControllerValue();
  switch (msg->GetController()) {
    case jdksmidi::C_MAIN_VOLUME:
      midiVolume = (unsigned short)((midiVolume & 0x7F  ) | (control_value << 7));
      break;
    case jdksmidi::C_MAIN_VOLUME_LSB:
		  midiVolume = (unsigned short)((midiVolume & 0x3F80) | control_value);
      break;
    default:
      return;
  }
}

void DawTrack::SetPan(const jdksmidi::MIDIBigMessage * msg) {
  auto control_value = msg->GetControllerValue();
  switch (msg->GetController()) {
    case jdksmidi::C_PAN:
      midiPan = (unsigned short)((midiPan & 0x7F  ) | (control_value << 7));
      break;
    case jdksmidi::C_PAN_LSB:
		  midiPan = (unsigned short)((midiPan & 0x3F80) | control_value);
      break;
    default:
      return;
  }
}

void DawTrack::SetFilter(const jdksmidi::MIDIBigMessage * msg) {
  auto control_value = msg->GetControllerValue();
  switch (msg->GetController()) {
    case jdksmidi::C_HARMONIC:
      midiFilterQ = (unsigned short)control_value;
      break;
    case jdksmidi::C_BRIGHTNESS:
		  midiFilterFc = (unsigned short)control_value;
      break;
    default:
      return;
  }
}

static const char * g_stdProgramNames[128] = {
  "Acoustic Grand Piano",
  "Bright Acoustic Piano",
  "Electric Grand Piano",
  "Honky-Tonk Piano",
  "Electric Piano 1",
  "Electric Piano 2",
  "Harpsichord",
  "Clavinet",
  "Celesta",
  "Glockenspiel",
  "Music Box",
  "Vibraphone",
  "Marimba",
  "Xylophone",
  "Tubular Bells",
  "Dulcimer",
  "Drawbar Organ",
  "Percussive Organ",
  "Rock Organ",
  "Church Organ",
  "Reed Organ",
  "Accordion",
  "Harmonica",
  "Tango Accordion",
  "Acoustic Guitar (nylon)",
  "Acoustic Guitar (steel)",
  "Electric Guitar (jazz)",
  "Electric Guitar (clean)",
  "Electric Guitar (muted)",
  "Overdriven Guitar",
  "Distortion Guitar",
  "Guitar Harmonics",
  "Acoustic Bass",
  "Electric Bass (finger)",
  "Electric Bass (pick)",
  "Fretless Bass",
  "Slap Bass 1",
  "Slap Bass 2",
  "Synth Bass 1",
  "Synth Bass 2",
  "Violin",
  "Viola",
  "Cello",
  "Contrabass",
  "Tremolo Strings",
  "Pizzicato Strings",
  "Orchestral Strings",
  "Timpani",
  "String Ensemble 1",
  "String Ensemble 2",
  "Synth Strings 1",
  "Synth Strings 2",
  "Choir Aahs",
  "Voice Oohs",
  "Synth Voice",
  "Orchestra Hit",
  "Trumpet",
  "Trombone",
  "Tuba",
  "Muted Trumpet",
  "French Horn",
  "Brass Section",
  "Synth Brass 1",
  "Synth Brass 2",
  "Soprano Sax",
  "Alto Sax",
  "Tenor Sax",
  "Baritone Sax",
  "Oboe",
  "English Horn",
  "Bassoon",
  "Clarinet",
  "Piccolo",
  "Flute",
  "Recorder",
  "Pan Flute",
  "Blown Bottle",
  "Skakuhachi",
  "Whistle",
  "Ocarina",
  "Lead 1 (square)",
  "Lead 2 (sawtooth)",
  "Lead 3 (calliope)",
  "Lead 4 (chiff)",
  "Lead 5 (charang)",
  "Lead 6 (voice)",
  "Lead 7 (fifths)",
  "Lead 8 (bass+lead)",
  "Pad 1 (new age)",
  "Pad 2 (warm)",
  "Pad 3 (polysynth)",
  "Pad 4 (choir)",
  "Pad 5 (bowed)",
  "Pad 6 (metallic)",
  "Pad 7 (halo)",
  "Pad 8 (sweep)",
  "FX 1 (rain)",
  "FX 2 (soundtrack)",
  "FX 3 (crystal)",
  "FX 4 (atmosphere)",
  "FX 5 (brightness)",
  "FX 6 (goblins)",
  "FX 7 (echoes)",
  "FX 8 (sci-fi)",
  "Sitar",
  "Banjo",
  "Shamisen",
  "Koto",
  "Kalimba",
  "Bagpipe",
  "Fiddle",
  "Shanai",
  "Tinkle Bell",
  "Agogo",
  "Steel Drums",
  "Woodblock",
  "Taiko Drum",
  "Melodic Tom",
  "Synth Drum",
  "Reverse Cymbal",
  "Guitar Fret Noise",
  "Breath Noise",
  "Seashore",
  "Bird Tweet",
  "Telephone Ring",
  "Helicopter",
  "Applause",
  "Gunshot",
};

/**
 * GetStdProgramName with pg is 1 based.
 */
const char * GetStdProgramName(int pg) {
  if (pg < 1 || pg > 128) return nullptr;
  return g_stdProgramNames[pg - 1];
}

/**
 * GetStdDrumName with pg is 1 based.
 */
const char * GetStdDrumName(int pg) {
  if (pg < 1 || pg > 128) return nullptr;
  switch (pg) {
    case 1: return "Standard Kit";
    case 9: return "Room Kit";
    case 17: return "Power Kit";
    case 25: return "Electronic Kit";
    case 26: return "TR-808/Analog Kit";
    case 33: return "Jazz Kit";
    case 41: return "Brush Kit";
    case 49: return "Orchestra Kit";
    case 57: return "Sound FX Kit";
    case 128: return "CM-64/CM-32L";
  }
  return nullptr;
}

int DawTrack::GetGetDrumPart() {
  if (m_seq) return m_seq->GetAudioDevice()->GetDrumPart(ch ? (ch-1) & 0xF : 0);
  return 0;
}

}