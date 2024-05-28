#include "DawSeq.hpp"

#define USE_SDLTICK 0

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

#include "TsfDev.hpp"
#include "DawTrack.hpp"

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
    std::map<int, std::unique_ptr<DawTrack>> tracks; // TODO: move to DawDoc
    std::vector<int> track_nums; // TODO: move to DawDoc
    BaseMidiOutDevice * audioDevice{};
    jdksmidi::MIDIClockTime clk_play_start_time{0};
    bool read_clk_play_start{true};

    DawSeq * self;

    Impl(DawSeq * owner): self(owner) {};
    DawTrack * AddNewTrack(int midi_track_id, jdksmidi::MIDITrack * midi_track); // TODO: move to DawDoc
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

    m_impl->track_nums.clear();
    m_impl->tracks.clear();
    SetPlayClockTime(0);

    displayState.ppqn = m_impl->midi_multi_tracks->GetClksPerBeat();

    // Detect Track MIDI channels
    for (int trk_num=0; trk_num<m_impl->midi_multi_tracks->GetNumTracks(); ++trk_num) {
      auto midi_track = m_impl->midi_multi_tracks->GetTrack(trk_num);
      if (midi_track->IsTrackEmpty()) continue;
      int ch = 0;
      std::string track_name;
      for (int event_num = 0; event_num < midi_track->GetNumEvents(); ++event_num) {
        const jdksmidi::MIDITimedBigMessage * msg = midi_track->GetEvent(event_num);
        if (msg->IsNoteOn()) {
          if (ch == 0) ch = msg->GetChannel()+1;
#if 0
          char msgbuf[1024];
          fmt::println("TRACK {} CH {} EVENT: {}", trk_num, channel, msg->MsgToText(msgbuf, 1024));
        } else if (ch != msg->GetChannel()+1) {
            fmt::println("WARNING: track {} has more than channels, was {} want {}", trk_num, channel,
              msg->GetChannel());
        }
#endif
        }

        if (msg->IsTrackName()) {
          if (track_name.empty()) track_name = msg->GetSysExString();
        }
      }
      DawTrack * track = m_impl->AddNewTrack(trk_num, midi_track);
      track->ch = ch;
      track->name = track_name.empty() ?fmt::format("Track {}", trk_num) : track_name;
    }

    jdksmidi::MIDIMultiTrackIterator it( m_impl->midi_multi_tracks.get() );
    it.GoToTime( 0 );
    do {
      int trk_num;
      const jdksmidi::MIDITimedBigMessage *msg;

      if ( it.GetCurEvent( &trk_num, &msg ) )
      {
        auto track_it = m_impl->tracks.find(trk_num);
        if (track_it == m_impl->tracks.end()) continue;
        DawTrack * track = track_it->second.get();

        if (msg->IsProgramChange()) {
          for (const auto & kv: m_impl->tracks) {
            if (kv.second->ch == msg->GetChannel() + 1) {
              if (kv.second->pg == 0) {
                kv.second->pg = msg->GetPGValue() + 1;
#if 1
                char msgbuf[1024];
                fmt::println("TRACK {} CH {} EVENT: {}", kv.second->id, track->ch, msg->MsgToText(msgbuf, 1024));
#endif
              }
            }
          }
        }

        if (msg->IsControlChange()) {
          for (const auto & kv: m_impl->tracks) {
            if (kv.second->ch == msg->GetChannel() + 1) {
              if (kv.second->pg == 0) {
                kv.second->SetBank(msg);
              }
            }
          }
        }

        //fprintf( stdout, "#%2d - ", trk_num );
        //DumpMIDITimedBigMessage( msg );
      }
    }
    while ( it.GoToNextEvent() );


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

      m_impl->seqMessaging.push(SeqMsg::OnMIDIFileLoaded(midifile));
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
      m_impl->audioDevice->HardwareMsgOut( msg, nullptr );
      msg.SetControlChange( chan, 0x40, 0 ); // C_DAMPER 0x40,     ///< hold pedal (sustain)
      m_impl->audioDevice->HardwareMsgOut( msg, nullptr );
      msg.SetAllNotesOff( (unsigned char)chan );
      m_impl->audioDevice->HardwareMsgOut( msg, nullptr );
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

      float pretend_clock_time = 0;
      float before_clock_time = pretend_clock_time;
