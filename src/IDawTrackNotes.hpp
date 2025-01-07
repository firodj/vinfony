#pragma once

namespace vinfony
{
struct DawNote;

class IDawTrackNotes {
public:
	virtual ~IDawTrackNotes() = default;

	virtual bool NewNote(long t, unsigned char n) = 0;
	virtual bool KillNote(long t, unsigned char n, bool destroy) = 0;
	virtual void DrawNote(int slot) = 0;
	virtual DawNote* GetNoteActive(int slot) = 0;
	virtual int GetNoteValueToSlot(unsigned char n) = 0;
	virtual void ResetStats() = 0;

	virtual void NoteOn(long t, char n, char v) = 0;
	virtual void NoteOff(long t, char n) = 0;
	virtual void ClipOff(long t) = 0;
	virtual int GetNoteProcessed() = 0;

	virtual void SetDerived(IDawTrackNotes *derived) = 0;
};

};