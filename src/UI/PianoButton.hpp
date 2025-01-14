#pragma once

#include "hscpp/module/Tracker.h"
#include "hscpp/mem/Ref.h"

namespace vinfony {

struct PianoButtonStyle {
  float whiteWidth;
  float whiteHeight;
  float blackWidth;
  float blackHeight;
  bool equalize;
};

struct DrawPianoRollParam {
    unsigned int texPiano;
    int pianoWidth;
    int pianoHeight;
};

class PianoButtonState {

public:

  PianoButtonState(PianoButtonStyle * style = nullptr);

  PianoButtonStyle style;

	float width_all;
	float width_octave;

	float tuts_start[12];
	float tuts_end[12];
	int	tuts_type[12];
  int tuts_color[14];
};

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

private:

  PianoButtonState m_szPiano;
};

};
