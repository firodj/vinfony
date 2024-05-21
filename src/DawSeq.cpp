#include "DawSeq.hpp"

#include <iostream>
#include <thread>
#include <atomic>
//#include <chrono>
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

#include "TsfDev.hpp"

namespace vinfony {

  struct DawSeq::Impl {
    //std::unique_ptr<jdksmidi::MIDIFileReadStreamFile> rs{};
    std::unique_ptr<jdksmidi::MIDIMultiTrack> midi_multi_tracks{};
    std::unique_ptr<jdksmidi::MIDISequencer> seq{};

    // Threading related
    std::thread th_read_midi_file{};
    std::atomic<bool> th_read_midi_file_done{false};
    std::atomic<bool> th_read_midi_file_running{false};
    bool midi_file_loaded{};
    std::thread th_play_midi{};
    std::atomic<bool> th_play_midi_running{false};
    std::atomic<bool> request_stop_midi{false};

    CircularFifo<SeqMsg, 8> seqMessaging{};
    std::map<int, std::unique_ptr<DawTrack>> tracks;
    int last_tracks_id{0};
    std::vector<int> track_nums;
  };

  DawSeq::DawSeq() {
    m_impl = std::make_unique<Impl>();
  }

  DawSeq::~DawSeq() {
    if (m_impl->th_read_midi_file.joinable())
        m_impl->th_read_midi_file.join();
    StopMIDI();
  }

  void DawSeq::ProcessMessage(std::function<bool(SeqMsg&)> proc) {
    SeqMsg msg;
    while (m_impl->seqMessaging.pop(msg)) {
      // check msg is Display? otherwise...
      if (proc) {
        if (!proc(msg)) break;
      }
    }
  }

  bool DawSeq::IsFileLoaded() {
    if (m_impl->th_read_midi_file_done) {
      m_impl->th_read_midi_file_done = false;

      if (m_impl->th_read_midi_file.joinable())
        m_impl->th_read_midi_file.join();


      // Tracks;
      AddNewTrack("Piano");
      AddNewTrack("Bass");
      AddNewTrack("Drum");
    }

    return m_impl->th_read_midi_file_running ? false : m_impl->midi_file_loaded;
  }

  int DawSeq::GetNumTracks() {
    return m_impl->track_nums.size();
  }

  bool DawSeq::ReadMIDIFile(std::string filename) {
    jdksmidi::MIDIFileReadStreamFile rs(filename.c_str());

    m_impl->midi_multi_tracks = std::make_unique<jdksmidi::MIDIMultiTrack>();

    jdksmidi::MIDIFileReadMultiTrack track_loader( m_impl->midi_multi_tracks.get() );

    jdksmidi::MIDIFileRead reader( &rs, &track_loader );

    if ( !reader.Parse() )
    {
      fmt::println("Error parse file {}", filename);
      return false;
    }

    if ( m_impl->midi_multi_tracks->GetNumTracksWithEvents() == 1 ) {//
      fmt::println("all events in one track: format 0, separated them!");
      // redistributes channel events in separate tracks
      m_impl->midi_multi_tracks->AssignEventsToTracks(0);
    }

    m_impl->seq = std::make_unique<jdksmidi::MIDISequencer>( m_impl->midi_multi_tracks.get() );
    fmt::println("Clocks per beat = {}", m_impl->midi_multi_tracks->GetClksPerBeat());
    CalcDuration();
    fmt::println("Duration {} ms / {} beat", displayState.duration_ms, displayState.play_duration);
    return true;
  }

  void DawSeq::CalcDuration() {
    double event_time = 0.; // in milliseconds
    jdksmidi::MIDIClockTime clk_time = 0;

    jdksmidi::MIDITimedBigMessage ev;
    int ev_track;

    m_impl->seq->GoToZero();

    while ( m_impl->seq->GetNextEvent( &ev_track, &ev ) )
    {
        // std::cout << EventAsText( ev ) << std::endl;

        // skip these events
        if ( ev.IsEndOfTrack() || ev.IsBeatMarker() )
            continue;

        // end of music is the time of last not end of track midi event!
        event_time = m_impl->seq->GetCurrentTimeInMs();
        clk_time   = m_impl->seq->GetCurrentMIDIClockTime();
    }

    displayState.duration_ms = event_time;
    displayState.play_duration = clk_time / m_impl->midi_multi_tracks->GetClksPerBeat();
  }

  // from: AdvancedSequencer::GetCurrentMIDIClockTime
  void DawSeq::CalcCurrentMIDITimeBeat(uint64_t now)
  {
    jdksmidi::MIDIClockTime time = m_impl->seq->GetCurrentMIDIClockTime();
    double ms_offset = now - m_impl->seq->GetCurrentTimeInMs();
    double ms_per_clock = 60000.0 / ( m_impl->seq->GetState()->tempobpm * m_impl->seq->GetCurrentTempoScale() * m_impl->midi_multi_tracks->GetClksPerBeat() );
    time += ( jdksmidi::MIDIClockTime )( ms_offset / ms_per_clock );

    displayState.play_cursor = (float)time/m_impl->midi_multi_tracks->GetClksPerBeat();
  }

