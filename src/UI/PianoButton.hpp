#pragma once

#include "hscpp/module/Tracker.h"
#include "hscpp/mem/Ref.h"

struct ImVec2;

namespace vinfony {

struct PianoButtonTempState;

class PianoButton {

	HSCPP_TRACK(PianoButton, "PianoButton");

public:
	hscpp_virtual ~PianoButton();
	PianoButton();

	void Creating();
	void Destroying();

	hscpp_virtual void DrawH();
	hscpp_virtual void DrawV();
	hscpp_virtual bool DrawRegion(const char *label, int start, int stop, int center, bool selected);

	int m_dbgMouseX, m_dbgMouseY;

protected:

	int PianoCheckPointH(ImVec2 & point);
	int PianoCheckPointV(ImVec2 & point);
	void PrepareDraw();

private:

	std::unique_ptr<PianoButtonTempState> m_tempState;
};

};
