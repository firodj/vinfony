#include "DawSeq.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

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
    std::unique_ptr<jdksmidi::MIDIMultiTrack> tracks{};
    std::unique_ptr<jdksmidi::MIDISequencer> seq{};
    std::thread th_read_midi_file{};
    std::atomic<bool> th_read_midi_file_done{false};
    std::atomic<bool> th_read_midi_file_running{false};
    bool midi_file_loaded{};
    std::thread th_play_midi{};
    std::atomic<bool> th_play_midi_running{false};
    std::atomic<bool> request_stop_midi{false};
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

    m_impl->seq = std::make_unique<jdksmidi::MIDISequencer>( m_impl->tracks.get() );

    if ( !reader.Parse() )
    {
      fmt::println("Error parse file {}", filename);
      return false;
    }

    return true;
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

      jdksmidi::MIDISequencer seq( m_impl->tracks.get() );
      seq.GoToTimeMs( pretend_clock_time );

      if ( !seq.GetNextEventTimeMs( &next_event_time ) )
      {
        return;
      }

      // simulate a clock going forward with 10 ms resolution for 1 hour
      float max_time = 3600. * 1000.;
      const auto start = std::chrono::high_resolution_clock::now();
      for ( ; pretend_clock_time < max_time; pretend_clock_time = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - start).count())
      {
        // find all events that came before or a the current time
        while ( next_event_time <= pretend_clock_time )
        {
          if ( seq.GetNextEvent( &ev_track, &msg ) )
          {
            // found the event!
            // show it to stdout
            fprintf( stdout, "tm=%06.0f : evtm=%06.0f :trk%02d : ", pretend_clock_time, next_event_time, ev_track );
            if (!dev->HardwareMsgOut( msg )) return;
            // now find the next message

            if ( !seq.GetNextEventTimeMs( &next_event_time ) )
            {
              // no events left so end
              fprintf( stdout, "End\n" );
              return;
            }
          }
        }
        // 10ms when using namespace std::chrono_literals;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (m_impl->request_stop_midi) break;
      }

      dev->Shutdown();
      m_impl->request_stop_midi = false;
    });
  }

  void DawSeq::StopMIDI() {
    if (m_impl->th_play_midi_running) {
      m_impl->request_stop_midi = true;
      if (m_impl->th_play_midi.joinable()) {
        m_impl->th_play_midi.join();
      }
    }
  }
}