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

DawTrackNotes::DawTrackNotes(): m_derived(this) {
  Reset();
  ResetStats();
}

void DawTrackNotes::Reset() {
  num_free_slot = 128;

  note_used_head = -1;
  note_used_tail = -1;

#if 0
  for (int i=0; i<128; i++) {
    note_free_slot[i] = (128 - 1) - i;
    note_value_to_slot[i] = -1;
  }
#else
  static bool initialize = true;
  static int noteFreeSlotInit[128];
  static int noteValueToSlotInit[128];
  if (initialize) {
    initialize = false;
    for (int i=0; i<128; i++) {
      noteFreeSlotInit[i] = (128 - 1) - i;
      noteValueToSlotInit[i] = -1;
    }
  }
  memcpy(note_free_slot,     noteFreeSlotInit,    sizeof(note_free_slot));
  memcpy(note_value_to_slot, noteValueToSlotInit, sizeof(note_value_to_slot));
#endif
}

void DawTrackNotes::ResetStats() {
  notes_processed = 0;
}

bool DawTrackNotes::NewNote(long t, unsigned char n) {
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
#if 0
  if (dbg_notesDisOrder) {
    int slot = note_value_to_slot[n];
    if (slot != -1) {
      fmt::print(fmt::fg(fmt::color::wheat), "WARN: Note On before Kill! t{},n{}\n", t, (int)n);
    }
  }
#endif
  m_derived->NoteOff(t, n);
  if (v > 0) m_derived->NewNote(t, n);
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
#if 0
  if (dbg_notesDisOrder) {
    int slot = note_value_to_slot[n];
    if (slot == -1) {
      fmt::print(fmt::fg(fmt::color::wheat), "WARN: Note Off nothing! t{},n{}\n", t, (int)n);
    }
  }
#endif
  m_derived->KillNote(t, n, true);
}

bool DawTrackNotes::KillNote(long t, unsigned char n, bool destroy) {
  int slot = note_value_to_slot[n];
  if (slot == -1) return false;
  auto &note_active = note_actives[slot];

  DEBUG_ASSERT(note_active.stop == -1);
  note_active.stop = t;
  DEBUG_ASSERT((int)note_active.note == (int)n);
  note_value_to_slot[n] = -1;

  m_derived->DrawNote(slot);

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
    m_derived->KillNote(t, note_active->note, false);
  }
  Reset();
}

void DawTrackNotes::DrawNote(int slot) {
  fmt::println("TODO: DrawNote %d\n", slot);
}

}