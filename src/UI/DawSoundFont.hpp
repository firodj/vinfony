#pragma once

#include <memory>
#include <string>
#include <functional>

#include "hscpp/module/PreprocessorMacros.h" // Added so macros are available when using a tracked class.
#include "hscpp/module/Tracker.h"
#include "hscpp/mem/Ref.h"

struct tsf;

namespace vinfony {

class PianoButton;

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

	const char *GetCurveTypeNames(int type) {
		const char * curveTypes[] = {"Linear", "Concave", "Convex", "Switch"};
		return curveTypes[type % 4];
	};

	int m_selectedId;
	int m_selectedType;
	int m_regionSelected;
	float m_heightSamples;
	hscpp::mem::UniqueRef<PianoButton> m_pPianoButton;
};

};