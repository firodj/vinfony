#include "DawSeq.hpp"

#define USE_SDLTICK 1

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
#include <fmt/color.h>

#include "TsfDev.hpp"
#include "DawTrack.hpp"
#include "DawDoc.hpp"

namespace vinfony {

  struct DawSeq::Impl {
    std::unique_ptr<DawDoc> doc{};
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
    TinySoundFontDevice * tsfdev{}; // should shared_ptr ?
    BassMidiDevice * bassdev{}; // should shared_ptr ?
    BaseMidiOutDevice *mididev{};
    jdksmidi::MIDIClockTime clk_play_start_time{0};
    bool read_clk_play_start{true};
    bool is_rewinding{false};

    DawSeq * self;

    Impl(DawSeq * owner): self(owner) {};
  };

  DawSeq::DawSeq() {
    m_impl = std::make_unique<Impl>(this);

    // NewDocs

    m_impl->doc = std::make_unique<DawDoc>();
    DawTrack * track = m_impl->doc->AddNewTrack(0, nullptr);
    track->SetSeq(this);
    track->ch = 1;
    track->name = "Piano";

    track = m_impl->doc->AddNewTrack(1, nullptr);
    track->SetSeq(this);
    track->ch = 10;
    track->name = "Drum";

    m_impl->midi_file_loaded = true;
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

    if (m_impl->doc && m_impl->doc->GetPPQN()) {
      if (m_impl->midi_seq && !IsPlaying()) m_impl->midi_seq->GoToTime( m_impl->clk_play_start_time );
      displayState.play_cursor = (float)clk_time/m_impl->doc->GetPPQN();
      if (m_impl->clk_play_start_time == 0) m_impl->is_rewinding = true;
    } else
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
    return m_impl->doc->GetNumTracks();
  }

  bool DawSeq::ReadMIDIFile(std::string filename) {
    class TheMidiEventsHandler: public jdksmidi::MIDIFileReadMultiTrack {
    public:
      TheMidiEventsHandler( jdksmidi::MIDIMultiTrack *mlttrk ):  jdksmidi::MIDIFileReadMultiTrack(mlttrk) {

      }
      virtual void mf_error( const char * msg ) override {
        fmt::print(fmt::fg(fmt::color::crimson), "\nError: {}\n", msg);
      }
      virtual void mf_warning( const char * msg ) override {
        fmt::print(fmt::fg(fmt::color::wheat), "\nWarning: {}\n", msg);
      }
    };

    jdksmidi::MIDIFileReadStreamFile rs(filename.c_str());
    jdksmidi::MIDIMultiTrack tracks{};
    TheMidiEventsHandler track_loader( &tracks );
    jdksmidi::MIDIFileRead reader( &rs, &track_loader );

    if ( !reader.Parse() )
    {
      fmt::print(fmt::fg(fmt::color::crimson), "\nError parse file {}\n", filename);
      return false;
    }

    m_impl->doc = std::make_unique<DawDoc>();
    m_impl->doc->LoadFromMIDIMultiTrack( &tracks );
    for (int r=0; r<m_impl->doc->GetNumTracks(); r++) {
      DawTrack * track = m_impl->doc->GetTrack(r);
      track->SetSeq(this);
      // better check default value from audioDevice (tsf)
      if (track->ch && track->pg == 0)            track->pg = 1;
      if (track->ch && track->midiPan == -1)      track->midiPan = 8192;
      if (track->ch && track->midiVolume == -1)   track->midiVolume = 16383;
      if (track->ch && track->midiFilterFc == -1) track->midiFilterFc = 64;
      if (track->ch && track->midiFilterQ == -1)  track->midiFilterQ = 64;
    }

    displayState.ppqn = m_impl->doc->GetPPQN();
    fmt::println("Clocks per beat = {}", displayState.ppqn);

    m_impl->midi_seq = std::make_unique<jdksmidi::MIDISequencer>( m_impl->doc->m_midiMultiTrack.get() );

    CalcDuration();
    fmt::println("Duration {} ms / {} beat", displayState.duration_ms, displayState.play_duration);
    SetPlayClockTime(0);

    return true;
  }

  void DawSeq::CalcDuration() {
    double event_time = 0.; // in milliseconds
    jdksmidi::MIDIClockTime clk_time = 0;

    jdksmidi::MIDITimedBigMessage ev;
    int ev_track;

    if (m_impl->midi_seq) {
      m_impl->midi_seq->GoToZero();

      while ( m_impl->midi_seq->GetNextEvent( &ev_track, &ev ) ) {
          // std::cout << EventAsText( ev ) << std::endl;

          // skip these events
          if ( ev.IsEndOfTrack() || ev.IsBeatMarker() )
              continue;

          // end of music is the time of last not end of track midi event!
          event_time = m_impl->midi_seq->GetCurrentTimeInMs();
          clk_time   = m_impl->midi_seq->GetCurrentMIDIClockTime();
      }

      displayState.duration_ms = event_time;
      displayState.play_duration = clk_time / m_impl->doc->GetPPQN();
    }
  }

