#include "DawMainProject.hpp"

#include <map>
#include <memory>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "imkosongg/ImKosongg.hpp"
#include <fmt/core.h>
#include "jdksmidi/track.h"
//#include <libassert/assert.hpp>

//#include "DawTrack.hpp"
//#include "DawTrackNotes.hpp"
#include "UI.hpp"
#include "IDawTrackNotes.hpp"
#include "DawNote.hpp"
#include "../Globals.hpp"

hscpp_require_include_dir("${projPath}/src")
hscpp_require_include_dir("${projPath}/kosongg/cpp")
hscpp_require_include_dir("${projPath}/ext/jdksmidi/include")
hscpp_require_include_dir("${projPath}/ext/imgui-docking")
hscpp_require_include_dir("${projPath}/ext/fmt/include")
hscpp_require_include_dir("${projPath}/ext/hscpp/extensions/mem/include")

hscpp_require_source("../../kosongg/cpp/Component.cpp")
hscpp_require_source("Splitter.cpp")

hscpp_if (os == "Windows")
	hscpp_require_library("${buildPath}/Debug/imgui.lib")
	hscpp_require_library("${buildPath}/ext/jdksmidi/Debug/jdksmidi.lib")
	hscpp_require_library("${buildPath}/ext/fmt/Debug/fmtd.lib")
hscpp_elif (os == "Posix")
	hscpp_require_library("${projPath}/bin/libimgui.dylib")
	hscpp_require_library("${projPath}/bin/libjdksmidi.dylib")
	hscpp_require_library("${projPath}/bin/libfmtd.dylib")
hscpp_else()
	// Diagnostic messages can be printed to the build output with hscpp_message.
	hscpp_message("Unknown OS ${os}.")
hscpp_end()

// Helper
namespace ImGui {
	ImVec2 GetScroll()
	{
		ImGuiWindow* window = GImGui->CurrentWindow;
		return window->Scroll;
	}
};

template <typename K, typename V>
struct nothrow_map : std::map<K, V> {
	nothrow_map() = default;
	nothrow_map(nothrow_map&& other) noexcept : std::map<K, V>(std::move(other)) {}
};

namespace vinfony {

struct DawUIStyle {
	int cursorWd     = 10;
	int beatWd       = 40;
	int leftPadding  = 40;
	int rightPadding = 40;
};

class DawTrackNotesUI: public IDawTrackNotes {
	public:

	// UI
	int scrnpos_x{0};
	int scrnpos_y{0};
	int track_h{0};
	//ImDrawList * draw_list{nullptr};
	DawDisplayState * displayState {nullptr};
	DawUIStyle * uiStyle {nullptr};
	long visible_start_clk;

	// Statistics
	int notes_to_draw{0};
	int notes_to_hide{0};
	int cur_event_num{-1};
	int start_show_event_num{-1};
	long start_show_event_clk{-1};

	std::unique_ptr<IDawTrackNotes> m_base;

	DawTrackNotesUI(IDawSeq * seq)
	{
		m_base = seq->CreateDawTrackNotes();
		m_base->SetDerived(this);
	}

	bool NewNote(long t, unsigned char n) override {
		if (!m_base->NewNote(t, n)) return false;
		int slot = m_base->GetNoteValueToSlot(n);
		if (slot == -1) return false;
		DawNote * note_active = m_base->GetNoteActive(slot);
		note_active->event_num = cur_event_num;
		return true;
	}

	bool KillNote(long t, unsigned char n, bool destroy) override {
		return m_base->KillNote(t, n, destroy);
	}

