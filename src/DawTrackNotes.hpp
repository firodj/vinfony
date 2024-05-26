#pragma once

#include "DawSeq.hpp"

namespace vinfony {

struct DawNote {
  long time;
  long stop;
  char note;
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
  long visible_start_clk;

  // UI
#if 0
  int scrnpos_x{0};
  int scrnpos_y{0};
  int track_h{0};
  DawDisplayState * displayState {nullptr};
  ImDrawList * draw_list{nullptr};
  DawUIStyle * uiStyle {nullptr};
#endif

  // Statistics
  int notes_processed{0};
  int notes_to_draw{0};
  int notes_to_hide{0};

  DawTrackNotes();
  void NoteOn(long t, char n, char v) ;
  void DumpDbg();
  void NoteOff(long t, char n);
  void ClipOff(long t);

  virtual void DrawNote(int slot);
  void Reset();
  void ResetStats();

protected:
  bool NewNote(long t, char n);
  bool KillNote(long t, char n, bool destroy);
};

};