#if USE_SDLTICK == 1
      // simulate a clock going forward with 10 ms resolution for 1 hour
      float start = SDL_GetTicks64();
#else
      std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now();
#endif
      const float max_time = 3600. * 1000.;

      for ( ; pretend_clock_time < max_time;
        before_clock_time = pretend_clock_time,
#if USE_SDLTICK == 1
        pretend_clock_time = SDL_GetTicks64() - start
#else
        pretend_clock_time = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count()
#endif
      ) {
        if (m_impl->read_clk_play_start) {
          if (m_impl->clk_play_start_time == 0) m_impl->audioDevice->Reset();

          AllMIDINoteOff();

          m_impl->midi_seq->GoToTime( m_impl->clk_play_start_time );
          //m_impl->midi_seq->GoToTimeMs( pretend_clock_time );
          pretend_clock_time = m_impl->midi_seq->GetCurrentTimeInMs();
          before_clock_time = pretend_clock_time;

          if ( !m_impl->midi_seq->GetNextEventTimeMs( &next_event_time ) ) {
            fmt::println("DEBUG: empty next event, stop!");
            return;
          }

          // simulate a clock going forward with 10 ms resolution for 1 hour
#if USE_SDLTICK == 1
          start = SDL_GetTicks64() - pretend_clock_time;
#else
          start = std::chrono::high_resolution_clock::now() - std::chrono::microseconds((long)(pretend_clock_time * 1000.0));
#endif
          m_impl->read_clk_play_start = false;
        }
        CalcCurrentMIDITimeBeat(pretend_clock_time);

        // TODO: Send SyncTime
        m_impl->audioDevice->UpdateMIDITicks();

        // find all events that came before or a the current time
        while ( next_event_time <= pretend_clock_time ) {
          if ( m_impl->midi_seq->GetNextEvent( &ev_track, &msg ) )
          {
            // found the event!
            // show it to stdout
            //printf( "PUSH: tm=%06.0f : evtm=%06.0f :trk%02d : \n", pretend_clock_time, next_event_time, ev_track );

            auto trackit = m_impl->tracks.find(ev_track);
            if (trackit != m_impl->tracks.end()) {

              if (msg.IsTrackName()) {
                std::string track_name = msg.GetSysExString();
                trackit->second->name = track_name;
                fmt::println("DEBUG: TRACK {} CHANNEL: {} NAME: {}", ev_track, msg.GetChannel(), track_name);
              }

              if (msg.IsControlChange()) {
                trackit->second->SetBank(&msg);
              }

              if (msg.IsProgramChange()) {
                if (trackit->second->ch == msg.GetChannel()+1) {
                  trackit->second->pg = msg.GetPGValue()+1;
                  fmt::println("DEBUG: TRACK {} CHANNEL: {} BANK: {:04X}h PG:{}", ev_track, msg.GetChannel(),
                    trackit->second->bank, trackit->second->pg);
                }

                for (const auto & kv: m_impl->tracks) {
                  if (trackit->first == kv.second->id) continue; // already set
                  if (kv.second->ch == msg.GetChannel() + 1) {
                    kv.second->pg = msg.GetPGValue()+1;
                  }
                }
              }
            }

            double shiftMs = m_impl->midi_seq->GetCurrentTimeInMs() - pretend_clock_time;
            if (!m_impl->audioDevice->HardwareMsgOut( msg, &shiftMs )) {
              return;
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
#if USE_SDLTICK == 1
        SDL_Delay(10);
#else
        std::this_thread::sleep_for(10ms);
#endif
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

  // TODO: refactor
  DawTrack * DawSeq::GetTrack(int track_num) {
    const int track_id = m_impl->track_nums[track_num];
    return m_impl->tracks[track_id].get();
  }

  // TODO: deprecated
  DawTrack * DawSeq::Impl::AddNewTrack(int midi_track_id, jdksmidi::MIDITrack * midi_track) {
    tracks[midi_track_id] = std::make_unique<DawTrack>();

    DawTrack * track = tracks[midi_track_id].get();
    track->id = midi_track_id;

    float ht = (float)(((int)ImGui::GetFrameHeightWithSpacing()*3/2) & ~1);
    track->h = ht;
    track->midi_track = midi_track;

    track_nums.push_back(track->id);

    return track;
  }

  void DawSeq::SetDevice(BaseMidiOutDevice * dev) {
    m_impl->audioDevice = dev;
  }

}