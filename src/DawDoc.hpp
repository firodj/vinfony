#pragma once

#include <map>
#include <memory>

#include "DawSeq.hpp"
#include "DawTrackNotes.hpp"

// Forward declarations
namespace jdksmidi {
  class MIDIMultiTrack;
};

namespace vinfony {

class DawDoc {
public:
  DawDoc();
  ~DawDoc();

  std::map<int, std::unique_ptr<DawTrack>> m_tracks;
  //int m_lastTrackNum{0};
  std::vector<int> m_trackNums;
  std::unique_ptr<jdksmidi::MIDIMultiTrack> m_midiMultiTrack;
  int m_uiDefaultTrackHeight{128};
  unsigned long m_firstNoteAppearClk{0};

  DawTrack * AddNewTrack(int midi_track_id, jdksmidi::MIDITrack * midi_track);
  bool LoadFromMIDIMultiTrack( jdksmidi::MIDIMultiTrack *mlt );
  jdksmidi::MIDIMultiTrack * GetMIDIMultiTrack() { return m_midiMultiTrack.get(); }
  int GetPPQN();
  DawTrack * GetTrack(int track_num);
  int GetNumTracks();
};

};
