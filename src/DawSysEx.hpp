#pragma once

#include <string>

// Forward declarations
namespace jdksmidi {
  class MIDISystemExclusive;
};

namespace vinfony {

class GMSysEx {
public:
  GMSysEx(const jdksmidi::MIDISystemExclusive *midi_sysex);

  static GMSysEx * Create(const jdksmidi::MIDISystemExclusive *midi_sysex);

  bool IsGMReset();
  void Parse();
  std::string Info();

private:
  const jdksmidi::MIDISystemExclusive * m_midi_sysex;
  unsigned char m_devID{}, m_subID1{}, m_subID2{};
  int m_eoxAt{};
};

class GSSysEx {
public:
  enum {
    CMD_RQ1 = 0x11,
    CMD_DT1 = 0x12,

    MDL_GS = 0x42,
  };

  GSSysEx(const jdksmidi::MIDISystemExclusive *midi_sysex);
  static GSSysEx * Create(const jdksmidi::MIDISystemExclusive *midi_sysex);

  void Parse();
  bool IsPartPatch();
  bool IsModelGS();
  int GetPart();
  int GetPartAddress();
  unsigned char GetData(int i) const;
  size_t GetDataLen() const;
  bool IsGSReset();
  bool IsDataSet();
  bool IsRequestData();
  bool IsSystemMode();
  bool IsUseRhythmPart();
  int GetUseRhythmPart();
  unsigned char GetSystemMode();

  std::string Info();

private:
  const jdksmidi::MIDISystemExclusive * m_midi_sysex;
  unsigned char m_devID{}, m_mdlID{}, m_cmdID{}, m_chkSum;
  unsigned int m_cmdAddr{};
  int m_chkHash{};
  const unsigned char *m_buf;
  int m_bufLen;
};

};