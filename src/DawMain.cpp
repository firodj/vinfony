#include "DawMain.hpp"

#include <map>
#include <memory>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "kosongg/Component.h"
#include <fmt/core.h>
#include "jdksmidi/track.h"
#include <libassert/assert.hpp>

#include "DawTrack.hpp"
#include "DawTrackNotes.hpp"

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
  const int SplitterThickness = 8;

  struct DawUIStyle {
    int cursorWd     = 10;
    int beatWd       = 40;
    int leftPadding  = 40;
    int rightPadding = 40;
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

  std::vector<DawMainStorage> g_storages;

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
    id = NewProp(storage, "No", [](DawPropDrawParam * param, DawSeq *seq) {
      std::string label = fmt::format("{:3d}", param->r);
      auto txtSize = ImGui::CalcTextSize(label.c_str());
      ImGui::SetCursorPosX( ImGui::GetCursorPosX() + param->self->w - txtSize.x );
      ImGui::Text("%s", label.c_str());
    });
    storage.props[id]->w = 20;

    NewProp(storage, "Name", [](DawPropDrawParam * param, DawSeq *seq) {
      ImGui::Text("%s", param->track->name.c_str() );
    });
    NewProp(storage, "Channel", [](DawPropDrawParam * param, DawSeq *seq) {
#if 0
      auto p1 = ImGui::GetCursorScreenPos();
      auto p2 = p1 + ImVec2{(float)param->self->w, (float)param->track->h};
      ImGui::GetWindowDrawList()->AddRectFilled(p1, p2, IM_COL32(255, 0,0, 255));
#endif

      const char* items[] = {"-", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};

      ImGui::SetNextItemWidth(param->self->w);
      ImGui::PushID(param->track->id);
      ImGui::Combo("##channel", (int*)&param->track->ch, items, IM_ARRAYSIZE(items));
      ImGui::PopID();
    });
    id = NewProp(storage, "Instrument", [](DawPropDrawParam * param, DawSeq *seq) {
      if (param->track->pg)
        ImGui::Text("%s (%04Xh : %d) ", GetStdProgramName(param->track->pg), param->track->bank & 0x7FFF, param->track->pg );
      else
        ImGui::Text("----");
    });
    storage.props[id]->w = 200;
    id = NewProp(storage, "Volume", [](DawPropDrawParam * param, DawSeq *seq) {
      if (param->track->ch) {
        ImGui::PushID(param->track->id);
        ImGui::SetNextItemWidth(param->self->w);
        if (ImGui::SliderInt("##volume", &param->track->midiVolume, 0, 16383)) {
          if (param->track->ch)
            seq->SendVolume(param->track->ch-1, param->track->midiVolume);
        }
        ImGui::PopID();
      } else
        ImGui::Text("----");
    });
    id = NewProp(storage, "Pan", [](DawPropDrawParam * param, DawSeq *seq) {
      if (param->track->ch) {
        ImGui::PushID(param->track->id);
        ImGui::SetNextItemWidth(param->self->w);
        if (ImGui::SliderInt("##pan", &param->track->midiPan, 0, 16383)) {
          if (param->track->ch)
            seq->SendPan(param->track->ch-1, param->track->midiPan);
        }
        ImGui::PopID();
      } else
        ImGui::Text("----");
    });
  }

  static void VSplitter(ImVec2 pos, float avail_h, SplitterOnDraggingFunc func) {
    ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
    ImU32 color_hover  = ImGui::GetColorU32(ImGuiCol_SeparatorHovered, 1.0);
    ImU32 color_active  = ImGui::GetColorU32(ImGuiCol_SeparatorActive, 1.0);

    ImGui::SetCursorPos(pos);
    ImGui::InvisibleButton("vsplitter", ImVec2{SplitterThickness, (float)ImMax(avail_h, 1.0f)});
    auto rcmin = ImGui::GetItemRectMin();
    auto rcmax = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      color_border = color_hover;
    }
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      if (func) func();
      color_border = color_active;
    }

    rcmin.x += SplitterThickness/2;
    rcmax.x = rcmin.x;
    ImGui::GetWindowDrawList()->AddLine(rcmin, rcmax, color_border);
  }

  static void HSplitter(ImVec2 pos, float avail_w, SplitterOnDraggingFunc func) {
    ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
    ImU32 color_hover  = ImGui::GetColorU32(ImGuiCol_SeparatorHovered, 1.0);
    ImU32 color_active = ImGui::GetColorU32(ImGuiCol_SeparatorActive, 1.0);

    ImGui::SetCursorPos(pos);
    ImGui::InvisibleButton("hsplitter", ImVec2{(float)ImMax(avail_w, 1.0f), SplitterThickness});
    auto rcmin = ImGui::GetItemRectMin();
    auto rcmax = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
      color_border = color_hover;
    }
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      if (func) func();
      color_border = color_active;
    }
    rcmin.y += SplitterThickness/2;
    rcmax.y = rcmin.y;
    ImGui::GetWindowDrawList()->AddLine(rcmin, rcmax, color_border);
  }

  void DawMain(const char *label, DawSeq *seq) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    const ImGuiID id = window->GetID(label);
    int * storageIdx = window->StateStorage.GetIntRef(id, -1);
    if (*storageIdx == -1) {
      *storageIdx = g_storages.size();
      g_storages.push_back(DawMainStorage{});
      InitDawMainStorage(g_storages[*storageIdx]);
    }
    DawMainStorage &storage = g_storages[*storageIdx];

    static float w = 200.0f;
    float h0 = (float)(((int)ImGui::GetFrameHeightWithSpacing()*3/2) & ~1);

    ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    //ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_WHITE);
    ImGui::SetNextWindowScroll(ImVec2{storage.scroll_x0, storage.scroll_y});
    float tot_h = 0;

    if (ImGui::BeginChild("child_1", {w, 0.0f}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar)) {
      auto wndpos = ImGui::GetWindowPos();
      auto wndsz = ImGui::GetWindowSize();
      auto scrnmax = wndpos + wndsz;
      auto cursor = ImGui::GetCursorScreenPos();

      auto xy0 = ImGui::GetCursorPos();
      auto avail = ImGui::GetContentRegionAvail();

      //ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
      // Row 0: Header
      float tot_w = 0;
      float pos_x = xy0.x;
      ImGui::SetCursorScreenPos(wndpos);
      for (int c=0; c<storage.prop_nums.size(); c++) {
        int prop_id = storage.prop_nums[c];
        DawProp * prop = storage.props[prop_id].get();

        if (c > 0) ImGui::SameLine();
        ImGui::SetCursorPosX(pos_x);
        ImGui::ColoredButtonV1(prop->name.c_str(), {(float)prop->w + SplitterThickness/2, h0});

        pos_x += prop->w + SplitterThickness;
      }
      tot_w = pos_x;
      ImGui::PopStyleVar(2);

      // Row N: Track
      ImGui::PushClipRect({ wndpos.x, wndpos.y + h0 }, { scrnmax.x, scrnmax.y }, false);
      if (seq->IsFileLoaded()) {
        float pos_y = xy0.y + h0;
        for (int r=0; r<seq->GetNumTracks(); r++) {
          DawTrack * track = seq->GetTrack(r);
          float pos_x = xy0.x;
          ImGui::SetCursorPosY(pos_y);

          for (int c=0; c<storage.prop_nums.size(); c++) {
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
              auto p2 = p1 + ImVec2{(float)param.self->w, (float)param.track->h};
              ImGui::PushClipRect(p1, p2, true);
              prop->DrawProp(&param, seq);
              ImGui::PopClipRect();
            }

            pos_x += prop->w + SplitterThickness;
          }

          // Draw Border R-Left
          pos_x = xy0.x;
          pos_y += track->h;
          ImGui::PushID(r);
          HSplitter({pos_x, pos_y}, tot_w, [&]() { track->h = ImMax(track->h + ImGui::GetIO().MouseDelta.y, 20.0f); });
          ImGui::PopID();

          // Move to Next Row
          pos_y += SplitterThickness;
        }
        tot_h = pos_y;
      }
      ImGui::PopClipRect(); // This Clip Rect make Button Behaviour undetected

      // Borders C
      {
        float pos_x = xy0.x;
        float pos_y = xy0.y;
        ImGui::SetCursorPosY(xy0.y);
        for (int c=0; c<storage.prop_nums.size(); c++) {
          int prop_id = storage.prop_nums[c];
          DawProp * prop = storage.props[prop_id].get();
          pos_x += prop->w;
          ImGui::PushID(c);
          VSplitter({pos_x, pos_y}, tot_h, [&]() { prop->w = ImMax(prop->w + ImGui::GetIO().MouseDelta.x, 20.0f); });
          ImGui::PopID();
          pos_x += SplitterThickness;
        }
      }

      //ImGui::SetCursorPos({0, 1500});
      storage.scroll_y = ImGui::GetScrollY();
      storage.scroll_x0 = ImGui::GetScrollX();
    }
    ImGui::EndChild();
    //ImGui::PopStyleColor(); // ImGuiCol_ChildBg
    ImGui::PopStyleVar();

    ImGui::SameLine(0, 0);

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
      ImVec2 scrnmax = wndpos + wndsz;

      //auto draw_list = ImGui::GetWindowDrawList();
      //auto timeline_pos = ImGui::GetCursorScreenPos();

      auto avail = ImGui::GetContentRegionAvail();

      float far_x = storage.uiStyle.leftPadding + (seq->displayState.play_duration * storage.uiStyle.beatWd) + storage.uiStyle.rightPadding;
      {
        auto xy0 = ImGui::GetCursorPos();
        ImGui::SetCursorPos({far_x, 0});
        ImGui::Dummy(ImVec2{1,1});
        ImGui::SetCursorPos(xy0);
      }

      // ImGui::GetCursorScreenPos -> respect  scrolled  because translated from GetCursorPos.
      // ImGui::GetContentRegionAvail -> a viewable portion only, dontcare scrolled.
      // ImGui::GetWindowContentRegionMin -> (local) ImGui::GetCursorPos initially.
      // ImGui::GetWindowContentRegionMax = ImGui::GetContentRegionMax ? -> respect scrolled
      //

      // Timeline
      ImVec2 scrnpos = wndpos;
      scrnpos.x += storage.uiStyle.leftPadding - storage.scroll_x1;

      {
        auto drawList = ImGui::GetWindowDrawList();
        int b = 0;
        int m = 0;
        drawList->AddRectFilled({ wndpos.x, scrnpos.y}, ImVec2{scrnmax.x, scrnpos.y+h0}, ImGui::GetColorU32(ImGuiCol_Header));
        drawList->AddLine({ scrnpos.x - storage.uiStyle.leftPadding, scrnpos.y+h0}, ImVec2{scrnmax.x, scrnpos.y+h0}, ImGui::GetColorU32(ImGuiCol_Border));
        int event_num = 0;
        DawTrack * track0 = seq->IsFileLoaded() ? seq->GetTrack(0) :  nullptr;
        jdksmidi::MIDIClockTime cur_clk = 0, next_timesig_clk = 0;
        int timesig_numerator = 4;  // beats per measure
        int timesig_denominator = 4;  // width of beat
        float beat_clk = seq->displayState.ppqn * 4.0f/timesig_denominator;
        bool show_timesig = true;
        //float max_clk = scrnmax.x - scrnpos.x / storage.uiStyle.beatWd;

        while (scrnpos.x < scrnmax.x) {
          const jdksmidi::MIDITimedBigMessage * msg{};

          if (track0) {

            while ( event_num < track0->midi_track->GetNumEvents() ) {
              msg = track0->midi_track->GetEvent(event_num);
              if (msg->IsTimeSig()) {
                if (msg->GetTime() == cur_clk) {
                  timesig_numerator = msg->GetTimeSigNumerator();
                  timesig_denominator = msg->GetTimeSigDenominator();
                  beat_clk = seq->displayState.ppqn * 4.0f/timesig_denominator;
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
            std::string label = fmt::format("{}", 1 + m);
            drawList->AddText(scrnpos + ImVec2{4,4}, IM_COL32_BLACK, label.c_str());
            if (show_timesig) {
              label = fmt::format("{}/{}", timesig_numerator, timesig_denominator);
              show_timesig = false;
              drawList->AddText(scrnpos + ImVec2{4,h0/2}, IM_COL32_BLACK, label.c_str());
            }
            m++;
          } else {
            drawList->AddLine(scrnpos + ImVec2{0, h0/2}, ImVec2{scrnpos.x, scrnmax.y}, ImGui::GetColorU32(ImGuiCol_Border));
          }
          float delta_clk = beat_clk;
          if (next_timesig_clk != 0 && cur_clk + beat_clk >= next_timesig_clk) {
            delta_clk = next_timesig_clk - cur_clk;
            next_timesig_clk = 0;
          }

          scrnpos.x += storage.uiStyle.beatWd * delta_clk / seq->displayState.ppqn;
          cur_clk += delta_clk;

          b++;
        }
      }

      scrnpos.y += h0;
      scrnpos.x = wndpos.x;

      // Cursor
      float cursor_x = storage.is_cursor_dragging ?
        storage.dragging_cursor_x :
        storage.uiStyle.beatWd * seq->displayState.play_cursor;

      {
        auto drawList = ImGui::GetWindowDrawList();

        //ImGui::SetCursorPos({ storage.uiStyle.leftPadding + cursor_x - (storage.uiStyle.cursorWd/2), h0/2});
        ImGui::SetCursorScreenPos({ wndpos.x - storage.scroll_x1 + storage.uiStyle.leftPadding + cursor_x - (storage.uiStyle.cursorWd/2) , wndpos.y + h0/2 });
        ImGui::InvisibleButton("cursor", ImVec2{(float)storage.uiStyle.cursorWd, h0/2});
        auto rcmin = ImGui::GetItemRectMin();
        auto rcmax = ImGui::GetItemRectMax();
        if (ImGui::IsItemHovered()) {
          ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if(ImGui::IsItemActive()) {
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (!storage.is_cursor_dragging)
              storage.dragging_cursor_x = cursor_x;
            storage.dragging_cursor_x =
              ImClamp(storage.dragging_cursor_x + ImGui::GetIO().MouseDelta.x,
                0.0f,
                seq->displayState.play_duration * storage.uiStyle.beatWd);
            storage.is_cursor_dragging = true;
          }
        }
        if (ImGui::IsItemDeactivated()) {
          if (storage.is_cursor_dragging) {
            storage.is_cursor_dragging = false;
            seq->SetMIDITimeBeat(storage.dragging_cursor_x / storage.uiStyle.beatWd);
          }
        }

        ImVec2 points[] = {
          { rcmin.x, rcmin.y},
          { rcmin.x, rcmax.y - (storage.uiStyle.cursorWd/2)},
          { rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y},
          { rcmax.x, rcmax.y - (storage.uiStyle.cursorWd/2)},
          { rcmax.x, rcmin.y},
        };

        drawList->AddConvexPolyFilled(points, IM_ARRAYSIZE(points), ImGui::GetColorU32(ImGuiCol_SliderGrab));
        drawList->AddLine({ rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y}, { rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y + tot_h}, ImGui::GetColorU32(ImGuiCol_Border));
      }

      // Contents and Borders R-R
      auto v_p1 = (storage.scroll_x1 - storage.uiStyle.leftPadding);
      auto v_p2 = v_p1 + wndsz.x;

      long visible_clk_p1 = v_p1 * (float) seq->displayState.ppqn  / (storage.uiStyle.beatWd);
      long visible_clk_p2 = v_p2 * (float) seq->displayState.ppqn  / (storage.uiStyle.beatWd);

      class DawTrackNotesUI: public DawTrackNotes {
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

        void DrawNote(int slot) override {
          if (note_actives[slot].stop >= visible_start_clk) {
            auto drawList = ImGui::GetWindowDrawList();
            float y0 = (track_h * (float)(127 - note_actives[slot].note)/ 128);
            assert (note_actives[slot].note >= 0 && note_actives[slot].note <= 127);
            float nh = (float)track_h/128;
            float x0 = (note_actives[slot].time - visible_start_clk);
            float x1 = (note_actives[slot].stop - visible_start_clk);

            x0 = x0 * uiStyle->beatWd / displayState->ppqn;
            x1 = x1 * uiStyle->beatWd / displayState->ppqn;

            ImVec2 p1{scrnpos_x + x0, scrnpos_y + y0};
            ImVec2 p2{scrnpos_x + x1, scrnpos_y + y0};
            if (nh >= 1.0) {
              p2.y  += nh;
              drawList->AddRectFilled(p1, p2, IM_COL32(255,64,64, 255));
            } else {
              drawList->AddLine(p1, p2, IM_COL32(255,64,64, 255));
            }

            notes_to_draw++;
          } else {
            notes_to_hide++;
          }
        }
      };

      DawTrackNotesUI trackNotes{};
      trackNotes.visible_start_clk = visible_clk_p1;
      //trackNotes.draw_list = draw_list;
      trackNotes.displayState = &seq->displayState;
      trackNotes.uiStyle = &storage.uiStyle;

      ImGui::PushClipRect({ wndpos.x, wndpos.y + h0 }, { scrnmax.x, scrnmax.y }, false);
      if (seq->IsFileLoaded()) {
        float pos_x = 0;
        float pos_y = 0 + h0;

        for (int r=0; r<seq->GetNumTracks(); r++) {
          DawTrack * track = seq->GetTrack(r);

          ImGui::PushID(r);
          trackNotes.scrnpos_x = wndpos.x + pos_x;
          trackNotes.scrnpos_y = wndpos.y - storage.scroll_y + pos_y;
          trackNotes.track_h = track->h;

          for (int event_num = 0; event_num < track->midi_track->GetNumEvents(); ++event_num) {
            const jdksmidi::MIDITimedBigMessage * msg = track->midi_track->GetEvent(event_num);
            if (msg->GetTime() >= visible_clk_p2) break; // event is ordered by time

            if (msg->IsNoteOn()) {
              trackNotes.NoteOn( msg->GetTime(), msg->GetNote(), msg->GetVelocity() );
            } if (msg->IsNoteOff()) {
              trackNotes.NoteOff( msg->GetTime(), msg->GetNote() );
            }
          }

          trackNotes.ClipOff(visible_clk_p2);

          pos_y += track->h;
          HSplitter({pos_x, pos_y}, storage.uiStyle.leftPadding + (seq->displayState.play_duration * storage.uiStyle.beatWd) + storage.uiStyle.rightPadding,
            [&]() { track->h = ImMax(track->h + ImGui::GetIO().MouseDelta.y, 20.0f); }
          );
          ImGui::PopID();
          pos_y += 8;
        }
      }
      ImGui::PopClipRect();

      // Debug
static bool show_debug = true;
      if (show_debug) {
        auto sticky_p = wndpos + ImVec2{0, wndsz.y - ImGui::GetFrameHeightWithSpacing() * 2}; // OR timeline_pos + ImGui::GetScroll();
        ImGui::GetWindowDrawList()->AddRectFilled(sticky_p, wndpos + wndsz, IM_COL32(255,255,0,255));
        ImGui::SetCursorScreenPos(sticky_p + ImVec2{4,4});
        if (ImGui::Button("X")) {
          show_debug = false;
        }
        ImGui::SameLine();
        auto p1 = (storage.scroll_x1 - storage.uiStyle.leftPadding);
        ImGui::Text("visible (%ld - %ld), todraw = %d, tohide = %d, process = %d",
          visible_clk_p1, visible_clk_p2, trackNotes.notes_to_draw, trackNotes.notes_to_hide, trackNotes.notes_processed);
      }

      // Auto Scroll
      if (seq->IsPlaying()) {
static float endPercentage = 5.0f/6.0f;
static float beginPercentage = 1.0f/6.0f;
        if ((cursor_x - storage.scroll_x1) < wndsz.x * beginPercentage) {
          storage.scroll_animate = 10;
          storage.scroll_target = (cursor_x - (wndsz.x * beginPercentage));
          if (storage.scroll_target < 0) storage.scroll_target = 0;
        } else if ((cursor_x - storage.scroll_x1) > wndsz.x * endPercentage) {
          storage.scroll_animate = 10;
          storage.scroll_target = (cursor_x - (wndsz.x * beginPercentage));
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
}