#include "jdksmidi/world.h"
#include "jdksmidi/midi.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"

#include <tml.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <memory>
#include <map>

#include <fmt/core.h>
#include <fmt/color.h>

#include <libassert/assert.hpp>

#include "DawTrackNotes.hpp"

namespace vinfony {

DawTrackNotes::DawTrackNotes() {
  Reset();
  ResetStats();
}

void DawTrackNotes::Reset() {
  num_free_slot = 128;

  note_used_head = -1;
  note_used_tail = -1;
  visible_start_clk = 0;

  for (int i=0; i<128; i++) {
    note_free_slot[i] = (128 - 1) - i;
    note_value_to_slot[i] = -1;
  }
}

void DawTrackNotes::ResetStats() {
  notes_processed = 0;
  notes_to_draw = 0;
  notes_to_hide = 0;
}

bool DawTrackNotes::NewNote(long t, char n) {
  notes_processed++;

  if (note_value_to_slot[n] != -1) return false;
  if (num_free_slot == 0) return false;
  --num_free_slot;
  int slot = note_free_slot[num_free_slot];
  note_free_slot[num_free_slot] = -1;
  note_value_to_slot[n] = slot;

  auto &note_active = note_actives[slot];
  note_active.time = t;
  note_active.stop = -1;
  note_active.note = n;
  note_active.used_prev = -1;
  note_active.used_next = -1;

  if (note_used_head == -1) {
    note_used_head = slot;
    note_used_tail = slot;
  } else {
    auto &note_used_prev = note_actives[note_used_tail];
    note_used_prev.used_next = slot;
    note_active.used_prev = note_used_tail;
    note_used_tail = slot;
  }

  return true;
}

void DawTrackNotes::NoteOn(long t, char n, char v) {
  NoteOff(t, n);
  if (v > 0) NewNote(t, n);
}

void DawTrackNotes::DumpDbg() {
  fmt::println("free = {}, used = {}", num_free_slot, 128-num_free_slot);
  fmt::print("note slot:\t");
  for (int n = 0; n<128; n++) {
    if (note_value_to_slot[n] != -1)
    fmt::print("{} -> {}\t", n, note_value_to_slot[n]);
  }
  fmt::print("\nfree slot:\t");
  for (int i = 0; i<128; i++) {
    if (note_free_slot[i] != -1)
    fmt::print("{} -> {}\t", i, note_free_slot[i]);
  }

  fmt::println("\nhead = {}, tail = {}\n", note_used_head, note_used_tail);
  DawNote dummy{};
  dummy.used_next = -1;
  auto * note_active = &dummy;
  for (int slot = note_used_head; slot != -1; slot = note_active->used_next) {
    note_active = &note_actives[slot];
    fmt::println("\t{} -> {} -> {};",  note_active->used_prev, slot, note_active->used_next );
  }

  fmt::println("");
}

void DawTrackNotes::NoteOff(long t, char n) {
  KillNote(t, n, true);
}

bool DawTrackNotes::KillNote(long t, char n, bool destroy) {
  int slot = note_value_to_slot[n];
  if (slot == -1) return false;
  auto &note_active = note_actives[slot];

  DEBUG_ASSERT(note_active.stop == -1);
  note_active.stop = t;
  DEBUG_ASSERT((int)note_active.note == (int)n);
  note_value_to_slot[n] = -1;

  DrawNote(slot);

  if (destroy) {
    if (note_active.used_prev != -1) {
      auto &note_used_prev = note_actives[note_active.used_prev];
      note_used_prev.used_next = note_active.used_next;
    } else {
      DEBUG_ASSERT(note_used_head == slot);
      note_used_head = note_active.used_next;
    }

    if (note_active.used_next != -1) {
      auto &note_used_next = note_actives[note_active.used_next];
      note_used_next.used_prev = note_active.used_prev;
    } else {
      DEBUG_ASSERT(note_used_tail == slot);
      note_used_tail = note_active.used_prev;
    }

    DEBUG_ASSERT(num_free_slot < 128);
    DEBUG_ASSERT(note_free_slot[num_free_slot] == -1 );
    note_free_slot[num_free_slot] = slot;
    num_free_slot++;
  }

  return true;
}

void DawTrackNotes::ClipOff(long t) {
  DawNote dummy{};
  dummy.used_next = -1;
  auto * note_active = &dummy;
  for (int slot = note_used_head; slot != -1; slot = note_active->used_next) {
    note_active = &note_actives[slot];

    DEBUG_ASSERT(note_value_to_slot[note_active->note] != -1);
    KillNote(t, note_active->note, false);
  }
  Reset();
}

void DawTrackNotes::DrawNote(int slot) {
  if (note_actives[slot].stop >= visible_start_clk) {
#if 0
    float y0 = (track_h * (float)(127 - note_actives[slot].note)/ 128);
    assert (note_actives[slot].note >= 0 && note_actives[slot].note <= 127);
    float nh = (float)track_h/128;
    float x0 = (note_actives[slot].time - visible_start_clk);
    float x1 = (note_actives[slot].stop - visible_start_clk);

    x0 = x0 * uiStyle->beatWd / displayState->ppqn;
    x1 = x1 * uiStyle->beatWd / displayState->ppqn;

    ImVec2 p1{scrnpos_x + x0, scrnpos_y + y0};
    ImVec2 p2{scrnpos_x + x1, scrnpos_y + y0};
    if (nh >= 1.0) {
      p2.y  += nh;
      draw_list->AddRectFilled(p1, p2, IM_COL32(255,64,64, 255));
    } else {
      draw_list->AddLine(p1, p2, IM_COL32(255,64,64, 255));
    }
#endif
    notes_to_draw++;
  } else {
    notes_to_hide++;
  }
}

}