	void DrawNote(int slot) override {
		if (m_base->GetNoteActive(slot)->stop >= visible_start_clk) {
			auto drawList = ImGui::GetWindowDrawList();
			float y0 = (track_h * (float)(127 - m_base->GetNoteActive(slot)->note)/ 128);

			//assert (note_actives[slot].note >= 0 && note_actives[slot].note <= 127);

			float nh = (float)track_h/128;
			float x0 = (m_base->GetNoteActive(slot)->time - visible_start_clk);
			float x1 = (m_base->GetNoteActive(slot)->stop - visible_start_clk);

			x0 = x0 * uiStyle->beatWd / displayState->ppqn;
			x1 = x1 * uiStyle->beatWd / displayState->ppqn;
			if (x1 - x0 < 1.0f) x1 = x0 + 1.0f;

			ImVec2 p1{scrnpos_x + x0, scrnpos_y + y0};
			ImVec2 p2{scrnpos_x + x1, scrnpos_y + y0};
			if (nh >= 1.0) {
				p2.y  += nh;
				drawList->AddRectFilled(p1, p2, IM_COL32(255,64,64, 255));
			} else {
				drawList->AddLine(p1, p2, IM_COL32(255,64,64, 255));
			}

			if (start_show_event_clk == -1 || m_base->GetNoteActive(slot)->time <= start_show_event_clk) {
				if (start_show_event_num == -1 || m_base->GetNoteActive(slot)->event_num < start_show_event_num) {
					start_show_event_clk = m_base->GetNoteActive(slot)->time;
					start_show_event_num = m_base->GetNoteActive(slot)->event_num;
				}
			}

			notes_to_draw++;
		} else {
			notes_to_hide++;
		}
	}

	DawNote* GetNoteActive(int slot) override {
		return m_base->GetNoteActive(slot);
	}
	int GetNoteValueToSlot(unsigned char n) override {
		return m_base->GetNoteValueToSlot(n);
	}
	void ResetStats() override {
		return m_base->ResetStats();
	}
	void NoteOn(long t, char n, char v) override {
		return m_base->NoteOn(t, n, v);
	}
	void NoteOff(long t, char n) override {
		return m_base->NoteOff(t, n);
	}
	void ClipOff(long t) override {
		return m_base->ClipOff(t);
	}
	int GetNoteProcessed() override {
		return m_base->GetNoteProcessed();
	}
	void SetDerived(IDawTrackNotes *derived) override {
		(void)derived;
	}
};

struct DawMainStorage {
	float scroll_y = 0.0f;
	float scroll_x0 = 0.0f;
	float scroll_x1 = 0.0f;

	int last_props_id = 0;
	int scroll_animate = 0;
	float scroll_target = 0;
	bool is_cursor_dragging = false;
	float dragging_cursor_x = 0;

	// the issue here, because DawProp has std::function
	// see:
	// * https://stackoverflow.com/questions/25330716/move-only-version-of-stdfunction
	// * https://stackoverflow.com/questions/56940199/how-to-capture-a-unique-ptr-in-a-stdfunction
	nothrow_map<int, std::unique_ptr<DawProp>> props;
	std::vector<int> prop_nums;

