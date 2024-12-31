#pragma once

namespace vinfony {

class DawSeqI {
public:
    DawSeqI() {};
    virtual ~DawSeqI() {}

    virtual bool IsFileLoaded() = 0;
    virtual void SetMIDITimeBeat(float time_beat) = 0;
    virtual void AsyncPlayMIDI() = 0;
    virtual void AsyncPlayMIDIStopped() = 0;
    virtual void StopMIDI() = 0;
    virtual void GetCurrentMBT(int &m, int &b, int &t) = 0;
    virtual float GetTempoBPM() = 0;
};

};