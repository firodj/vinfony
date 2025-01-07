#pragma once

#include <string>

namespace jdksmidi {

class MIDITrack;

}

namespace vinfony {

class IDawTrack {
public:
    virtual ~IDawTrack() = default;

    virtual const char * GetName() = 0;
    virtual int GetId() = 0;
    virtual unsigned int & GetCh() = 0;
    virtual unsigned int GetPg() = 0;
    virtual unsigned int GetBank() = 0;
    virtual int GetDrumPart() = 0;

    virtual int & GetMidiVolume() = 0;
    virtual int & GetMidiPan() = 0;
    virtual int & GetMidiFilterFc() = 0;
    virtual int & GetMidiFilterQ() = 0;

    virtual int & GetH() = 0;

    virtual int & GetViewcacheStartEventNum() = 0;
	virtual long & GetViewcacheStartVisibleClk() = 0;

    virtual jdksmidi::MIDITrack * GetMidiTrack() = 0;
};

};