  void DawSeq::CloseMIDIFile() {
    m_impl->midi_file_loaded = false;
    m_impl->seq.reset();
    m_impl->midi_multi_tracks.reset();
  }

  void DawSeq::AsyncReadMIDIFile(std::string filename) {
    StopMIDI();
    CloseMIDIFile();

    fmt::println("Opening midi file: {}", filename);
    if (m_impl->th_read_midi_file_running) return;
    m_impl->th_read_midi_file_running = true;

    m_impl->th_read_midi_file = std::thread([this](std::string filename){
      std::string midifile = filename;
      m_impl->midi_file_loaded = ReadMIDIFile(midifile);
      m_impl->th_read_midi_file_done = true;
      fmt::println("Done open midi file: {}", midifile);
      m_impl->th_read_midi_file_running = false;
    }, filename);
  }

  void DawSeq::AsyncPlayMIDI() {
    if (m_impl->th_play_midi_running) return;

    m_impl->th_play_midi_running = true;

    m_impl->th_play_midi = std::thread([this]() {
      float pretend_clock_time = 0.0;
      float next_event_time = 0.0;
      jdksmidi::MIDITimedBigMessage msg;
      int ev_track;

      auto dev = CreateTsfDev();
      if (!dev->Init()) return;

      std::shared_ptr<void> _(nullptr, [&](...) {
        m_impl->th_play_midi_running = false;
        m_impl->request_stop_midi = false;
        m_impl->seqMessaging.push(SeqMsg::ThreadTerminate());
        dev->Shutdown();
      });

      m_impl->seq->GoToTimeMs( pretend_clock_time );

      if ( !m_impl->seq->GetNextEventTimeMs( &next_event_time ) )
      {
        return;
      }

      // simulate a clock going forward with 10 ms resolution for 1 hour
      float max_time = 3600. * 1000.;
      const auto start = SDL_GetTicks64();
      for ( ; pretend_clock_time < max_time; pretend_clock_time = SDL_GetTicks64() - start)
      {
        CalcCurrentMIDITimeBeat(pretend_clock_time);

        // find all events that came before or a the current time
        while ( next_event_time <= pretend_clock_time ) {
          if ( m_impl->seq->GetNextEvent( &ev_track, &msg ) )
          {
            // found the event!
            // show it to stdout
            //printf( "PUSH: tm=%06.0f : evtm=%06.0f :trk%02d : \n", pretend_clock_time, next_event_time, ev_track );
            if (msg.IsTrackName()) {
              char track_name[256];
              // yes, copy the track name
              int len = msg.GetSysEx()->GetLengthSE();
              if ( len > (int)sizeof( track_name ) - 1 )
                len = (int)sizeof( track_name ) - 1;

              memcpy( track_name, msg.GetSysEx()->GetBuf(), len );
              track_name[len] = '\0';
              fmt::println("TRACK {} CHANNEL: {} NAME: {}", ev_track, msg.GetChannel(), track_name);
            } else {

              if (!dev->HardwareMsgOut( msg )) return;

            }
            // now find the next message

            if ( !m_impl->seq->GetNextEventTimeMs( &next_event_time ) )
            {
              // no events left so end
              printf( "End\n" );
              m_impl->request_stop_midi =  true;
              break;
            }
          }
        }

        // 10ms when using namespace std::chrono_literals;
        SDL_Delay(10);
        if (m_impl->request_stop_midi) break;
      }

      for (int chan=0; chan<16; ++chan) {
        msg.SetControlChange( chan, 0x40, 0 ); // C_DAMPER 0x40,     ///< hold pedal (sustain)
        dev->HardwareMsgOut( msg );
        msg.SetAllNotesOff( (unsigned char)chan );
        dev->HardwareMsgOut( msg );
      }

    });
  }

  void DawSeq::StopMIDI() {
    if (m_impl->th_play_midi_running) {
      m_impl->request_stop_midi = true;
    }
    if (m_impl->th_play_midi.joinable()) {
      m_impl->th_play_midi.join();
    }
  }

  DawTrack * DawSeq::GetTrack(int track_num) {
    const int track_id = m_impl->track_nums[track_num];
    return m_impl->tracks[track_id].get();
  }

  int DawSeq::AddNewTrack(std::string name) {
    m_impl->last_tracks_id++;
    m_impl->tracks[m_impl->last_tracks_id] = std::make_unique<DawTrack>();

    auto track = m_impl->tracks[m_impl->last_tracks_id].get();
    track->id = m_impl->last_tracks_id;
    track->name = name;
    float ht = (float)(((int)ImGui::GetFrameHeightWithSpacing()*3/2) & ~1);
    track->h = ht;

    m_impl->track_nums.push_back(track->id);

    return track->id;
  }

}