	DawUIStyle uiStyle;
};

int NewProp(DawMainStorage & storage, std::string name, DawPropDrawFunc func) {
	storage.last_props_id++;
	storage.props.emplace(storage.last_props_id, std::make_unique<DawProp>());

	auto prop = storage.props[storage.last_props_id].get();
	prop->id = storage.last_props_id;
	prop->name = name;
	prop->w = 100;
	prop->DrawProp = func;

	storage.prop_nums.push_back(prop->id);

	return prop->id;
}

void InitDawMainStorage(DawMainStorage & storage) {
	int id;
	// Properties
	id = NewProp(storage, "No", [](DawPropDrawParam * param, IDawSeq *seq) {
		(void)seq;

		std::string label = fmt::format("{:3d}", param->r);
		auto txtSize = ImGui::CalcTextSize(label.c_str());
		ImGui::SetCursorPosX( ImGui::GetCursorPosX() + param->self->w - txtSize.x );
		ImGui::Text("%s", label.c_str());

	});
	storage.props[id]->w = 20;

	NewProp(storage, "Name", [](DawPropDrawParam * param, IDawSeq *seq) {
		(void)seq;

		ImGui::Text("%s", param->track->GetName() );
	});
	NewProp(storage, "Channel", [](DawPropDrawParam * param, IDawSeq *seq) {
		(void)seq;
#if 0
		auto p1 = ImGui::GetCursorScreenPos();
		auto p2 = p1 + ImVec2{(float)param->self->w, (float)param->track->h};
		ImGui::GetWindowDrawList()->AddRectFilled(p1, p2, IM_COL32(255, 0,0, 255));
#endif

		const char* items[] = {"-", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};

		ImGui::SetNextItemWidth(param->self->w);
		ImGui::PushID(param->track->GetId());
		ImGui::Combo("##channel", (int*)&param->track->GetCh(), items, IM_ARRAYSIZE(items));
		ImGui::PopID();
	});
	id = NewProp(storage, "Instrument", [](DawPropDrawParam * param, IDawSeq *seq) {
		(void)seq;

		if (param->track->GetPg()) {
			int grpdrum = param->track->GetDrumPart();
			if (!grpdrum)
				ImGui::Text("%s (%04Xh : %d) ", seq->GetStdProgramName(param->track->GetPg()), param->track->GetBank() & 0x7FFF, param->track->GetPg() );
			else
				ImGui::Text("%s (%04Xh : %d) ", seq->GetStdDrumName(param->track->GetPg()), param->track->GetBank() & 0x7FFF, param->track->GetPg() );
		}
		else
			ImGui::Text("----");
	});
	storage.props[id]->w = 200;
	id = NewProp(storage, "Volume", [](DawPropDrawParam * param, IDawSeq *seq) {
		if (param->track->GetCh()) {
			ImGui::PushID(param->track->GetId());
			ImGui::SetNextItemWidth(param->self->w);
			if (ImGui::SliderInt("##volume", &param->track->GetMidiVolume(), 0, 16383)) {
				if (param->track->GetCh())
					seq->SendVolume(param->track->GetCh()-1, param->track->GetMidiVolume());
			}
			ImGui::PopID();
		} else
			ImGui::Text("----");
	});
	id = NewProp(storage, "Pan", [](DawPropDrawParam * param, IDawSeq *seq) {
		if (param->track->GetCh()) {
			ImGui::PushID(param->track->GetId());
			ImGui::SetNextItemWidth(param->self->w);
			if (ImGui::SliderInt("##pan", &param->track->GetMidiPan(), 0, 16383)) {
				if (param->track->GetCh())
					seq->SendPan(param->track->GetCh()-1, param->track->GetMidiPan());
			}
			ImGui::PopID();
		} else
			ImGui::Text("----");
	});
	id = NewProp(storage, "Fc", [](DawPropDrawParam * param, IDawSeq *seq) {
		if (param->track->GetCh()) {
			ImGui::PushID(param->track->GetId());
			ImGui::SetNextItemWidth(param->self->w);
			if (ImGui::SliderInt("##fc", &param->track->GetMidiFilterFc(), 0, 127)) {
				if (param->track->GetCh())
					seq->SendFilter(param->track->GetCh()-1, param->track->GetMidiFilterFc(), param->track->GetMidiFilterQ());
			}
			ImGui::PopID();
		} else
			ImGui::Text("----");
	});
	id = NewProp(storage, "Q", [](DawPropDrawParam * param, IDawSeq *seq) {
		if (param->track->GetCh()) {
			ImGui::PushID(param->track->GetId());
			ImGui::SetNextItemWidth(param->self->w);
			if (ImGui::SliderInt("##q", &param->track->GetMidiFilterQ(), 0, 127)) {
				if (param->track->GetCh())
					seq->SendFilter(param->track->GetCh()-1, param->track->GetMidiFilterQ(), param->track->GetMidiFilterQ());
			}
			ImGui::PopID();
		} else
			ImGui::Text("----");
	});
}

void DawMainProject::Draw(IDawSeq *seq)
{
	if (m_needRedraw) {
		if (m_storage) m_storage.reset();
		m_needRedraw = false;
	}
	if (!m_storage) {
		m_storage = std::make_unique<DawMainStorage>();
		InitDawMainStorage(*m_storage);
	}
	//DawMainStorage &storage = *m_storage;

	//static float w = 100.0f;
	m_drawingState.header_h = (float)(((int)ImGui::GetFrameHeightWithSpacing()*3/2) & ~1);
	m_drawingState.seq = seq;

	//ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);

	DrawChild1();

	ImGui::SameLine(0, 0);

	DrawChild2();
}

void DawMainProject::DrawChild1()
{
	Globals *globals = Globals::Resolve();
	DawMainStorage &storage = *m_storage;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
	//ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_WHITE);
	ImGui::SetNextWindowScroll(ImVec2{storage.scroll_x0, storage.scroll_y});
	m_drawingState.tot_h = 0;

	if (ImGui::BeginChild("child_1", {100.0f, 0.0f}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar)) {
		auto wndpos = ImGui::GetWindowPos();
		auto wndsz = ImGui::GetWindowSize();
		auto sbarsz = ImGui::GetCurrentWindow()->ScrollbarSizes;
		if (sbarsz.x == 0) sbarsz.x = 2; // width of splitter resizable line
		auto scrnmax = wndpos + wndsz - sbarsz;
		//auto cursor = ImGui::GetCursorScreenPos();

		auto xy0 = ImGui::GetCursorPos();

		float tot_w = 0;
		//ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
		// Row 0: Header
		{
			float pos_x = xy0.x;
			ImGui::SetCursorScreenPos(wndpos);
			for (unsigned int c=0; c<storage.prop_nums.size(); c++) {
				int prop_id = storage.prop_nums[c];
				DawProp * prop = storage.props[prop_id].get();

				if (c > 0) ImGui::SameLine();
				ImGui::SetCursorPosX(pos_x);
				globals->pImKosongg->ColoredButtonV1(prop->name.c_str(), {(float)prop->w + SplitterThickness/2, m_drawingState.header_h});

				pos_x += prop->w + SplitterThickness;
			}
			tot_w = pos_x;
		}
		ImGui::PopStyleVar(2 /* ImGuiStyleVar_FramePadding + ImGuiStyleVar_ItemSpacing */);

		// Row N: Track
		ImGui::PushClipRect({ wndpos.x, wndpos.y + m_drawingState.header_h }, { scrnmax.x, scrnmax.y}, false);
		if (m_drawingState.seq->IsFileLoaded()) {
			float pos_y = xy0.y + m_drawingState.header_h;
			for (int r=0; r<m_drawingState.seq->GetNumTracks(); r++) {
				IDawTrack * track = m_drawingState.seq->GetTrack(r);
				float pos_x = xy0.x;
				ImGui::SetCursorPosY(pos_y);

				for (unsigned int c=0; c<storage.prop_nums.size(); c++) {
					int prop_id = storage.prop_nums[c];
					DawProp * prop = storage.props[prop_id].get();

					DawPropDrawParam param{};
					param.self = prop;
					param.track = track;
					param.r = r;
					param.c = c;

					if (prop->DrawProp) {
						if (c > 0) ImGui::SameLine();
						ImGui::SetCursorPosX(pos_x);
						auto p1 = ImGui::GetCursorScreenPos();
						auto p2 = p1 + ImVec2{(float)param.self->w, (float)param.track->GetH()};
						ImGui::PushClipRect(p1, p2, true);
						prop->DrawProp(&param, m_drawingState.seq);
						ImGui::PopClipRect();
					}

					pos_x += prop->w + SplitterThickness;
				}

				// Draw Border R-Left
				pos_x = xy0.x;
				pos_y += track->GetH();
				ImGui::PushID(r);
				if (HSplitter({pos_x, pos_y}, tot_w)) {
					track->GetH() = ImMax(track->GetH() + ImGui::GetIO().MouseDelta.y, 20.0f);
				}
				ImGui::PopID();

				// Move to Next Row
				pos_y += SplitterThickness;
			}
			m_drawingState.tot_h = pos_y;
		}
		ImGui::PopClipRect(); // This Clip Rect make Button Behaviour undetected

		// Borders C
		{
			float pos_x = xy0.x;
			float pos_y = xy0.y;
			ImGui::SetCursorPosY(xy0.y);
			for (unsigned int c=0; c<storage.prop_nums.size(); c++) {
				int prop_id = storage.prop_nums[c];
				DawProp * prop = storage.props[prop_id].get();
				pos_x += prop->w;
				ImGui::PushID(c);
				if (VSplitter({pos_x, pos_y}, m_drawingState.tot_h)) {
					prop->w = ImMax(prop->w + ImGui::GetIO().MouseDelta.x, 20.0f);
				}
				ImGui::PopID();
				pos_x += SplitterThickness;
			}
		}

		auto sticky_p = wndpos + ImVec2{0, wndsz.y - ImGui::GetFrameHeightWithSpacing() * 2}; // OR timeline_pos + ImGui::GetScroll();
		ImGui::GetWindowDrawList()->AddRectFilled(sticky_p, wndpos + wndsz, IM_COL32(255,0,255,128));
		ImGui::SetCursorScreenPos(sticky_p + ImVec2{4,4});
		if (ImGui::Button("Refresh")) {
			m_needRedraw = true;
		}
		ImGui::SameLine();

		//ImGui::SetCursorPos({0, 1500});
		storage.scroll_y = ImGui::GetScrollY();
		storage.scroll_x0 = ImGui::GetScrollX();
	}
	ImGui::EndChild();
	//ImGui::PopStyleColor(); // ImGuiCol_ChildBg
	ImGui::PopStyleVar(/* ImGuiStyleVar_WindowPadding */);
}

void DawMainProject::DrawChild2()
{
	DawMainStorage &storage = *m_storage;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
	//ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_WHITE);
	ImGui::SetNextWindowScroll(ImVec2{storage.scroll_x1, storage.scroll_y});
	if (ImGui::BeginChild("child_2", {0, 0}, true, ImGuiWindowFlags_HorizontalScrollbar)) {
		storage.scroll_x1 = ImGui::GetScrollX();
		storage.scroll_y = ImGui::GetScrollY();
		float scroll_max_x1 = ImGui::GetScrollMaxX();
		auto wndpos = ImGui::GetWindowPos();
		auto wndsz = ImGui::GetWindowSize();
		auto sbarsz = ImGui::GetCurrentWindow()->ScrollbarSizes;
		ImVec2 scrnmax = wndpos + wndsz - sbarsz;

		ImVec2 far = {
			storage.uiStyle.leftPadding + (m_drawingState.seq->GetDisplayState()->play_duration * storage.uiStyle.beatWd) + storage.uiStyle.rightPadding,
			0};
		EnlargeWindow(far);

		DrawTimeline(wndpos, scrnmax);
		DrawCursor(wndpos);

		DrawNotes(wndpos, scrnmax);

#if 0
		// Debug
		if (m_showDebug) {
			auto sticky_p = wndpos + ImVec2{0, wndsz.y - ImGui::GetFrameHeightWithSpacing() * 2}; // OR timeline_pos + ImGui::GetScroll();
			ImGui::GetWindowDrawList()->AddRectFilled(sticky_p, wndpos + wndsz, IM_COL32(255,255,0,128));
			ImGui::SetCursorScreenPos(sticky_p + ImVec2{4,4});
			if (ImGui::Button("X")) {
				m_showDebug = false;
			}
			ImGui::SameLine();
			//auto p1 = (storage.scroll_x1 - storage.uiStyle.leftPadding);
			ImGui::Text("Visible (%ld - %ld), todraw = %d, tohide = %d, process = %d, startshow = %d",
				visible_clk_p1, visible_clk_p2, dbg_notes_to_draw, dbg_notes_to_hide, dbg_notes_processed,
					dbg_start_show_event_num);
		}
#endif

		// Auto Scroll
		if (m_drawingState.seq->IsPlaying() || m_drawingState.seq->IsRewinding()) {
static float endPercentage = 5.0f/6.0f;
static float beginPercentage = 1.0f/6.0f;
			if ((m_drawingState.cursor_x - storage.scroll_x1) < wndsz.x * beginPercentage) {
				storage.scroll_animate = 10;
				storage.scroll_target = (m_drawingState.cursor_x - (wndsz.x * beginPercentage));
				if (storage.scroll_target < 0) storage.scroll_target = 0;
			} else if ((m_drawingState.cursor_x - storage.scroll_x1) > wndsz.x * endPercentage) {
				storage.scroll_animate = 10;
				storage.scroll_target = (m_drawingState.cursor_x - (wndsz.x * beginPercentage));
				if (storage.scroll_target > scroll_max_x1) storage.scroll_target = scroll_max_x1;
			}
		}

		if (storage.scroll_animate == 1) {
			storage.scroll_animate = 0;
			storage.scroll_x1 = storage.scroll_target;
		} else if (storage.scroll_animate > 1) {
			storage.scroll_animate--;
			float direction = storage.scroll_target - storage.scroll_x1;
			if (direction <= -1.0 || direction >= 1.0) {
				storage.scroll_x1 += direction*0.5;
			}
		}
	}
	ImGui::EndChild();
	// ImGui::PopStyleColor(); // ImGuiCol_ChildBg
	ImGui::PopStyleVar(2);
}

void DawMainProject::DrawTimeline(ImVec2 & wndpos, ImVec2 & scrnmax)
{
	DawMainStorage &storage = *m_storage;

	jdksmidi::MIDIClockTime cur_clk = 0, next_timesig_clk = 0;
	int timesig_numerator = 4;  // beats per measure
	int timesig_denominator = 4;  // width of beat
	float beat_clk = m_drawingState.seq->GetDisplayState()->ppqn * 4.0f/timesig_denominator;
	bool show_timesig = true;

	int b = 0;
	int m = 0;
	int event_num = 0;
	IDawTrack * track0 = m_drawingState.seq->IsFileLoaded() ? m_drawingState.seq->GetTrack(0) :  nullptr;

	// Timeline
	ImVec2 scrnpos = wndpos;
	scrnpos.x += storage.uiStyle.leftPadding - storage.scroll_x1;

	auto drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled({ wndpos.x, scrnpos.y}, ImVec2{scrnmax.x, scrnpos.y+m_drawingState.header_h}, ImGui::GetColorU32(ImGuiCol_Header));
	drawList->AddLine({ scrnpos.x - storage.uiStyle.leftPadding, scrnpos.y+m_drawingState.header_h}, ImVec2{scrnmax.x, scrnpos.y+m_drawingState.header_h}, ImGui::GetColorU32(ImGuiCol_Border));

	while (scrnpos.x < scrnmax.x) {
		const jdksmidi::MIDITimedBigMessage * msg{};

		if (track0) {

			while ( event_num < track0->GetMidiTrack()->GetNumEvents() ) {
				msg = track0->GetMidiTrack()->GetEvent(event_num);
				if (msg->IsTimeSig()) {
					if (msg->GetTime() == cur_clk) {
						timesig_numerator = msg->GetTimeSigNumerator();
						timesig_denominator = msg->GetTimeSigDenominator();
						beat_clk = m_drawingState.seq->GetDisplayState()->ppqn * 4.0f/timesig_denominator;
						b = 0;
						show_timesig = true;
					} else {
						next_timesig_clk = msg->GetTime();
						break;
					}
				}
				event_num++;
			}
		}

		if (b % timesig_numerator == 0) {
			drawList->AddLine(scrnpos, ImVec2{scrnpos.x, scrnmax.y}, ImGui::GetColorU32(ImGuiCol_Separator), 2.0);
			std::string labeltime = fmt::format("{}", 1 + m);
			drawList->AddText(scrnpos + ImVec2{4,4}, IM_COL32_BLACK, labeltime.c_str());
			if (show_timesig) {
				labeltime = fmt::format("{}/{}", timesig_numerator, timesig_denominator);
				show_timesig = false;
				drawList->AddText(scrnpos + ImVec2{4,m_drawingState.header_h/2}, IM_COL32_BLACK, labeltime.c_str());
			}
			m++;
		} else {
			drawList->AddLine(scrnpos + ImVec2{0, m_drawingState.header_h/2}, ImVec2{scrnpos.x, scrnmax.y}, ImGui::GetColorU32(ImGuiCol_Border));
		}
		float delta_clk = beat_clk;
		if (next_timesig_clk != 0 && cur_clk + beat_clk >= next_timesig_clk) {
			delta_clk = next_timesig_clk - cur_clk;
			next_timesig_clk = 0;
		}

		scrnpos.x += storage.uiStyle.beatWd * delta_clk / m_drawingState.seq->GetDisplayState()->ppqn;
		cur_clk += delta_clk;

		b++;
	}
}

void DawMainProject::DrawCursor(ImVec2 & wndpos)
{
	DawMainStorage &storage = *m_storage;

	// Cursor
	m_drawingState.cursor_x = storage.is_cursor_dragging ?
		storage.dragging_cursor_x :
		storage.uiStyle.beatWd * m_drawingState.seq->GetDisplayState()->play_cursor;

	//ImGui::SetCursorPos({ storage.uiStyle.leftPadding + m_drawingState.cursor_x - (storage.uiStyle.cursorWd/2), header_h/2});
	ImGui::SetCursorScreenPos({
		wndpos.x - storage.scroll_x1 + storage.uiStyle.leftPadding + m_drawingState.cursor_x - (storage.uiStyle.cursorWd/2),
		wndpos.y + m_drawingState.header_h/2
	});
	ImGui::InvisibleButton("cursor", ImVec2{(float)storage.uiStyle.cursorWd, m_drawingState.header_h/2});
	auto rcmin = ImGui::GetItemRectMin();
	auto rcmax = ImGui::GetItemRectMax();
	if (ImGui::IsItemHovered()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if(ImGui::IsItemActive()) {
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
			if (!storage.is_cursor_dragging)
				storage.dragging_cursor_x = m_drawingState.cursor_x;
			storage.dragging_cursor_x =
				ImClamp(storage.dragging_cursor_x + ImGui::GetIO().MouseDelta.x,
					0.0f,
					m_drawingState.seq->GetDisplayState()->play_duration * storage.uiStyle.beatWd);
			storage.is_cursor_dragging = true;
		}
	}
	if (ImGui::IsItemDeactivated()) {
		if (storage.is_cursor_dragging) {
			storage.is_cursor_dragging = false;
			m_drawingState.seq->SetMIDITimeBeat(storage.dragging_cursor_x / storage.uiStyle.beatWd);
		}
	}

	ImVec2 points[] = {
		{ rcmin.x, rcmin.y},
		{ rcmin.x, rcmax.y - (storage.uiStyle.cursorWd/2)},
		{ rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y},
		{ rcmax.x, rcmax.y - (storage.uiStyle.cursorWd/2)},
		{ rcmax.x, rcmin.y},
	};

	auto drawList = ImGui::GetWindowDrawList();
	drawList->AddConvexPolyFilled(points, IM_ARRAYSIZE(points), ImGui::GetColorU32(ImGuiCol_SliderGrab));
	drawList->AddLine({ rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y}, { rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y + m_drawingState.tot_h}, ImGui::GetColorU32(ImGuiCol_Border));
}

void DawMainProject::EnlargeWindow(ImVec2 & far)
{
	auto xy0 = ImGui::GetCursorPos();
	ImGui::SetCursorPos(far);
	ImGui::Dummy(ImVec2{1,1});
	ImGui::SetCursorPos(xy0);
}

void DawMainProject::DrawNotes(ImVec2 & wndpos, ImVec2 & scrnmax)
{
	DawMainStorage &storage = *m_storage;

	// Contents and Borders R-R
	auto v_p1 = (storage.scroll_x1 - storage.uiStyle.leftPadding);
	auto v_p2 = v_p1 + scrnmax.x;

	m_drawingState.visible_clk_p1 = v_p1 * (float) m_drawingState.seq->GetDisplayState()->ppqn  / (storage.uiStyle.beatWd);
	m_drawingState.visible_clk_p2 = v_p2 * (float) m_drawingState.seq->GetDisplayState()->ppqn  / (storage.uiStyle.beatWd);

	int dbg_start_show_event_num = -1;
	int dbg_notes_to_draw = 0, dbg_notes_to_hide = 0, dbg_notes_processed = 0;

	ImGui::PushClipRect({ wndpos.x, wndpos.y + m_drawingState.header_h }, { scrnmax.x, scrnmax.y }, false);
	if (m_drawingState.seq->IsFileLoaded()) {
		float pos_x = 0;
		float pos_y = 0 + m_drawingState.header_h;

		for (int r=0; r<m_drawingState.seq->GetNumTracks(); r++) {
			IDawTrack * track = m_drawingState.seq->GetTrack(r);

			DawTrackNotesUI trackNotes(m_drawingState.seq);
			trackNotes.visible_start_clk = m_drawingState.visible_clk_p1;
			trackNotes.displayState = m_drawingState.seq->GetDisplayState();
			trackNotes.uiStyle = &storage.uiStyle;
			trackNotes.scrnpos_x = wndpos.x + pos_x;
			trackNotes.scrnpos_y = wndpos.y - storage.scroll_y + pos_y;
			trackNotes.track_h = track->GetH();
			trackNotes.start_show_event_num = -1;
			trackNotes.start_show_event_clk = -1;
			int event_num = 0;

			if (track->GetViewcacheStartVisibleClk() >= 0 && track->GetViewcacheStartVisibleClk() <= m_drawingState.visible_clk_p1) {
				event_num = track->GetViewcacheStartEventNum();
			}

			ImGui::PushID(r);

			for (; event_num < track->GetMidiTrack()->GetNumEvents(); ++event_num) {
				trackNotes.cur_event_num = event_num;
				const jdksmidi::MIDITimedBigMessage * msg = track->GetMidiTrack()->GetEvent(event_num);
				if ((signed long)msg->GetTime() >= m_drawingState.visible_clk_p2) break; // event is ordered by time

				if (msg->IsNoteOn()) {
					trackNotes.NoteOn( msg->GetTime(), msg->GetNote(), msg->GetVelocity() );
				} if (msg->IsNoteOff()) {
					trackNotes.NoteOff( msg->GetTime(), msg->GetNote() );
				}
			}

			trackNotes.ClipOff(m_drawingState.visible_clk_p2);

			pos_y += track->GetH();
			if (HSplitter({pos_x, pos_y}, storage.uiStyle.leftPadding + (m_drawingState.seq->GetDisplayState()->play_duration * storage.uiStyle.beatWd) + storage.uiStyle.rightPadding))
			{
				track->GetH() = ImMax(track->GetH() + ImGui::GetIO().MouseDelta.y, 20.0f);
			}

			ImGui::PopID();
			pos_y += 8;

			if (m_drawingState.visible_clk_p1 >= track->GetViewcacheStartVisibleClk()) {
				track->GetViewcacheStartVisibleClk() = m_drawingState.visible_clk_p1;
				if (trackNotes.start_show_event_num != -1)
					track->GetViewcacheStartEventNum() = trackNotes.start_show_event_num;
			} else {
				track->GetViewcacheStartVisibleClk() = -1;
				track->GetViewcacheStartEventNum() = 0;
			}

			if (track->GetId() == 5) { // only  check one track
				dbg_start_show_event_num = track->GetViewcacheStartEventNum();
			}
			dbg_notes_to_draw   += trackNotes.notes_to_draw;
			dbg_notes_to_hide   += trackNotes.notes_to_hide;
			dbg_notes_processed += trackNotes.GetNoteProcessed();
		}
	}
	ImGui::PopClipRect();
}
//
DawMainProject::DawMainProject() {
	auto cb = [this](hscpp::SwapInfo& info) {
		info.SaveMove("storage", m_storage);
		info.Save("showDebug", m_showDebug);
		info.Save("needRedraw", m_needRedraw);
    };

    Hscpp_SetSwapHandler(cb);

	if (Hscpp_IsSwapping()) {
		return;
	}

	Creating();
}

DawMainProject::~DawMainProject() {
	if (Hscpp_IsSwapping())
    {
        return;
    }

	Destroying();
}

void DawMainProject::Update() {
	ImGui::SetCurrentContext( Globals::Resolve()->pImGuiContext );

	ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
	if (ImGui::Begin("Vinfony Project")) {
		ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3.0f);

		Draw(Globals::Resolve()->sequencer);

		ImGui::PopStyleVar();
	}
	ImGui::End();
}

void DawMainProject::Creating() {
	fmt::println( "DawMainProject::Creating" );

	m_showDebug = true;
	m_needRedraw = false;
}

void DawMainProject::Destroying() {
	fmt::println( "DawMainProject::Destroying" );
}

}