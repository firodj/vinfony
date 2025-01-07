#pragma once

#include "IDawTrackNotes.hpp"
#include "DawNote.hpp"

namespace vinfony {

class DawTrackNotes: public IDawTrackNotes {
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
  IDawTrackNotes *m_derived;

  DawTrackNotes();
  void NoteOn(long t, char n, char v) override;
  void DumpDbg();
  void NoteOff(long t, char n) override;
  void ClipOff(long t) override;

  bool NewNote(long t, unsigned char n) override;
  bool KillNote(long t, unsigned char n, bool destroy) override;
  void DrawNote(int slot) override;
  void Reset();
  void ResetStats() override;
  DawNote* GetNoteActive(int slot) override { return &note_actives[slot]; };
	int GetNoteValueToSlot(unsigned char n) override { return note_value_to_slot[n]; };
  int GetNoteProcessed() override { return notes_processed; }
  void SetDerived(IDawTrackNotes *derived) override { m_derived = derived; };
};

};
