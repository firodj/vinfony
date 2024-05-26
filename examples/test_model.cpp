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

namespace vinfony {
  class GMSysEx {
  public:
    GMSysEx(const  jdksmidi::MIDISystemExclusive *midi_sysex): m_midi_sysex(midi_sysex) {
      Parse();
    };

    static GMSysEx * Create(const jdksmidi::MIDISystemExclusive *midi_sysex) {
      if (midi_sysex->GetLength() < 5) return nullptr;
      if (midi_sysex->GetData(0) == 0x7E)
        return new GMSysEx(midi_sysex);
      return nullptr;
    }

    bool IsGMReset() {
      return (m_devID == 0x7F &&
        m_subID1 == 0x09 &&
        m_subID2 == 0x01 &&
        m_eoxAt == 5);
    }

    void Parse() {
      m_devID  = m_midi_sysex->GetData(1);
      m_subID1 = m_midi_sysex->GetData(2);
      m_subID2 = m_midi_sysex->GetData(3);

      m_eoxAt = m_midi_sysex->GetData( m_midi_sysex->GetLength() - 1 ) == 0xF7 ? m_midi_sysex->GetLength() : 0;
    }

  private:
    const jdksmidi::MIDISystemExclusive * m_midi_sysex;
    unsigned char m_devID{}, m_subID1{}, m_subID2{};
    int m_eoxAt{};
  };

  class GSSysEx {
  public:
    enum {
      CMD_RQ1 = 0x11,
      CMD_DT1 = 0x12,

      MDL_GS = 0x42,
    };

    GSSysEx(const  jdksmidi::MIDISystemExclusive *midi_sysex): m_midi_sysex(midi_sysex) {
      Parse();
    };

    static GSSysEx * Create(const jdksmidi::MIDISystemExclusive *midi_sysex) {
      if (midi_sysex->GetLength() < 10) return nullptr;
      if (midi_sysex->GetData(0) == 0x41)
        return new GSSysEx(midi_sysex);
      return nullptr;
    }

    void Parse() {
      m_devID  = m_midi_sysex->GetData(1);
      m_mdlID  = m_midi_sysex->GetData(2);
      m_cmdID  = m_midi_sysex->GetData(3); // 0x11=RQ1 / 0x12=DT1
      m_cmdAddr = ( m_midi_sysex->GetData(4) << 16 ) |
        ( m_midi_sysex->GetData(5) << 8 ) |
        ( m_midi_sysex->GetData(6) );
      m_chkHash = m_midi_sysex->GetData(4) + m_midi_sysex->GetData(5) + m_midi_sysex->GetData(6);
      m_buf = m_midi_sysex->GetBuf() + 7;
      m_bufLen = m_midi_sysex->GetLength() - 9;
    }

    bool IsPartPatch() {
      return ((m_cmdAddr & 0xFF0000) == 0x400000 && (m_cmdAddr & 0x00F000) != 0);
    }

    bool IsModelGS() {
      return MDL_GS == m_mdlID;
    }

    int GetPart() {
      if (!IsPartPatch()) return 0;
      unsigned char part = (m_cmdAddr & 0x000F00) >> 8;
      return part == 0 ? 10 : part >= 10 ? part + 1 : part;
    }

    int GetPartAddress() {
      if (!IsPartPatch()) return 0;
      return m_cmdAddr & 0x00F0FF;
    }

    unsigned char GetData(int i) const {
      return m_buf[i];
    }

    size_t GetDataLen() const {
      return m_bufLen;
    }

    bool IsGSReset() {
      return IsModelGS() && IsDataSet() && m_cmdAddr == 0x40007F && GetDataLen() == 1 && GetData(0) == 0x00;
    }

    bool IsDataSet() {
      return m_cmdID = CMD_DT1;
    }

    bool IsRequestData() {
      return m_cmdID = CMD_RQ1;
    }

    bool IsSystemMode() {
      return IsModelGS() && IsDataSet() && m_cmdAddr == 0x00007F && GetDataLen() == 1;
    }

    bool IsUseRhythmPart() {
      if (!IsPartPatch()) return false;
      return GetPartAddress() == 0x1015;
    }

