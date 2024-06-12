#pragma once

#include <string>

// Forward declarations
namespace jdksmidi {
  class MIDISystemExclusive;
};

namespace vinfony {

enum SysxVendor {
  SYSX_NONE, SYSX_GM, SYSX_GS, SYSX_XG
};

class BaseSysEx {
public:
  BaseSysEx(const jdksmidi::MIDISystemExclusive *midi_sysex) : m_midi_sysex(*midi_sysex), m_sysxVendor(SYSX_NONE) {};
  virtual ~BaseSysEx() {};
  virtual std::string Info() = 0;
  virtual void Parse() = 0;
  static BaseSysEx * Create(const jdksmidi::MIDISystemExclusive *midi_sysex);

  SysxVendor m_sysxVendor;

protected:
  const jdksmidi::MIDISystemExclusive m_midi_sysex;
};

class GMSysEx: public BaseSysEx {
public:
  GMSysEx(const jdksmidi::MIDISystemExclusive *midi_sysex): BaseSysEx(midi_sysex) { m_sysxVendor = SYSX_GM; Parse(); };

  bool IsGMReset();
  void Parse() override;
  std::string Info() override;

private:
  unsigned char m_devID{}, m_subID1{}, m_subID2{};
  int m_eoxAt{};
};

class GSSysEx: public BaseSysEx {
public:
  enum {
    CMD_RQ1 = 0x11,
    CMD_DT1 = 0x12,

    MDL_GS = 0x42,
  };

  GSSysEx(const jdksmidi::MIDISystemExclusive *midi_sysex): BaseSysEx(midi_sysex) { m_sysxVendor = SYSX_GS; Parse(); };

  void Parse() override;
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

  std::string Info() override;

private:
  unsigned char m_devID{}, m_mdlID{}, m_cmdID{}, m_chkSum;
  unsigned int m_cmdAddr{};
  int m_chkHash{};
  const unsigned char *m_buf;
  int m_bufLen;
};

class XGSysEx: public BaseSysEx {
public:
  XGSysEx(const jdksmidi::MIDISystemExclusive *midi_sysex): BaseSysEx(midi_sysex) { m_sysxVendor = SYSX_XG; Parse(); };

  std::string Info() override;
  void Parse() override;
  bool IsXGReset();
  size_t GetDataLen() const;
  unsigned char GetData(int i) const;

private:
  unsigned char m_devID{}, m_mdlID{}, m_chkSum;
  unsigned int m_cmdAddr{};
  int m_chkHash{};
  const unsigned char *m_buf;
  int m_bufLen;
};

};
