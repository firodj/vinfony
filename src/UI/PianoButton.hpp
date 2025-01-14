#pragma once

#include "hscpp/module/Tracker.h"

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

public:
  PianoButton();

  void DrawH(const char *label, PianoButtonStyle * style = nullptr);
  void DrawV(const char *label, PianoButtonStyle * style = nullptr);
  bool DrawRegion(const char *label, int start, int stop, int center, bool selected);

private:

  PianoButtonState m_szPiano;
};

};