    int GetUseRhythmPart() {
      if (!IsUseRhythmPart()) return 0;
      return GetData(0);
    }

    unsigned char GetSystemMode() {
      if (!IsSystemMode()) return 0;
      if (GetData(0) == 0) return 1;
      if (GetData(0) == 1) return 2;
      return 0;
    }

    std::string Info() {
      if (IsGSReset()) {
        return "GS Reset";
      } else if (IsSystemMode()) {
        return fmt::format("System Mode {}", GetSystemMode());
      } else if (IsPartPatch()) {
        if (IsUseRhythmPart()) {
          return fmt::format("Part Patch {} Use Rhythm Part {}", GetPart(), GetUseRhythmPart());
        }
        return fmt::format("Part Patch {} Address {:04X}h", GetPart(), GetPartAddress());
      }
      return "??";
    }

  private:
    const jdksmidi::MIDISystemExclusive * m_midi_sysex;
    unsigned char m_devID{}, m_mdlID{}, m_cmdID{}, m_chkSum;
    unsigned int m_cmdAddr{};
    int m_chkHash{};
    const unsigned char *m_buf;
    int m_bufLen;
  };

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
        //unsigned char mfgID{}, devID{}, cmdID{}, mdlID{}, subID1{}, subID2{}, chkSum{};
        //unsigned int cmdAddr{};
        //std::vector<unsigned char> data{};
        //int chkHash{};

        std::string description;
        bool gm_reset_test = true;
        bool gs_reset_test = true;
        bool gm_reset = false;
        bool gs_reset = false;

