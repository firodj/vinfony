#include "DawSeq.hpp"

#include <iostream>
#include <thread>
#include <atomic>
//#include <chrono>
#include <memory>
#include <thread>

#include <SDL.h>

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
  class DawNotifier : public jdksmidi::MIDISequencerGUIEventNotifier
  {
  public:
    /// The constructor. The class will send text messages to the FILE *f
    DawNotifier(): en( true ), f(stdout)
    {
    }

    /// The destructor
    virtual ~DawNotifier()
    {
    }

    virtual void Notify( const jdksmidi::MIDISequencer *seq, jdksmidi::MIDISequencerGUIEvent e );

    virtual bool GetEnable() const
    {
      return en;
    }

    virtual void SetEnable( bool f )
    {
      en = f;
    }

  private:
      FILE * f;
      bool en;
  };

  void DawNotifier::Notify( const jdksmidi::MIDISequencer *seq, jdksmidi::MIDISequencerGUIEvent e )
  {
    if ( en )
    {
      if ( e.GetEventGroup() == jdksmidi::MIDISequencerGUIEvent::GROUP_ALL )
      {
        fprintf( f, "GUI RESET\n" );
      }
      else if ( e.GetEventGroup() == jdksmidi::MIDISequencerGUIEvent::GROUP_TRANSPORT )
      {
        if ( e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_TRANSPORT_BEAT )
        {
          fprintf( f, "MEAS %3d BEAT %3d MIDICLOCK:%6lu\n",
            seq->GetCurrentMeasure() + 1,
            seq->GetCurrentBeat() + 1,
            seq->GetCurrentMIDIClockTime() );
        }
      }

      else if ( e.GetEventGroup() == jdksmidi::MIDISequencerGUIEvent::GROUP_CONDUCTOR )
      {
        if ( e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_CONDUCTOR_TIMESIG )
        {
          fprintf( f,
                    "TIMESIG: %d/%d\n",
                    seq->GetState()->timesig_numerator,  /* NC */
                    seq->GetState()->timesig_denominator /* NC */
                  );
        }

        if ( e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_CONDUCTOR_TEMPO )
        {
          fprintf( f, "TEMPO: %3.2f\n", seq->GetState()->tempobpm /* NC */ );
        }
        if ( /* NC new */
          e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_CONDUCTOR_KEYSIG )
        {
          fprintf( f, "KEYSIG: \n" ); /* NC : TODO: fix this */
        }
        if ( /* NC new */
          e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_CONDUCTOR_MARKER )
        {
          fprintf( f, "MARKER: %s\n", seq->GetState()->marker_name );
        }
      }
      else if ( e.GetEventGroup() == jdksmidi::MIDISequencerGUIEvent::GROUP_TRACK ) /* NC: NEW */
      {
        if ( e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_TRACK_NAME )
        {
          fprintf(
            f, "TRACK %2d NAME: %s\n", e.GetEventSubGroup(), seq->GetTrackState( e.GetEventSubGroup() )->track_name );
        }
        if ( e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_TRACK_PG )
        {
          fprintf( f, "TRACK %2d PROGRAM: %d\n", e.GetEventSubGroup(), seq->GetTrackState( e.GetEventSubGroup() )->pg );
        }
        if ( /* NC new */
          e.GetEventItem() == jdksmidi::MIDISequencerGUIEvent::GROUP_TRACK_VOLUME )
        {
          fprintf(
            f, "TRACK %2d VOLUME: %d\n", e.GetEventSubGroup(), seq->GetTrackState( e.GetEventSubGroup() )->volume );
        }
        // GROUP_TRACK_NOTE ignored!
      }
      else
      {
        fprintf( f, "GUI EVENT: G=%d, SG=%d, ITEM=%d\n", e.GetEventGroup(), e.GetEventSubGroup(), e.GetEventItem() );
      }
    }
  }

  struct DawSeq::Impl {
    //std::unique_ptr<jdksmidi::MIDIFileReadStreamFile> rs{};
    std::unique_ptr<jdksmidi::MIDIMultiTrack> tracks{};
    std::unique_ptr<jdksmidi::MIDISequencer> seq{};
    std::thread th_read_midi_file{};
    std::atomic<bool> th_read_midi_file_done{false};
    std::atomic<bool> th_read_midi_file_running{false};
    bool midi_file_loaded{};
    std::thread th_play_midi{};
    std::atomic<bool> th_play_midi_running{false};
    std::atomic<bool> request_stop_midi{false};
    std::unique_ptr<DawNotifier> notifier{};
    std::atomic<float> midi_time_beat{0};
  };

  DawSeq::DawSeq() {
    m_impl = std::make_unique<Impl>();
  }

  DawSeq::~DawSeq() {
    if (m_impl->th_read_midi_file.joinable())
        m_impl->th_read_midi_file.join();
    StopMIDI();
  }

  bool DawSeq::IsFileLoaded() {
    if (m_impl->th_read_midi_file_done) {
      m_impl->th_read_midi_file_done = false;

      if (m_impl->th_read_midi_file.joinable())
        m_impl->th_read_midi_file.join();
    }

    return m_impl->th_read_midi_file_running ? false : m_impl->midi_file_loaded;
  }

  bool DawSeq::ReadMIDIFile(std::string filename) {
    jdksmidi::MIDIFileReadStreamFile rs(filename.c_str());

    m_impl->tracks = std::make_unique<jdksmidi::MIDIMultiTrack>();

    jdksmidi::MIDIFileReadMultiTrack track_loader( m_impl->tracks.get() );

    jdksmidi::MIDIFileRead reader( &rs, &track_loader );

    if ( !reader.Parse() )
    {
      fmt::println("Error parse file {}", filename);
      return false;
    }

    m_impl->notifier = std::make_unique<DawNotifier>();
    m_impl->seq      = std::make_unique<jdksmidi::MIDISequencer>( m_impl->tracks.get(), m_impl->notifier.get() );
    fmt::println("Clocks per beat = {}", m_impl->tracks->GetClksPerBeat());

    return true;
  }

  // from: AdvancedSequencer::GetCurrentMIDIClockTime
  void DawSeq::CalcCurrentMIDITimeBeat(uint64_t now)
  {
    jdksmidi::MIDIClockTime time = m_impl->seq->GetCurrentMIDIClockTime();
    double ms_offset = now - m_impl->seq->GetCurrentTimeInMs();
    double ms_per_clock = 60000.0 / ( m_impl->seq->GetState()->tempobpm * m_impl->seq->GetCurrentTempoScale() * m_impl->tracks->GetClksPerBeat() );
    time += ( jdksmidi::MIDIClockTime )( ms_offset / ms_per_clock );
    m_impl->midi_time_beat = (float)time/m_impl->tracks->GetClksPerBeat();
  }

  void DawSeq::AsyncReadMIDIFile(std::string filename) {
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

  float DawSeq::GetMIDITimeBeat() {
    return m_impl->midi_time_beat;
  }

  void DawSeq::AsyncPlayMIDI() {
    if (m_impl->th_play_midi_running) return;

    m_impl->th_play_midi_running = true;
    m_impl->th_play_midi = std::thread([this]() {
      std::shared_ptr<void> _(nullptr, [this](...) {
        m_impl->th_play_midi_running = false;
      });

      float pretend_clock_time = 0.0;
      float next_event_time = 0.0;
      jdksmidi::MIDITimedBigMessage msg;
      int ev_track;

      auto dev = CreateTsfDev();
      if (!dev->Init()) return;

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
        while ( next_event_time <= pretend_clock_time )
        {
          if ( m_impl->seq->GetNextEvent( &ev_track, &msg ) )
          {
            // found the event!
            // show it to stdout
            //fprintf( stdout, "tm=%06.0f : evtm=%06.0f :trk%02d : ", pretend_clock_time, next_event_time, ev_track );
            if (!dev->HardwareMsgOut( msg )) return;
            // now find the next message

            if ( !m_impl->seq->GetNextEventTimeMs( &next_event_time ) )
            {
              // no events left so end
              //fprintf( stdout, "End\n" );
              return;
            }
          }
        }

        // 10ms when using namespace std::chrono_literals;
        SDL_Delay(10);
        if (m_impl->request_stop_midi) break;
      }

      dev->Shutdown();
      m_impl->request_stop_midi = false;
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
}