  // from: AdvancedSequencer::GetCurrentMIDIClockTime
  void DawSeq::CalcCurrentMIDITimeBeat(uint64_t now_ms)
  {
    jdksmidi::MIDIClockTime time = 0;
    int ppqn = 24;

    if (m_impl->midi_seq && m_impl->doc) {
      double ms_offset = now_ms - m_impl->midi_seq->GetCurrentTimeInMs();
      double ms_per_clock = 60000.0 / ( m_impl->midi_seq->GetState()->tempobpm * m_impl->midi_seq->GetCurrentTempoScale() * m_impl->doc->GetPPQN() );

      time = m_impl->midi_seq->GetCurrentMIDIClockTime() + (jdksmidi::MIDIClockTime )( ms_offset / ms_per_clock );

      ppqn = m_impl->doc->GetPPQN();
    }

    m_impl->clk_play_start_time = time;
    displayState.play_cursor = (float)time/ppqn;
  }

  float DawSeq::GetTempoBPM() {
    float tempobpm = 120;

    if (m_impl->midi_seq) tempobpm = m_impl->midi_seq->GetState()->tempobpm;
    return tempobpm;
  }

  void DawSeq::GetCurrentMBT(int &m, int &b, int &t) {
    if (m_impl->midi_seq) {
      m = m_impl->midi_seq->GetCurrentMeasure() + 1;
      b = m_impl->midi_seq->GetCurrentBeat() + 1;
      t = displayState.ppqn > 0 ? (int)(displayState.play_cursor * displayState.ppqn) % displayState.ppqn : 0;
    } else {
      m = 0;
      b = 0;
      t = 0;
    }
  }

  void DawSeq::SetMIDITimeBeat(float time_beat) {
    SetPlayClockTime( time_beat * m_impl->doc->GetPPQN() );
  }

