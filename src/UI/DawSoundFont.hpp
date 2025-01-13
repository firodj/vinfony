#pragma once

#include <memory>
#include <string>
#include <functional>

#include "hscpp/module/PreprocessorMacros.h" // Added so macros are available when using a tracked class.
#include "hscpp/module/Tracker.h"

struct tsf;

namespace vinfony {

// DawSoundFont
class DawSoundFont {

	HSCPP_TRACK(DawSoundFont, "DawSoundFont");

public:
	hscpp_virtual ~DawSoundFont();
    DawSoundFont();
	hscpp_virtual void Update();

	void Draw(tsf *pTinySoundFont);

	void Creating();
    void Destroying();
};

};