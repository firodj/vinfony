#include "PianoButton.hpp"
#include <vector>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

namespace vinfony {

struct PianoButtonState {
  PianoButtonStyle style;

	float width_all;
	float width_octave;

	float tuts_start[12];
	float tuts_end[12];
	int	tuts_type[12];
  int tuts_color[14];
};

static std::vector<PianoButtonState> g_pianoButtonStates;
static ImGuiID g_lastPianoButtonID{0};

PianoButtonStyle DefaultPianoButtonStyle() {
  return PianoButtonStyle{
    12,
    60,
    8,
    40,
    true,
  };
}

PianoButtonState & PianoButtonStateGet(int * pInOutIDX, PianoButtonStyle * style) {
  if (*pInOutIDX != -1) return g_pianoButtonStates[*pInOutIDX];

  *pInOutIDX = g_pianoButtonStates.size();
  g_pianoButtonStates.push_back(PianoButtonState{});
  PianoButtonState & m_szPiano = g_pianoButtonStates[*pInOutIDX];

  // calculate all needed size
  if (style) m_szPiano.style = *style; else m_szPiano.style = DefaultPianoButtonStyle();

	const float midWhite = (1+m_szPiano.style.whiteWidth) - m_szPiano.style.blackWidth/2;

  if (m_szPiano.style.equalize) {
    m_szPiano.style.blackWidth = m_szPiano.style.whiteWidth;
    m_szPiano.width_octave = 12 * m_szPiano.style.whiteWidth;
    m_szPiano.width_all = 120 * m_szPiano.style.whiteWidth;
  } else {
	  m_szPiano.width_octave = 7 * m_szPiano.style.whiteWidth;
    m_szPiano.width_all =	(10 * m_szPiano.width_octave) +
                (5 * m_szPiano.style.whiteWidth);
  }

  if (m_szPiano.style.equalize) {
    for (int  i=0; i<12; i++) {
      m_szPiano.tuts_start[i] = m_szPiano.style.whiteWidth*i;
      m_szPiano.tuts_end[i] =m_szPiano.tuts_start[i] + m_szPiano.style.whiteWidth;
    }
  } else

  {
    const float x[12] = {	0+m_szPiano.style.whiteWidth*0,
              midWhite+m_szPiano.style.whiteWidth*0,
              0+m_szPiano.style.whiteWidth*1,
              midWhite+m_szPiano.style.whiteWidth*1,
              0+m_szPiano.style.whiteWidth*2,
              0+m_szPiano.style.whiteWidth*3,
              midWhite+m_szPiano.style.whiteWidth*3,
              0+m_szPiano.style.whiteWidth*4,
              midWhite+m_szPiano.style.whiteWidth*4,
              0+m_szPiano.style.whiteWidth*5,
              midWhite+m_szPiano.style.whiteWidth*5,
              0+m_szPiano.style.whiteWidth*6};
    const float y[12] = {	x[0]+m_szPiano.style.whiteWidth,
              x[1]+m_szPiano.style.blackWidth,
              x[2]+m_szPiano.style.whiteWidth,
              x[3]+m_szPiano.style.blackWidth,
              x[4]+m_szPiano.style.whiteWidth,
              x[5]+m_szPiano.style.whiteWidth,
              x[6]+m_szPiano.style.blackWidth,
              x[7]+m_szPiano.style.whiteWidth,
              x[8]+m_szPiano.style.blackWidth,
              x[9]+m_szPiano.style.whiteWidth,
              x[10]+m_szPiano.style.blackWidth,
              x[11]+m_szPiano.style.whiteWidth};
    memcpy(m_szPiano.tuts_start, x, sizeof(x));
    memcpy(m_szPiano.tuts_end, y, sizeof(y));
  }

	const int z[12] = { 0,3,1,3,2, 0,3,1,3,1,3,2};
	memcpy(m_szPiano.tuts_type, z, sizeof(z));


  return m_szPiano;
}

int PianoCheckPoint(PianoButtonState & m_szPiano, ImVec2 point) {
// outer piano area
	if (point.y >= m_szPiano.style.whiteHeight ||
		point.x >= m_szPiano.width_all)
		return -1;
	if (point.y < 0 || point.x < 0)
		return -1;

	// check tuts number;
	int n_octave = point.x / m_szPiano.width_octave;
	// check tuts hitam;
	int n_x = (int)point.x % (int)m_szPiano.width_octave;
	int i;
	// check black note first,
	if (point.y < m_szPiano.style.blackHeight || m_szPiano.style.equalize)
		for (i = 0; i<12; i++)
			if (m_szPiano.tuts_type[i] == 3)
				if ( n_x >= m_szPiano.tuts_start[i] &&
					n_x <  m_szPiano.tuts_end[i] ) {
          int n = i+(n_octave*12);
          return n > 127 ? 127 : n;
        }
	// nothing b4, then check in white note
	for (i=0; i<12; i++)
		if (m_szPiano.tuts_type[i] < 3)
			if ( n_x >= m_szPiano.tuts_start[i] &&
				n_x <  m_szPiano.tuts_end[i] ) {
			  int n = i+(n_octave*12);
        return n > 127 ? 127 : n;
      }
	// something wrong happen, give bad result
	return -1;
}

void PianoButton(const char *label, PianoButtonStyle * style) {
  ImGuiID pianoID = ImGui::GetID(label);
  int *pianoStorageID = ImGui::GetStateStorage()->GetIntRef(pianoID, -1);
  PianoButtonState & m_szPiano = PianoButtonStateGet(pianoStorageID, style);
  g_lastPianoButtonID = pianoID;

  ImVec2 pmin = ImGui::GetCursorScreenPos();
  ImVec2 pmax = pmin + ImVec2{m_szPiano.width_all, m_szPiano.style.whiteHeight};
  ImGui::InvisibleButton("##button", ImVec2{m_szPiano.width_all, m_szPiano.style.whiteHeight});
  ImVec2 mousePos;
  int noteOn = -1;
  if (ImGui::IsItemHovered()) {
    mousePos = ImGui::GetMousePos() - pmin;
    noteOn = PianoCheckPoint(m_szPiano, mousePos);
  }
  auto drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(pmin, pmax, IM_COL32_WHITE);

  ImVec2 ptuts = pmin;
  if (noteOn != -1 && m_szPiano.tuts_type[noteOn % 12] != 3) {
    ptuts = pmin + ImVec2{ m_szPiano.width_octave * (noteOn/12), 0 } + ImVec2{m_szPiano.tuts_start[noteOn%12], 0.0};
    drawList->AddRectFilled(ptuts, ptuts + ImVec2{m_szPiano.style.whiteWidth, m_szPiano.style.whiteHeight}, IM_COL32(255, 0,0, 255));
  }

  ptuts = pmin;
  for (int i=0; i<128; i++) {
    if (i && ((i%12) == 0)) pmin += ImVec2{ m_szPiano.width_octave, 0 };
    ptuts = pmin + ImVec2{m_szPiano.tuts_start[i%12], 0.0};
    switch (m_szPiano.tuts_type[i % 12]) {
    case 3:
      drawList->AddRectFilled(ptuts, ptuts + ImVec2{m_szPiano.style.blackWidth, m_szPiano.style.blackHeight}, i == noteOn ? IM_COL32(255, 0,0, 255) : IM_COL32_BLACK);
      if (m_szPiano.style.equalize)
        drawList->AddLine(ptuts + ImVec2{m_szPiano.style.blackWidth/2, m_szPiano.style.blackHeight}, ptuts + ImVec2{m_szPiano.style.blackWidth/2, m_szPiano.style.whiteHeight}, IM_COL32_BLACK);
      break;
    default:
      if (!m_szPiano.style.equalize || m_szPiano.tuts_type[(i+1) % 12] != 3)
        drawList->AddLine(ptuts + ImVec2{m_szPiano.style.whiteWidth, 0}, ptuts + ImVec2{m_szPiano.style.whiteWidth, m_szPiano.style.whiteHeight}, IM_COL32_BLACK);
    }
  }
}

int PianoCheckPointV(PianoButtonState & m_szPiano, ImVec2 point) {
// outer piano area
	if (point.x >= m_szPiano.style.whiteHeight ||
		point.y >= m_szPiano.width_all)
		return -1;
	if (point.y < 0 || point.x < 0)
		return -1;

	// check tuts number;
	int n_octave = point.y / m_szPiano.width_octave;
	// check tuts hitam;
	int n_x = (int)point.y % (int)m_szPiano.width_octave;
	int i;
	// check black note first,
	if (point.x < m_szPiano.style.blackHeight || m_szPiano.style.equalize)
		for (i = 0; i<12; i++)
			if (m_szPiano.tuts_type[i] == 3)
				if ( n_x >= m_szPiano.tuts_start[i] &&
					n_x <  m_szPiano.tuts_end[i] ) {
          int n = i+(n_octave*12);
          return n > 127 ? 127 : n;
        }
	// nothing b4, then check in white note
	for (i=0; i<12; i++)
		if (m_szPiano.tuts_type[i] < 3)
			if ( n_x >= m_szPiano.tuts_start[i] &&
				n_x <  m_szPiano.tuts_end[i] ) {
			  int n = i+(n_octave*12);
        return n > 127 ? 127 : n;
      }
	// something wrong happen, give bad result
	return -1;
}

void PianoButtonV(const char *label, PianoButtonStyle * style) {
  ImGuiID pianoID = ImGui::GetID(label);
  int *pianoStorageID = ImGui::GetStateStorage()->GetIntRef(pianoID, -1);
  PianoButtonState & m_szPiano = PianoButtonStateGet(pianoStorageID, style);
  g_lastPianoButtonID = pianoID;

  ImVec2 pmin = ImGui::GetCursorScreenPos();
  ImVec2 pmax = pmin + ImVec2{m_szPiano.style.whiteHeight, m_szPiano.width_all};
  ImGui::InvisibleButton("##button", ImVec2{m_szPiano.style.whiteHeight, m_szPiano.width_all});
  ImVec2 mousePos;
  int noteOn = -1;
  if (ImGui::IsItemHovered()) {
    mousePos = ImGui::GetMousePos() - pmin;
    noteOn = PianoCheckPointV(m_szPiano, mousePos);
  }
  auto drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(pmin, pmax, IM_COL32_WHITE);

  ImVec2 ptuts = pmin;
  if (noteOn != -1 && m_szPiano.tuts_type[noteOn % 12] != 3) {
    ptuts = pmin + ImVec2{ 0, m_szPiano.width_octave * (noteOn/12) } + ImVec2{ 0.0, m_szPiano.tuts_start[noteOn%12]};
    drawList->AddRectFilled(ptuts, ptuts + ImVec2{m_szPiano.style.whiteHeight, m_szPiano.style.whiteWidth}, IM_COL32(255, 0,0, 255));
  }

  ptuts = pmin;
  for (int i=0; i<128; i++) {
    if (i && ((i%12) == 0)) pmin += ImVec2{ 0, m_szPiano.width_octave };
    ptuts = pmin + ImVec2{0.0, m_szPiano.tuts_start[i%12]};
    switch (m_szPiano.tuts_type[i % 12]) {
    case 3:
      drawList->AddRectFilled(ptuts, ptuts + ImVec2{m_szPiano.style.blackHeight, m_szPiano.style.blackWidth}, i == noteOn ? IM_COL32(255, 0,0, 255) : IM_COL32_BLACK);
      if (m_szPiano.style.equalize)
        drawList->AddLine(ptuts + ImVec2{ m_szPiano.style.blackHeight, m_szPiano.style.blackWidth/2 }, ptuts + ImVec2{ m_szPiano.style.whiteHeight, m_szPiano.style.blackWidth/2 }, IM_COL32_BLACK);
      break;
    default:
      if (!m_szPiano.style.equalize || m_szPiano.tuts_type[(i+1) % 12] != 3)
        drawList->AddLine(ptuts + ImVec2{ 0.0, m_szPiano.style.whiteWidth }, ptuts + ImVec2{ m_szPiano.style.whiteHeight, m_szPiano.style.whiteWidth }, IM_COL32_BLACK);
    }
  }
}

bool PianoRegion(const  char *label, int start, int stop, int center, bool selected) {
  if (g_lastPianoButtonID == 0) return false;
  int *pianoStorageID = ImGui::GetStateStorage()->GetIntRef(g_lastPianoButtonID, -1);
  if (*pianoStorageID == -1) return false;
  PianoButtonState & m_szPiano = PianoButtonStateGet(pianoStorageID, nullptr);

  float regionH = m_szPiano.style.whiteWidth;
  ImVec2 pmin = ImGui::GetCursorScreenPos();
  ImVec2 pstart = pmin;
  ImVec2 pstop = pmin;
  ImVec2 pcenter = pmin;
  float rcenter  = m_szPiano.tuts_type[center%12] == 3 ? m_szPiano.style.blackWidth : m_szPiano.style.whiteWidth;
  rcenter = (rcenter-2)/2;

  float ts = m_szPiano.tuts_start[start%12];
  if (m_szPiano.tuts_type[start%12] != 3 && start>0 && m_szPiano.tuts_type[(start-1)%12] == 3) {
    ts = m_szPiano.tuts_end[(start-1)%12];
  }
  pstart += ImVec2{ (start/12 * m_szPiano.width_octave) + ts, 0 };

  float te = m_szPiano.tuts_end[stop%12];
  if (m_szPiano.tuts_type[stop%12] != 3 && stop<127 && m_szPiano.tuts_type[(stop+1)%12] == 3) {
    te = m_szPiano.tuts_start[(stop+1)%12];
  }

  pstop += ImVec2{ (stop/12 * m_szPiano.width_octave)  + te, 0 };

  float tc = (m_szPiano.tuts_start[center%12] + m_szPiano.tuts_end[center%12])/2;
  pcenter += ImVec2{ (center/12 * m_szPiano.width_octave) + tc, (regionH)/2 };

  ImGui::PushID(label);
  ImColor regionColor = selected ? IM_COL32(255, 128, 0, 255) : IM_COL32(0, 0,255, 255);
  //bool clicked = ImGui::InvisibleButton("##button", ImVec2{m_szPiano.width_all, regionH + 2});
  ImGui::SetCursorScreenPos(pstart);
  bool clicked = ImGui::InvisibleButton("##button", pstop + ImVec2{0, regionH} - pstart);
  bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    regionColor = IM_COL32(255,0,0, 255);
  }
  auto drawList = ImGui::GetWindowDrawList();
  drawList->AddRect(pstart, pstop + ImVec2{0, regionH}, regionColor);

  if (selected || hovered) {
    drawList->AddCircle(pcenter + ImVec2{1.0, 1.0}, rcenter, regionColor);
  }

  ImGui::PopID();
  return clicked;
}

}