  void DawSeq::CloseMIDIFile() {
    m_impl->midi_file_loaded = false;
    m_impl->midi_seq.reset();
    m_impl->doc.reset();
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

  void DawSeq::AllMIDINoteOff() {
    jdksmidi::MIDITimedBigMessage msg;
    for (int chan=0; chan<16; ++chan) {
      msg.SetPitchBend( chan, 0 );
      m_impl->mididev->HardwareMsgOut( msg, nullptr );
      msg.SetControlChange( chan, 0x40, 0 ); // C_DAMPER 0x40,     ///< hold pedal (sustain)
      m_impl->mididev->HardwareMsgOut( msg, nullptr );
      msg.SetAllNotesOff( (unsigned char)chan );
      m_impl->mididev->HardwareMsgOut( msg, nullptr );
    }
  }

  void DawSeq::SendVolume(int chan, unsigned short value) {
    jdksmidi::MIDITimedBigMessage msg;
    msg.SetControlChange( chan, jdksmidi::C_MAIN_VOLUME, (value & 0x3F80) >> 7);
    m_impl->mididev->HardwareMsgOut( msg, nullptr );
    msg.SetControlChange( chan, jdksmidi::C_MAIN_VOLUME_LSB, value & 0x7F );
    m_impl->mididev->HardwareMsgOut( msg, nullptr );
  }

  void DawSeq::SendPan(int chan, unsigned short value) {
    jdksmidi::MIDITimedBigMessage msg;
    msg.SetControlChange( chan, jdksmidi::C_PAN, (value & 0x3F80) >> 7);
    m_impl->mididev->HardwareMsgOut( msg, nullptr );
    msg.SetControlChange( chan, jdksmidi::C_PAN_LSB, value & 0x7F );
    m_impl->mididev->HardwareMsgOut( msg, nullptr );
  }

  void DawSeq::SendFilter(int chan, unsigned short valFc, unsigned short valQ) {
    jdksmidi::MIDITimedBigMessage msg;
    msg.SetControlChange( chan, jdksmidi::C_HARMONIC, valQ);
    m_impl->mididev->HardwareMsgOut( msg, nullptr );
    msg.SetControlChange( chan, jdksmidi::C_BRIGHTNESS, valFc );
    m_impl->mididev->HardwareMsgOut( msg, nullptr );
  }

  void DawSeq::Reset() {
    m_impl->mididev->Reset();
  }

  // AsyncPlayMIDIStopped being calledd by OnAsyncPlayMIDITerminated
  void DawSeq::AsyncPlayMIDIStopped() {
    if (m_impl->th_play_midi.joinable()) {
      m_impl->th_play_midi.join();
    }
  }

  void DawSeq::AsyncPlayMIDI() {
    if (m_impl->th_play_midi_running) return;

    // Select MIDI devices, BASSMIDI or TSF
#if 1
    m_impl->mididev = (BaseMidiOutDevice*) m_impl->bassdev;
#else
    m_impl->mididev = (BaseMidiOutDevice*) m_impl->tsfdev;
#endif
    printf( "Selecting MIDI Devices: %s\n", m_impl->mididev->GetName() );

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
#if USE_SDLTICK == 1
      // simulate a clock going forward with 10 ms resolution for 1 hour
      float start = SDL_GetTicks64();
#else
      std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now();
#endif
      const float max_time = 3600. * 1000.;

      for ( ; pretend_clock_time < max_time;
#if USE_SDLTICK == 1
        pretend_clock_time = SDL_GetTicks64() - start
#else
        pretend_clock_time = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count()
#endif
      ) {

        if (m_impl->read_clk_play_start) {
          bool isRewinding = m_impl->clk_play_start_time == 0;
          if (isRewinding)
            m_impl->mididev->Reset();

          AllMIDINoteOff();

          m_impl->midi_seq->GoToTime( isRewinding && m_impl->doc ? m_impl->doc->m_firstNoteAppearClk : m_impl->clk_play_start_time );
          pretend_clock_time = m_impl->midi_seq->GetCurrentTimeInMs();

          if (isRewinding)
          m_impl->midi_seq->GoToTime( 0 );

          if ( !m_impl->midi_seq->GetNextEventTimeMs( &next_event_time ) ) {
            m_impl->request_stop_midi = true;
            break;
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
        m_impl->mididev->UpdateMIDITicks();

        // find all events that came before or a the current time
        while ( next_event_time <= pretend_clock_time ) {
          if ( m_impl->midi_seq->GetNextEvent( &ev_track, &msg ) )
          {
            // found the event!
            // show it to stdout
            //printf( "PUSH: tm=%06.0f : evtm=%06.0f :trk%02d : \n", pretend_clock_time, next_event_time, ev_track );

            auto trackit = m_impl->doc->m_tracks.find(ev_track);
            if (trackit != m_impl->doc->m_tracks.end()) {

              if (msg.IsTrackName()) {
                std::string track_name = msg.GetSysExString();
                trackit->second->name = track_name;
                fmt::println("DEBUG: TRACK {} CHANNEL: {} NAME: {}", ev_track, msg.GetChannel(), track_name);
              }

              if (msg.IsControlChange()) {
                trackit->second->SetBank(&msg);
                trackit->second->SetVolume(&msg);
                trackit->second->SetPan(&msg);
                trackit->second->SetFilter(&msg);
              }

              if (msg.IsProgramChange()) {
                if (trackit->second->ch == msg.GetChannel()+1) {
                  trackit->second->pg = msg.GetPGValue()+1;
                  fmt::println("DEBUG: TRACK {} CHANNEL: {} BANK: {:04X}h PG:{}", ev_track, msg.GetChannel(),
                    trackit->second->bank, trackit->second->pg);
                }

                for (const auto & kv: m_impl->doc->m_tracks) {
                  if (trackit->first == kv.second->id) continue; // already set
                  if (kv.second->ch == msg.GetChannel() + 1) {
                    kv.second->pg = msg.GetPGValue()+1;
                  }
                }
              }
            }

            double shiftMs = m_impl->midi_seq->GetCurrentTimeInMs() - pretend_clock_time;
            if (!m_impl->mididev->HardwareMsgOut( msg, &shiftMs )) {
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
    return !m_impl->request_stop_midi;
    //return m_impl->th_play_midi_running;
  }

  bool DawSeq::IsRewinding() {
    if (m_impl->is_rewinding) {
      m_impl->is_rewinding = false;
      return true;
    }
    return false;
  }

  // TODO: refactor
  DawTrack * DawSeq::GetTrack(int track_num) {
    return m_impl->doc->GetTrack(track_num);
  }

  void DawSeq::SetTSFDevice(TinySoundFontDevice * dev) {
    m_impl->tsfdev = dev;
    m_impl->mididev = (BaseMidiOutDevice*)dev;
  }

  void DawSeq::SetBASSDevice(BassMidiDevice * dev) {
    m_impl->bassdev = dev;
  }

  void DawSeq::RenderMIDICallback(uint8_t * stream, int len) {
    const int blockSize = 64;
    const int sizeOfStereoSample = (2 * sizeof(float)); // stereo is 2 output channels
    //Number of samples to process
    int SampleCount  = (len / sizeOfStereoSample);
    int BlockCount   = (SampleCount / blockSize);
    int SampleMarker = SampleCount - ((BlockCount-1)*blockSize);
    int LastSampleMarker = 0;

static float pretend_clock_time = 0;  // TODO
static float next_event_time = 0; // TODO
static float starting_time = 0;
static long processing_samples = 0;
    jdksmidi::MIDITimedBigMessage msg;
    int ev_track;
    const float samplePeriodMs = 1000.0/(float)m_impl->mididev->GetAudioSampleRate();

    if (m_impl->midi_seq && m_impl->th_play_midi_running && !m_impl->request_stop_midi) {
      // request to update cursor
      if (m_impl->read_clk_play_start) {
        bool isRewinding = m_impl->clk_play_start_time == 0;
        if (isRewinding)
          m_impl->mididev->Reset();

        AllMIDINoteOff();

        m_impl->midi_seq->GoToTime( isRewinding && m_impl->doc ? m_impl->doc->m_firstNoteAppearClk : m_impl->clk_play_start_time );
        pretend_clock_time = m_impl->midi_seq->GetCurrentTimeInMs();

        if (isRewinding)
          m_impl->midi_seq->GoToTime( 0 );

        if ( !m_impl->midi_seq->GetNextEventTimeMs( &next_event_time ) ) {
          m_impl->request_stop_midi = true;
        }

        starting_time = pretend_clock_time;
        processing_samples = 0;

        m_impl->read_clk_play_start = false;
      }

      // Update display cursor position
      CalcCurrentMIDITimeBeat(pretend_clock_time);
    }

    // Process MIDI events per Block
    for (;BlockCount;--BlockCount) {
      int SampleBlock = SampleMarker - LastSampleMarker;

      // If Processing MIDI events
      if (m_impl->midi_seq && m_impl->th_play_midi_running && !m_impl->request_stop_midi) {
        processing_samples += SampleBlock;
        pretend_clock_time = starting_time + (processing_samples * samplePeriodMs);

        // OTHER update pretend clock strategy is using increments, but
        // disadvantage of floating point may cause lost precision for looooongest duration
        // pretend_clock_time += SampleBlock * samplePeriodMs;

        while ( next_event_time <= pretend_clock_time ) {
          if ( m_impl->midi_seq->GetNextEvent( &ev_track, &msg ) )
          {
            auto trackit = m_impl->doc->m_tracks.find(ev_track);
            if (trackit != m_impl->doc->m_tracks.end()) {

              if (msg.IsTrackName()) {
                std::string track_name = msg.GetSysExString();
                trackit->second->name = track_name;
                fmt::println("DEBUG: TRACK {} CHANNEL: {} NAME: {}", ev_track, msg.GetChannel(), track_name);
              }

              if (msg.IsControlChange()) {
                trackit->second->SetBank(&msg);
                trackit->second->SetVolume(&msg);
                trackit->second->SetPan(&msg);
                trackit->second->SetFilter(&msg);
              }

              if (msg.IsProgramChange()) {
                if (trackit->second->ch == msg.GetChannel()+1) {
                  trackit->second->pg = msg.GetPGValue()+1;
                  fmt::println("DEBUG: TRACK {} CHANNEL: {} BANK: {:04X}h PG:{}", ev_track, msg.GetChannel()+1,
                    trackit->second->bank, trackit->second->pg);
                }

                for (const auto & kv: m_impl->doc->m_tracks) {
                  if (trackit->first == kv.second->id) continue; // already set
                  if (kv.second->ch == msg.GetChannel() + 1) {
                    kv.second->pg = msg.GetPGValue()+1;
                  }
                }
              }
            }

            if (!m_impl->mididev->HardwareMsgOut( msg, nullptr )) {
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
      }

      m_impl->tsfdev->RenderStereoFloat((float*)stream, SampleBlock);

      stream += (SampleBlock * sizeOfStereoSample);

      LastSampleMarker = SampleMarker;
      SampleMarker += blockSize;
    }

    if (m_impl->request_stop_midi && m_impl->th_play_midi_running) {
      AllMIDINoteOff();
      m_impl->th_play_midi_running = false;
      m_impl->request_stop_midi = false;
    }
  }

  TinySoundFontDevice * DawSeq::GetTSFDevice() {
    return m_impl->tsfdev;
  }
}