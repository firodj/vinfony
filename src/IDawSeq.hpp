#pragma once

#include <memory>

namespace vinfony {

class IDawTrack;
class IDawTrackNotes;
struct DawDisplayState;

class IDawSeq {
public:
    virtual ~IDawSeq() = default;

    virtual bool IsFileLoaded() = 0;
    virtual bool IsRewinding() = 0;
    virtual bool IsPlaying() = 0;

    virtual void SetMIDITimeBeat(float time_beat) = 0;
    virtual void AsyncPlayMIDI() = 0;
    virtual void AsyncPlayMIDIStopped() = 0;
    virtual void StopMIDI() = 0;
    virtual void GetCurrentMBT(int &m, int &b, int &t) = 0;
    virtual float GetTempoBPM() = 0;

    virtual IDawTrack * GetTrack(int track_num) = 0;
    virtual int GetNumTracks() = 0;

    virtual void SendVolume(int chan, unsigned short value) = 0;
    virtual void SendPan(int chan, unsigned short value) = 0;
    virtual void SendFilter(int chan, unsigned short valFc, unsigned short valQ) = 0;

    virtual DawDisplayState *GetDisplayState() = 0;

	virtual const char * GetStdProgramName(int pg) = 0;
	virtual const char * GetStdDrumName(int pg) = 0;

    virtual std::unique_ptr<IDawTrackNotes> CreateDawTrackNotes() = 0;
};

};