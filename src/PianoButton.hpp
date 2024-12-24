#pragma once

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

void PianoButton(const char *label, PianoButtonStyle * style = nullptr);
void PianoButtonV(const char *label, PianoButtonStyle * style = nullptr);
bool PianoRegion(const char *label, int start, int stop, int center, bool selected);
};
