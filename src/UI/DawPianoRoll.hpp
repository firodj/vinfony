#pragma once

#include "hscpp/module/PreprocessorMacros.h" // Added so macros are available when using a tracked class.
#include "hscpp/module/Tracker.h"
#include "hscpp/mem/Ref.h"

namespace vinfony
{

class PianoButton;

class DawPianoRoll
{

	HSCPP_TRACK(DawPianoRoll, "DawPianoRoll");

public:
	hscpp_virtual ~DawPianoRoll();
    DawPianoRoll();
	hscpp_virtual void Update();

	void Creating();
    void Destroying();

	hscpp::mem::UniqueRef<PianoButton> m_pPianoButton;
};

};
