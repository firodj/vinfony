#include "PianoButton.hpp"
#include <vector>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

#include "hscpp/module/Tracker.h"

#include "../globals.hpp"

hscpp_require_include_dir("${projPath}/ext/imgui-docking")
hscpp_require_include_dir("${projPath}/ext/hscpp/extensions/mem/include")

hscpp_if (os == "Windows")
	hscpp_require_library("${buildPath}/kosongg/cmake/imgui/Debug/imgui.lib")
hscpp_elif (os == "Posix")
	//hscpp_require_library("${buildPath}/Debug/libimgui.a")
	hscpp_require_library("${projPath}/bin/libimgui.dylib")
hscpp_else()
	// Diagnostic messages can be printed to the build output with hscpp_message.
	hscpp_message("Unknown OS ${os}.")
hscpp_end()

namespace vinfony {

struct PianoButtonStyle {
	float whiteWidth = 12;
	float whiteHeight = 60;
	float blackWidth = 8;
	float blackHeight = 40;
	bool equalize = true;
};

struct PianoConfig {
	PianoButtonStyle style;

	float width_all;
	float width_octave;

	float tuts_start[12];
	float tuts_end[12];
	int	tuts_type[12];
};

struct PianoState {
	PianoConfig * refConfig;
	int noteOn;
};

struct PianoButtonTempState {
	PianoConfig m_defaultPianoConfig;
	std::vector<PianoState> m_pianos;
};

/*
PianoButtonStyle DefaultPianoButtonStyle() {
	return PianoButtonStyle{
		12,
		60,
		8,
		40,
		true,
	};
}
*/

/**
  InitDefaultPianoConfig

  tuts_type:

  ----------+
            | E  -> 2 (white tuts after black)
  ]]]]]] ---| D# -> 3 (black tuts)
            | D  -> 1 (white tuts between black)
  ]]]]]] ---| C# -> 3 (black tuts)
            | C  -> 0 (white tuts before black)
  ----------+

  Note: no white tuts without adjacent black tuts.
 */
void InitDefaultPianoConfig(PianoConfig & m_defaultPianoConfig)
{

	PianoButtonStyle & style = m_defaultPianoConfig.style;
	float & width_all        = m_defaultPianoConfig.width_all;
	float & width_octave     = m_defaultPianoConfig.width_octave;

	float * tuts_start = m_defaultPianoConfig.tuts_start;
	float * tuts_end   = m_defaultPianoConfig.tuts_end;
	int * tuts_type    = m_defaultPianoConfig.tuts_type;

	// calculate all needed size
	//style = new_style ? *new_style : DefaultPianoButtonStyle();

	const float midWhite = (1+m_defaultPianoConfig.style.whiteWidth) - style.blackWidth/2;

	if (style.equalize) {
		style.blackWidth = style.whiteWidth;
		width_octave = 12 * style.whiteWidth;
		width_all = 120 * style.whiteWidth;
	} else {
		width_octave = 7 * style.whiteWidth;
		width_all =	(10 * width_octave) +
								(5 * style.whiteWidth);
	}

	if (style.equalize) {
		for (int  i=0; i<12; i++) {
			tuts_start[i] = style.whiteWidth*i;
			tuts_end[i] =tuts_start[i] + style.whiteWidth;
		}
	} else

	{
		const float x[12] = {	0+style.whiteWidth*0,
							midWhite+style.whiteWidth*0,
							0+style.whiteWidth*1,
							midWhite+style.whiteWidth*1,
							0+style.whiteWidth*2,
							0+style.whiteWidth*3,
							midWhite+style.whiteWidth*3,
							0+style.whiteWidth*4,
							midWhite+style.whiteWidth*4,
							0+style.whiteWidth*5,
							midWhite+style.whiteWidth*5,
							0+style.whiteWidth*6};
		const float y[12] = {	x[0]+style.whiteWidth,
							x[1]+style.blackWidth,
							x[2]+style.whiteWidth,
							x[3]+style.blackWidth,
							x[4]+style.whiteWidth,
							x[5]+style.whiteWidth,
							x[6]+style.blackWidth,
							x[7]+style.whiteWidth,
							x[8]+style.blackWidth,
							x[9]+style.whiteWidth,
							x[10]+style.blackWidth,
							x[11]+style.whiteWidth};
		memcpy(tuts_start, x, sizeof(x));
		memcpy(tuts_end, y, sizeof(y));
	}

	const int z[12] = { 0,3,1,3,2, 0,3,1,3,1,3,2};
	memcpy(tuts_type, z, sizeof(z));
}

int PianoButton::PianoCheckPointH(ImVec2 & point)
{
	PianoState & current = m_tempState->m_pianos.back();
	auto & pianoCfg = *current.refConfig;

	// outer piano area
	if (point.y >= pianoCfg.style.whiteHeight ||
		point.x >= pianoCfg.width_all)
		return -1;
	if (point.y < 0 || point.x < 0)
		return -1;

	// check tuts number;
	int n_octave = point.x / pianoCfg.width_octave;
	// check tuts hitam;
	int n_x = (int)point.x % (int)pianoCfg.width_octave;
	int i;
	// check black note first,
	if (point.y < pianoCfg.style.blackHeight || pianoCfg.style.equalize)
		for (i = 0; i<12; i++)
			if (pianoCfg.tuts_type[i] == 3)
				if ( n_x >= pianoCfg.tuts_start[i] &&
					n_x <  pianoCfg.tuts_end[i] ) {
					int n = i+(n_octave*12);
					return n > 127 ? 127 : n;
				}
	// nothing b4, then check in white note
	for (i=0; i<12; i++)
		if (pianoCfg.tuts_type[i] < 3)
			if ( n_x >= pianoCfg.tuts_start[i] &&
				n_x <  pianoCfg.tuts_end[i] ) {
				int n = i+(n_octave*12);
				return n > 127 ? 127 : n;
			}
	// something wrong happen, give bad result
	return -1;
}

PianoButton::PianoButton()
{
	auto cb = [this](hscpp::SwapInfo& info) {
		//info.Save("defaultPianoConfig", m_defaultPianoConfig);
	};

	Hscpp_SetSwapHandler(cb);

	if (Hscpp_IsSwapping()) {
		return;
	}

	Creating();
}

PianoButton::~PianoButton()
{
	if (Hscpp_IsSwapping())
	{
		return;
	}

	Destroying();
}

void PianoButton::Creating()
{

}

void PianoButton::Destroying() {

}

void PianoButton::DrawH()
{
	PrepareDraw();

	m_tempState->m_pianos.emplace_back();
	PianoState & current = m_tempState->m_pianos.back();

	current.refConfig = & m_tempState->m_defaultPianoConfig;
	current.noteOn = -1;

	auto & pianoCfg = *current.refConfig;

	ImVec2 pmin = ImGui::GetCursorScreenPos();
	ImVec2 pmax = pmin + ImVec2{pianoCfg.width_all, pianoCfg.style.whiteHeight};
	ImGui::InvisibleButton("##button", ImVec2{pianoCfg.width_all, pianoCfg.style.whiteHeight});
	ImVec2 mousePos;
	int noteOn = -1;
	if (ImGui::IsItemHovered()) {
		mousePos = ImGui::GetMousePos() - pmin;
		noteOn = PianoCheckPointH(mousePos);
	}
	auto drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(pmin, pmax, IM_COL32_WHITE);

	ImVec2 ptuts = pmin;
	if (noteOn != -1 && pianoCfg.tuts_type[noteOn % 12] != 3) {
		ptuts = pmin + ImVec2{ pianoCfg.width_octave * (noteOn/12), 0 } + ImVec2{pianoCfg.tuts_start[noteOn%12], 0.0};
		drawList->AddRectFilled(ptuts, ptuts + ImVec2{pianoCfg.style.whiteWidth, pianoCfg.style.whiteHeight}, IM_COL32(255, 0,0, 255));
	}

	ptuts = pmin;
	for (int i=0; i<128; i++) {
		if (i && ((i%12) == 0)) pmin += ImVec2{ pianoCfg.width_octave, 0 };
		ptuts = pmin + ImVec2{pianoCfg.tuts_start[i%12], 0.0};
		switch (pianoCfg.tuts_type[i % 12]) {
		case 3:
			drawList->AddRectFilled(ptuts, ptuts + ImVec2{pianoCfg.style.blackWidth, pianoCfg.style.blackHeight}, i == noteOn ? IM_COL32(255, 0,0, 255) : IM_COL32_BLACK);
			if (pianoCfg.style.equalize)
				drawList->AddLine(ptuts + ImVec2{pianoCfg.style.blackWidth/2, pianoCfg.style.blackHeight}, ptuts + ImVec2{pianoCfg.style.blackWidth/2, pianoCfg.style.whiteHeight}, IM_COL32_BLACK);
			break;
		default:
			if (!pianoCfg.style.equalize || pianoCfg.tuts_type[(i+1) % 12] != 3)
				drawList->AddLine(ptuts + ImVec2{pianoCfg.style.whiteWidth, 0}, ptuts + ImVec2{pianoCfg.style.whiteWidth, pianoCfg.style.whiteHeight}, IM_COL32_BLACK);
		}
	}
}

int PianoButton::PianoCheckPointV(ImVec2 & point)
{
	PianoState & current = m_tempState->m_pianos.back();
	auto & pianoCfg = *current.refConfig;

	point.y = pianoCfg.width_all - point.y;
	// outer piano area
	if (point.x >= pianoCfg.style.whiteHeight ||
		point.y >= pianoCfg.width_all)
		return -1;
	if (point.y < 0 || point.x < 0)
		return -1;

	// check tuts number;
	int n_octave = point.y / pianoCfg.width_octave;
	// check tuts hitam;
	int n_x = (int)point.y % (int)pianoCfg.width_octave;
	int i;
	// check black note first,
	if (point.x < pianoCfg.style.blackHeight || pianoCfg.style.equalize)
		for (i = 0; i<12; i++)
			if (pianoCfg.tuts_type[i] == 3)
				if ( n_x >= pianoCfg.tuts_start[i] &&
					n_x <  pianoCfg.tuts_end[i] ) {
					int n = i+(n_octave*12);
					return n > 127 ? 127 : n;
				}
	// nothing b4, then check in white note
	for (i=0; i<12; i++)
		if (pianoCfg.tuts_type[i] < 3)
			if ( n_x >= pianoCfg.tuts_start[i] &&
				n_x <  pianoCfg.tuts_end[i] ) {
				int n = i+(n_octave*12);
				return n > 127 ? 127 : n;
			}
	// something wrong happen, give bad result
	return -1;
}

void PianoButton::PrepareDraw()
{
	ImGui::SetCurrentContext( Globals::Resolve()->pImGuiContext );
	if (!m_tempState) {
		m_tempState = std::make_unique<PianoButtonTempState>();
		InitDefaultPianoConfig(m_tempState->m_defaultPianoConfig);
	}
}

void PianoButton::DrawV()
{
	PrepareDraw();

	m_tempState->m_pianos.emplace_back();
	PianoState & current = m_tempState->m_pianos.back();

	current.refConfig = & m_tempState->m_defaultPianoConfig;
	current.noteOn = -1;

	auto & pianoCfg = *current.refConfig;

	ImVec2 pmin = ImGui::GetCursorScreenPos();
	ImVec2 pmax = pmin + ImVec2{pianoCfg.style.whiteHeight, pianoCfg.width_all};
	ImGui::InvisibleButton("##pianobutton", ImVec2{pianoCfg.style.whiteHeight, pianoCfg.width_all});
	ImVec2 pstart = ImVec2{pmin.x, pmax.y};

	if (ImGui::IsItemHovered()) {

		ImVec2 mousePos = ImGui::GetMousePos() - pmin;

		m_dbgMouseX = mousePos.x;
		m_dbgMouseY = mousePos.y;

		current.noteOn = PianoCheckPointV(mousePos);
	}

	// Draw white for all tuts.
	ImGui::GetWindowDrawList()->AddRectFilled(pmin, pmax, IM_COL32_WHITE);

	ImVec2 ptuts = pstart;
	if (current.noteOn != -1 && pianoCfg.tuts_type[current.noteOn % 12] != 3) {
		ptuts = pstart - ImVec2{ 0, pianoCfg.width_octave * (current.noteOn/12) } - ImVec2{ 0.0, pianoCfg.tuts_start[current.noteOn%12]};
		ImGui::GetWindowDrawList()->AddRectFilled(ptuts, ptuts + ImVec2{pianoCfg.style.whiteHeight, -pianoCfg.style.whiteWidth}, IM_COL32(255, 0,0, 255));
	}

	ptuts = pstart;
	for (int i=0; i<128; i++) {
		if (i && ((i%12) == 0)) pstart -= ImVec2{ 0, pianoCfg.width_octave };
		ptuts = pstart - ImVec2{0.0, pianoCfg.tuts_start[i%12]};

		switch (pianoCfg.tuts_type[i % 12]) {
		case 3:
			ImGui::GetWindowDrawList()->AddRectFilled(ptuts, ptuts + ImVec2{pianoCfg.style.blackHeight, -pianoCfg.style.blackWidth}, i == current.noteOn ? IM_COL32(255, 0,0, 255) : IM_COL32_BLACK);
			if (pianoCfg.style.equalize)
				ImGui::GetWindowDrawList()->AddLine(ptuts + ImVec2{ pianoCfg.style.blackHeight, -pianoCfg.style.blackWidth/2 }, ptuts + ImVec2{ pianoCfg.style.whiteHeight, -pianoCfg.style.blackWidth/2 }, IM_COL32_BLACK);
			break;
		default:
			if (!pianoCfg.style.equalize || pianoCfg.tuts_type[(i+1) % 12] != 3)
				ImGui::GetWindowDrawList()->AddLine(ptuts - ImVec2{ 0.0, pianoCfg.style.whiteWidth }, ptuts + ImVec2{ pianoCfg.style.whiteHeight, -pianoCfg.style.whiteWidth }, IM_COL32_BLACK);
		}

		ImGui::GetWindowDrawList()->AddText(ptuts, IM_COL32_BLACK, "pwp");
	}

	//PianoState & current = m_tempState->m_pianos.back();

	m_tempState->m_pianos.pop_back();
}

bool PianoButton::DrawRegion(const char *label, int start, int stop, int center, bool selected)
{
	PrepareDraw();

	PianoConfig & pianoCfg = m_tempState->m_defaultPianoConfig;

	float regionH = pianoCfg.style.whiteWidth;
	ImVec2 pmin = ImGui::GetCursorScreenPos();
	ImVec2 pstart = pmin;
	ImVec2 pstop = pmin;
	ImVec2 pcenter = pmin;
	float rcenter  = pianoCfg.tuts_type[center%12] == 3 ? pianoCfg.style.blackWidth : pianoCfg.style.whiteWidth;
	rcenter = (rcenter-2)/2;

	float ts = pianoCfg.tuts_start[start%12];
	if (pianoCfg.tuts_type[start%12] != 3 && start>0 && pianoCfg.tuts_type[(start-1)%12] == 3) {
		ts = pianoCfg.tuts_end[(start-1)%12];
	}
	pstart += ImVec2{ (start/12 * pianoCfg.width_octave) + ts, 0 };

	float te = pianoCfg.tuts_end[stop%12];
	if (pianoCfg.tuts_type[stop%12] != 3 && stop<127 && pianoCfg.tuts_type[(stop+1)%12] == 3) {
		te = pianoCfg.tuts_start[(stop+1)%12];
	}

	pstop += ImVec2{ (stop/12 * pianoCfg.width_octave)  + te, 0 };

	float tc = (pianoCfg.tuts_start[center%12] + pianoCfg.tuts_end[center%12])/2;
	pcenter += ImVec2{ (center/12 * pianoCfg.width_octave) + tc, (regionH)/2 };

	ImGui::PushID(label);
	ImColor regionColor = selected ? IM_COL32(255, 128, 0, 255) : IM_COL32(0, 0,255, 255);
	//bool clicked = ImGui::InvisibleButton("##button", ImVec2{pianoCfg.width_all, regionH + 2});
	ImGui::SetCursorScreenPos(pstart);
	bool clicked = ImGui::InvisibleButton("##button", pstop + ImVec2{0, regionH} - pstart);
	bool hovered = ImGui::IsItemHovered();
	if (hovered) {
		regionColor = IM_COL32(255,0,0, 255);
	}

	ImGui::GetWindowDrawList()->AddRect(pstart, pstop + ImVec2{0, regionH}, regionColor);
	if (selected || hovered) {
		ImGui::GetWindowDrawList()->AddCircle(pcenter + ImVec2{1.0, 1.0}, rcenter, regionColor);
	}

	ImGui::PopID();
	return clicked;
}

}