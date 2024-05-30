#pragma once

#include "DawSeq.hpp"

namespace vinfony {

struct DawNote {
  long time;
  long stop;
  char note;
  int event_num;
  int used_prev;
  int used_next;
};

class DawTrackNotes {
public:
  int note_value_to_slot[128];
  DawNote note_actives[128];
  int note_free_slot[128];

  int num_free_slot;
  int note_used_head;
  int note_used_tail;

  // Statistics
  int notes_processed{0};
  bool dbg_notesDisOrder{false};

  DawTrackNotes();
  void NoteOn(long t, char n, char v) ;
  void DumpDbg();
  void NoteOff(long t, char n);
  void ClipOff(long t);

  virtual void DrawNote(int slot);
  void Reset();
  virtual void ResetStats();

protected:
  virtual bool NewNote(long t, char n);
  virtual bool KillNote(long t, char n, bool destroy);
};

};
