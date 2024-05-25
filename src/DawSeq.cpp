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
    std::unique_ptr<jdksmidi::MIDISequencer> midi_seq{};

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
    BaseMidiOutDevice * audioDevice{};
    jdksmidi::MIDIClockTime clk_play_start_time{0};
    bool read_clk_play_start{true};

    DawSeq * self;

    Impl(DawSeq * owner): self(owner) {};
    int AddNewTrack(std::string name, jdksmidi::MIDITrack * midi_track);
  };

  DawSeq::DawSeq() {
    m_impl = std::make_unique<Impl>(this);
  }

  DawSeq::~DawSeq() {
    if (m_impl->th_read_midi_file.joinable())
        m_impl->th_read_midi_file.join();
    StopMIDI();
    AsyncPlayMIDIStopped();
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

  void DawSeq::SetPlayClockTime(unsigned long clk_time) {
    m_impl->clk_play_start_time = clk_time;

    if (m_impl->midi_multi_tracks->GetClksPerBeat())
      displayState.play_cursor = (float)clk_time/m_impl->midi_multi_tracks->GetClksPerBeat();
    else
      displayState.play_cursor = 0;

    m_impl->read_clk_play_start =  true;
  }

  bool DawSeq::IsFileLoaded() {
    if (m_impl->th_read_midi_file_done) {
      m_impl->th_read_midi_file_done = false;

      if (m_impl->th_read_midi_file.joinable())
        m_impl->th_read_midi_file.join();

      m_impl->track_nums.clear();
      m_impl->tracks.clear();
      m_impl->last_tracks_id = 0;
      SetPlayClockTime(0);

      displayState.ppqn = m_impl->midi_multi_tracks->GetClksPerBeat();

      for (int i=0; i<m_impl->midi_multi_tracks->GetNumTracks(); ++i) {
        auto midi_track = m_impl->midi_multi_tracks->GetTrack(i);
        if (midi_track->IsTrackEmpty()) continue;

        auto track_name = fmt::format("Track {}", i);

        for (int event_num = 0; event_num < midi_track->GetNumEvents(); ++event_num) {
          const jdksmidi::MIDITimedBigMessage * msg = midi_track->GetEvent(event_num);
          if (msg->IsTrackName()) {
            track_name = msg->GetSysExString();
            break;
          }
        }

        m_impl->AddNewTrack(track_name, midi_track);
      }
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

    m_impl->midi_seq = std::make_unique<jdksmidi::MIDISequencer>( m_impl->midi_multi_tracks.get() );
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

    m_impl->midi_seq->GoToZero();

    while ( m_impl->midi_seq->GetNextEvent( &ev_track, &ev ) )
    {
        // std::cout << EventAsText( ev ) << std::endl;

        // skip these events
        if ( ev.IsEndOfTrack() || ev.IsBeatMarker() )
            continue;

        // end of music is the time of last not end of track midi event!
        event_time = m_impl->midi_seq->GetCurrentTimeInMs();
        clk_time   = m_impl->midi_seq->GetCurrentMIDIClockTime();
    }

    displayState.duration_ms = event_time;
    displayState.play_duration = clk_time / m_impl->midi_multi_tracks->GetClksPerBeat();
  }

  // from: AdvancedSequencer::GetCurrentMIDIClockTime
  void DawSeq::CalcCurrentMIDITimeBeat(uint64_t now_ms)
  {
    jdksmidi::MIDIClockTime time = m_impl->midi_seq->GetCurrentMIDIClockTime();
    double ms_offset = now_ms - m_impl->midi_seq->GetCurrentTimeInMs();
    double ms_per_clock = 60000.0 / ( m_impl->midi_seq->GetState()->tempobpm * m_impl->midi_seq->GetCurrentTempoScale() * m_impl->midi_multi_tracks->GetClksPerBeat() );
    time += ( jdksmidi::MIDIClockTime )( ms_offset / ms_per_clock );
    m_impl->clk_play_start_time = time;

    displayState.play_cursor = (float)time/m_impl->midi_multi_tracks->GetClksPerBeat();
  }

  void DawSeq::SetMIDITimeBeat(float time_beat) {
    SetPlayClockTime( time_beat * m_impl->midi_multi_tracks->GetClksPerBeat() );
  }

  void DawSeq::CloseMIDIFile() {
    m_impl->midi_file_loaded = false;
    m_impl->midi_seq.reset();
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

  // AsyncPlayMIDIStopped being calledd by OnAsyncPlayMIDITerminated
  void DawSeq::AsyncPlayMIDIStopped() {
    if (m_impl->th_play_midi.joinable()) {
      m_impl->th_play_midi.join();
    }
  }

  void DawSeq::AllMIDINoteOff() {
    jdksmidi::MIDITimedBigMessage msg;
    for (int chan=0; chan<16; ++chan) {
      msg.SetPitchBend( chan, 0 );
      m_impl->audioDevice->HardwareMsgOut( msg );
      msg.SetControlChange( chan, 0x40, 0 ); // C_DAMPER 0x40,     ///< hold pedal (sustain)
      m_impl->audioDevice->HardwareMsgOut( msg );
      msg.SetAllNotesOff( (unsigned char)chan );
      m_impl->audioDevice->HardwareMsgOut( msg );
    }
  }

  void DawSeq::Reset() {
    m_impl->audioDevice->Reset();
  }

  void DawSeq::AsyncPlayMIDI() {
    if (m_impl->th_play_midi_running) return;

    m_impl->th_play_midi_running = true;
    m_impl->read_clk_play_start = true;

    m_impl->th_play_midi = std::thread([this]() {
      float next_event_time;
      jdksmidi::MIDITimedBigMessage msg;
      int ev_track;

      std::shared_ptr<void> _(nullptr, [&](...) {
        m_impl->th_play_midi_running = false;
        m_impl->request_stop_midi = false;
        m_impl->seqMessaging.push(SeqMsg::OnAsyncPlayMIDITerminated());
      });

      m_impl->audioDevice->Reset();

      float pretend_clock_time = 0;
      // simulate a clock going forward with 10 ms resolution for 1 hour
      const float max_time = 3600. * 1000.;
      float start = SDL_GetTicks64();

      for ( ; pretend_clock_time < max_time; pretend_clock_time = SDL_GetTicks64() - start)
      {
        if (m_impl->read_clk_play_start) {
          AllMIDINoteOff();

          m_impl->midi_seq->GoToTime( m_impl->clk_play_start_time );
          //m_impl->midi_seq->GoToTimeMs( pretend_clock_time );
          pretend_clock_time = m_impl->midi_seq->GetCurrentTimeInMs();

          if ( !m_impl->midi_seq->GetNextEventTimeMs( &next_event_time ) ) {
            fmt::println("DEBUG: empty next event, stop!");
            return;
          }

          // simulate a clock going forward with 10 ms resolution for 1 hour
          start = SDL_GetTicks64() - pretend_clock_time;

          m_impl->read_clk_play_start = false;
        }
        CalcCurrentMIDITimeBeat(pretend_clock_time);

        // find all events that came before or a the current time
        while ( next_event_time <= pretend_clock_time ) {
          if ( m_impl->midi_seq->GetNextEvent( &ev_track, &msg ) )
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
              // fmt::println("DEBUG: TRACK {} CHANNEL: {} NAME: {}", ev_track, msg.GetChannel(), track_name);
            } else {
              if (!m_impl->audioDevice->HardwareMsgOut( msg )) {
                return;
              }
            }
            // now find the next message

            if ( !m_impl->midi_seq->GetNextEventTimeMs( &next_event_time ) ) {
              // no events left so end
              fmt::println( "End" );
              m_impl->request_stop_midi =  true;
              break;
            }
          }
        }

        // 10ms when using namespace std::chrono_literals;
        SDL_Delay(10);
        if (m_impl->request_stop_midi) {
          break;
        }
      }

      AllMIDINoteOff();
    });
  }

  void DawSeq::StopMIDI() {
    if (m_impl->th_play_midi_running) {
      m_impl->request_stop_midi = true;
    }
  }

  bool DawSeq::IsPlaying() {
    return m_impl->th_play_midi_running;
  }

  DawTrack * DawSeq::GetTrack(int track_num) {
    const int track_id = m_impl->track_nums[track_num];
    return m_impl->tracks[track_id].get();
  }

  int DawSeq::Impl::AddNewTrack(std::string name, jdksmidi::MIDITrack * midi_track) {
    last_tracks_id++;
    tracks[last_tracks_id] = std::make_unique<DawTrack>();

    auto track = tracks[last_tracks_id].get();
    track->id = last_tracks_id;
    track->name = name;
    float ht = (float)(((int)ImGui::GetFrameHeightWithSpacing()*3/2) & ~1);
    track->h = ht;
    track->midi_track = midi_track;

    track_nums.push_back(track->id);

    return track->id;
  }

  void DawSeq::SetDevice(BaseMidiOutDevice * dev) {
    m_impl->audioDevice = dev;
  }

}