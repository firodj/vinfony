#include "jdksmidi/world.h"
#include "jdksmidi/midi.h"
#include "jdksmidi/track.h"
#include "jdksmidi/multitrack.h"
#include "jdksmidi/filereadmultitrack.h"
#include "jdksmidi/fileread.h"
#include "jdksmidi/fileshow.h"
#include "jdksmidi/sequencer.h"
#include "RtMidi/RtMidi.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <memory>
#include <map>

#include <fmt/core.h>
#include <fmt/color.h>

#include <libassert/assert.hpp>

#include "DawSysEx.hpp"

namespace vinfony {

// GMSysEx
GMSysEx::GMSysEx(const jdksmidi::MIDISystemExclusive *midi_sysex): m_midi_sysex(midi_sysex) {
  Parse();
};

GMSysEx * GMSysEx::Create(const jdksmidi::MIDISystemExclusive *midi_sysex) {
  if (midi_sysex->GetLength() < 5) return nullptr;
  if (midi_sysex->GetData(0) == 0x7E)
    return new GMSysEx(midi_sysex);
  return nullptr;
}

bool GMSysEx::IsGMReset() {
  return (m_devID == 0x7F &&
    m_subID1 == 0x09 &&
    m_subID2 == 0x01 &&
    m_eoxAt == 5);
}

void GMSysEx::Parse() {
  m_devID  = m_midi_sysex->GetData(1);
  m_subID1 = m_midi_sysex->GetData(2);
  m_subID2 = m_midi_sysex->GetData(3);

  m_eoxAt = m_midi_sysex->GetData( m_midi_sysex->GetLength() - 1 ) == 0xF7 ? m_midi_sysex->GetLength() : 0;
}

std::string GMSysEx::Info() {
  if (IsGMReset()) {
    return "GM Reset";
  }
}

// GSSysEx
GSSysEx::GSSysEx(const  jdksmidi::MIDISystemExclusive *midi_sysex): m_midi_sysex(midi_sysex) {
  Parse();
};

GSSysEx * GSSysEx::Create(const jdksmidi::MIDISystemExclusive *midi_sysex) {
  if (midi_sysex->GetLength() < 10) return nullptr;
  if (midi_sysex->GetData(0) == 0x41)
    return new GSSysEx(midi_sysex);
  return nullptr;
}

void GSSysEx::Parse() {
  m_devID  = m_midi_sysex->GetData(1);
  m_mdlID  = m_midi_sysex->GetData(2);
  m_cmdID  = m_midi_sysex->GetData(3); // 0x11=RQ1 / 0x12=DT1
  m_cmdAddr = ( m_midi_sysex->GetData(4) << 16 ) |
    ( m_midi_sysex->GetData(5) << 8 ) |
    ( m_midi_sysex->GetData(6) );
  m_chkHash = m_midi_sysex->GetData(4) + m_midi_sysex->GetData(5) + m_midi_sysex->GetData(6);
  m_buf = m_midi_sysex->GetBuf() + 7;
  m_bufLen = m_midi_sysex->GetLength() - 9;
}

bool GSSysEx::IsPartPatch() {
  return ((m_cmdAddr & 0xFF0000) == 0x400000 && (m_cmdAddr & 0x00F000) != 0);
}

bool GSSysEx::IsModelGS() {
  return MDL_GS == m_mdlID;
}

int GSSysEx::GetPart() {
  if (!IsPartPatch()) return 0;
  unsigned char part = (m_cmdAddr & 0x000F00) >> 8;
  return part == 0 ? 10 : part >= 10 ? part + 1 : part;
}

int GSSysEx::GetPartAddress() {
  if (!IsPartPatch()) return 0;
  return m_cmdAddr & 0x00F0FF;
}

unsigned char GSSysEx::GetData(int i) const {
  return m_buf[i];
}

size_t GSSysEx::GetDataLen() const {
  return m_bufLen;
}

bool GSSysEx::IsGSReset() {
  return IsModelGS() && IsDataSet() && m_cmdAddr == 0x40007F && GetDataLen() == 1 && GetData(0) == 0x00;
}

bool GSSysEx::IsDataSet() {
  return m_cmdID = CMD_DT1;
}

bool GSSysEx::IsRequestData() {
  return m_cmdID = CMD_RQ1;
}

bool GSSysEx::IsSystemMode() {
  return IsModelGS() && IsDataSet() && m_cmdAddr == 0x00007F && GetDataLen() == 1;
}

bool GSSysEx::IsUseRhythmPart() {
  if (!IsPartPatch()) return false;
  return GetPartAddress() == 0x1015;
}

int GSSysEx::GetUseRhythmPart() {
  if (!IsUseRhythmPart()) return 0;
  return GetData(0);
}

unsigned char GSSysEx::GetSystemMode() {
  if (!IsSystemMode()) return 0;
  if (GetData(0) == 0) return 1;
  if (GetData(0) == 1) return 2;
  return 0;
}

std::string GSSysEx::Info() {
  if (IsGSReset()) {
    return "GS Reset";
  } else if (IsSystemMode()) {
    return fmt::format("System Mode {}", GetSystemMode());
  } else if (IsPartPatch()) {
    if (IsUseRhythmPart()) {
      return fmt::format("Part Patch {} Use Rhythm Part {}", GetPart(), GetUseRhythmPart());
    }
    return fmt::format("Part Patch {} Address {:04X}h", GetPart(), GetPartAddress());
  }
  return "??";
}

}