        for (int i=0; i<msg->GetSysEx()->GetLength(); i++, buf++) {
#if 0
            if (i == 0) { // ID_number
              mfgID = *buf;
              switch (mfgID) {
                case 0x41:
                  description += "MFG:ROLAND "; break;
                  gm_reset_test = false;
                case 0x7E:
                  description += "MFG:UNIV-NON-RT "; break;
                  gs_reset_test = false;
                case 0x7F:
                  description += "MFG:UNIV-RT "; break;
                  gm_reset_test = false;
                  gs_reset_test = false;
                default:
                  description += fmt::format("MFG:{:#2X} ", mfgID); break;
                  gm_reset_test = false;
                  gs_reset_test = false;
              }
            }
            if (i == 1) {
              devID = *buf;
              switch (devID) {
                case 0x7F:
                  description += "BROADCAST "; break;
                default:
                  description += fmt::format("DEV:{} ({:#2X})", devID+1, devID); break;
                  gm_reset_test = false;
              }
            }
            if (mfgID == 0x7E) {
              if (i == 2) {
                subID1 = *buf;
                switch (subID1) {
                case 0x09:
                  description += "SUB#1:GM-MESSAGE "; break;
                default:
                  description += fmt::format("SUB#1:{} ", subID1);
                  gm_reset_test = false;
                }
              }
              if (i == 3) {
                subID2 = *buf;
                switch (subID2) {
                case 0x01:
                  description += "SUB#2:ON "; break;
                default:
                  description += fmt::format("SUB#2:{} ", subID2);
                  gm_reset_test = false;
                }
              }
              if (i >= 4) {
                if (0xF7 == *buf) {
                  description += "EOX";
                  if (gm_reset_test) gm_reset = true;
                } else {
                  description += "WARN:unhandled ";
                  gm_reset_test = false;
                }
              }
            }
            if (mfgID == 0x41) {
              if (i == 2) {
                mdlID = *buf;
                switch (mdlID) {
                case 0x42:
                  description += "MDL:GS "; break;
                default:
                  description += fmt::format("MDL:{} ", mdlID);
                  gs_reset_test = false;
                }
              }
              if (i == 3) {
                cmdID = *buf;
                switch (cmdID) {
                case 0x11:
                  description += "CMD:RQ1 ";
                  gs_reset_test = false;
                  break;
                case 0x12:
                  description += "CMD:DT1 "; break;
                default:
                  description += fmt::format("CMD:{:02x}h ", cmdID);
                  gs_reset_test = false;
                }
              }
              if (i == 4) {
                cmdAddr = *buf << 16;
                chkHash = *buf;
              }
              if (i == 5) {
                cmdAddr |= *buf << 8;
                chkHash += *buf;
              }
              if (i == 6) {
                cmdAddr |= *buf;
                chkHash += *buf;
                description += fmt::format("CMD-ADDR:{:06X}h ", cmdAddr);
              }
              if (i >= 7) {
                if (0xF7 == *buf) {
                  description += "EOX ";
                  if (data.size() >= 1) {
                    chkSum = data.back();
                    data.pop_back();

                    for (auto & d : data) {
                      chkHash += d;
                    }
                    description += fmt::format("ACTCHKSUM:{:02X}h ", 128 - (chkHash % 128));
                  }

                  // if (gs_reset_test) {
                  description += fmt::format("DATA-LEN:{} CHKSUM:{:02x}h ", data.size(), chkSum);

                  if (cmdAddr != 0x40007F) gs_reset_test = false;

                  if ((cmdAddr & 0xFF0000) == 0x400000 && (cmdAddr & 0x00F000) != 0) {
                    int fixaddr = cmdAddr & 0xF0FF;
                    char part = (cmdAddr & 0x0F00) >> 8;
                    part = part == 0 ? 10 : part >= 10 ? part + 1 : part;
                    description += fmt::format("PART {} ", (int)part);
                    if (fixaddr == 0x1015) {
                      if (data.size() == 1) {
                        description += fmt::format("USE-RHY-PART {}", (int)data[0]);
                      }
                    } else {
                      description += fmt::format("ADDR {:04X}h", fixaddr);
                    }
                  }

                  switch (cmdAddr) {
                  case 0x400000:  description += "MASTER-TUNE "; break;
                  case 0x400004:  description += "MASTER-VOL "; break;
                  case 0x400005:  description += "MASTER-KEYSHIFT "; break;
                  case 0x400006:  description += "MASTER-PAN "; break;
                  case 0x40007F:
                    if (data.size() == 1 && data[0] == 0x00) {
                      if (gs_reset_test) gs_reset = true;
                    } else {
                      gs_reset_test = false;
                    }
                    break;
                  case 0x00007F:
                    if (data.size() == 1 && data[0] == 0x00) {
                      description += "MODE-1 ";
                      // chksum == 1
                    } else
                    if (data.size() == 1 && data[0] == 0x01) {
                      description += "MODE-2 ";
                      // chksum == 0
                    }
                    break;
                  case 0x400130:
                    description += "REVERB-MACRO "; break;
                  case 0x400131:
                    description += "REVERB-CHAR "; break;
                  case 0x400132:
                    description += "REVERB-PRELPF "; break;
                  case 0x400133:
                    description += "REVERB-LVL "; break;
                  case 0x400134:
                    description += "REVERB-TIME "; break;
                  case 0x400135:
                    description += "REVERB-DLYFB "; break;
                  case 0x400137:
                    description += "REVERB-PERDLY-TIME "; break;
                  }

                } else {
                  data.push_back(*buf);
                  //description += "WARN:unhandled ";
                }
              }
            }
          #endif
          fmt::print(fmt::fg(fmt::color::aqua), " {:02X}", *buf);
        }

        //if (gm_reset)
        //if (gs_reset) description += " GS-RESET";

        std::unique_ptr<vinfony::GMSysEx> gmsysex(vinfony::GMSysEx::Create(msg->GetSysEx()));
        if (gmsysex) if (gmsysex->IsGMReset()) description += " GM-RESET";
        std::unique_ptr<vinfony::GSSysEx> gssysex(vinfony::GSSysEx::Create(msg->GetSysEx()));
        if (gssysex) description += gssysex->Info();
        fmt::print("\n=> {}\n", description);
      }
      //fmt::println("time: {}", msg->GetTime());
    }

    if (!eot) {
      fmt::print(fmt::fg(fmt::color::wheat), "WARNING: track without eot\n");
    }
    trackNotes.ClipOff(stop_time);
    fmt::println("notes show {} hide {} process {}",
    trackNotes.notes_to_draw,
    trackNotes.notes_to_hide,
    trackNotes.notes_processed);
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
