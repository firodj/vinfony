#include "DawMain.hpp"

#include <map>
#include <memory>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "kosongg/Component.h"
#include <fmt/core.h>

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
    prop->DrawTrack = func;

    storage.prop_nums.push_back(prop->id);

    return prop->id;
  }

  void InitDawMainStorage(DawMainStorage & storage) {
    // Properties
    NewProp(storage, "No", [](DawPropDrawParam * param) { ImGui::Text("%d", param->r); });
    NewProp(storage, "Name", [](DawPropDrawParam * param) {
      auto p1 = ImGui::GetCursorScreenPos();
      auto p2 = p1 + ImVec2{(float)param->self->w, (float)param->track->h};
      ImGui::PushClipRect(p1, p2, true);
      // ImGui::GetWindowDrawList()->AddRectFilled(p1, p2, IM_COL32(255, 0,0, 255));
      ImGui::Text("%s", param->track->name.c_str() );
      ImGui::PopClipRect();
    });
    NewProp(storage, "Channel", [](DawPropDrawParam * param) {
      auto p1 = ImGui::GetCursorScreenPos();
      auto p2 = p1 + ImVec2{(float)param->self->w, (float)param->track->h};

      const char* items[] = {"-", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
      static int item_current = 0;
      ImGui::SetNextItemWidth(param->self->w);
      ImGui::PushID(param->track->id);
      ImGui::Combo("##channel", &item_current, items, IM_ARRAYSIZE(items));
      ImGui::PopID();
    });
    NewProp(storage, "Instrument", nullptr);
  }

  static void VSplitter(ImVec2 pos, float avail_h, SplitterOnDraggingFunc func) {
    auto draw_list     = ImGui::GetWindowDrawList();
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
    draw_list->AddLine(rcmin, rcmax, color_border);
  }

  static void HSplitter(ImVec2 pos, float avail_w, SplitterOnDraggingFunc func) {
    auto draw_list     = ImGui::GetWindowDrawList();
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
    draw_list->AddLine(rcmin, rcmax, color_border);
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
    // DAW test
    auto conavail = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_WHITE);
    ImGui::SetNextWindowScroll(ImVec2{storage.scroll_x0, storage.scroll_y});
    float tot_h = 0;

    if (ImGui::BeginChild("child_1", {w, 0.0f}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar)) {
      auto draw_list = ImGui::GetWindowDrawList();

      auto cursor = ImGui::GetCursorScreenPos();
      auto legend = ImGui::GetWindowSize();

      auto xy0 = ImGui::GetCursorPos();
      auto avail = ImGui::GetContentRegionAvail();

      // Row 0: Header
      float tot_w = 0;
      float pos_x = xy0.x;
      for (int c=0; c<storage.prop_nums.size(); c++) {
        int prop_id = storage.prop_nums[c];
        DawProp * prop = storage.props[prop_id].get();
        if (c > 0) ImGui::SameLine();
        ImGui::SetCursorPosX(pos_x);
        ImGui::Button(prop->name.c_str(), {(float)prop->w, h0});
        pos_x += prop->w + SplitterThickness;
      }
      tot_w = pos_x;

      // Row N: Track

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

            if (prop->DrawTrack) {
              if (c > 0) ImGui::SameLine();
              ImGui::SetCursorPosX(pos_x);
              prop->DrawTrack(&param);
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

      //ImGui::SliderFloat("float", &f, 0.0f, 1.0f);           // Edit 1 float using a slider from 0.0f to 1.0f

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
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::SameLine(0, 0);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_WHITE);
    ImGui::SetNextWindowScroll(ImVec2{storage.scroll_x1, storage.scroll_y});
    if (ImGui::BeginChild("child_2", {0, 0}, true, ImGuiWindowFlags_HorizontalScrollbar)) {
      storage.scroll_x1 = ImGui::GetScrollX();
      storage.scroll_y = ImGui::GetScrollY();
      float scroll_max_x1 = ImGui::GetScrollMaxX();

      auto draw_list = ImGui::GetWindowDrawList();
      auto timeline_pos = ImGui::GetCursorScreenPos();
      auto xy0 = ImGui::GetCursorPos();
      auto avail = ImGui::GetContentRegionAvail();

      float far_x = storage.uiStyle.leftPadding + (seq->displayState.play_duration * storage.uiStyle.beatWd) + storage.uiStyle.rightPadding;
      ImGui::SetCursorPos({far_x, 0});
      ImGui::Dummy(ImVec2{1,1});

      ImGui::SetCursorPos(xy0);

      // ImGui::GetCursorScreenPos -> respect  scrolled  because translated from GetCursorPos.
      // ImGui::GetContentRegionAvail -> a viewable portion only, dontcare scrolled.
      // ImGui::GetWindowContentRegionMin -> (local) ImGui::GetCursorPos initially.
      // ImGui::GetWindowContentRegionMax = ImGui::GetContentRegionMax ? -> respect scrolled
      //

      auto wndpos = ImGui::GetWindowPos();
      auto wndsz = ImGui::GetWindowSize();
      ImVec2 scrnmax = wndpos + wndsz;

      // Timeline
      int t = 0;

      ImVec2 scrnpos = timeline_pos;
      scrnpos.x += storage.uiStyle.leftPadding;

      //draw_list->AddLine({ timeline_pos.x, timeline_pos.y+h0/2}, ImVec2{scrnmax.x, timeline_pos.y+h0/2}, IM_COL32(255,128,0,255));
      while (scrnpos.x < scrnmax.x) {
        draw_list->AddLine(scrnpos + ImVec2{0, h0/2}, scrnpos + ImVec2{0, h0}, ImGui::GetColorU32(ImGuiCol_Border));
        if (t % 4 == 0) {
          draw_list->AddLine(scrnpos, scrnpos + ImVec2(0, h0/2), ImGui::GetColorU32(ImGuiCol_Border));
          char tmps[512];
          ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", 1 + (t/4));
          draw_list->AddText(scrnpos + ImVec2{4,4}, IM_COL32_BLACK, tmps);
        }
        scrnpos.x += storage.uiStyle.beatWd;
        t++;
      }

      // Cursor


      float cursor_x = storage.is_cursor_dragging ?
        storage.dragging_cursor_x :
        storage.uiStyle.beatWd * seq->displayState.play_cursor;

      {
        if (seq->IsPlaying()) {
          if ((cursor_x - storage.scroll_x1) < wndsz.x *1/4) {
            storage.scroll_animate = 10;
            storage.scroll_target = (cursor_x - (wndsz.x*1/4)) - 4;
            if (storage.scroll_target < 0) storage.scroll_target = 0;
          } else if ((cursor_x - storage.scroll_x1) > wndsz.x *3/4) {
            storage.scroll_animate = 10;
            storage.scroll_target = 4 + (cursor_x - (wndsz.x*1/4));
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

        // Draw
        ImGui::SetCursorPos({ storage.uiStyle.leftPadding + cursor_x - (storage.uiStyle.cursorWd/2), h0/2});
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

        draw_list->AddConvexPolyFilled(points, IM_ARRAYSIZE(points), ImGui::GetColorU32(ImGuiCol_SliderGrab));
        draw_list->AddLine({ rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y}, { rcmin.x + (storage.uiStyle.cursorWd/2), rcmax.y + tot_h}, ImGui::GetColorU32(ImGuiCol_Border));
      }

      // Contents and Borders R-R
      if (seq->IsFileLoaded()) {
        float pos_x = xy0.x;
        float pos_y = xy0.y + h0;
        for (int r=0; r<seq->GetNumTracks(); r++) {
          DawTrack * track = seq->GetTrack(r);

          ImGui::PushID(r);
          pos_y += track->h;
          HSplitter({pos_x, pos_y}, seq->displayState.play_duration * storage.uiStyle.beatWd, [&]() { track->h = ImMax(track->h + ImGui::GetIO().MouseDelta.y, 20.0f); });
          ImGui::PopID();
          pos_y += 8;
        }
      }

      auto sticky_p = wndpos + ImVec2{0, wndsz.y - ImGui::GetFrameHeightWithSpacing() * 2}; // OR timeline_pos + ImGui::GetScroll();
      draw_list->AddRectFilled(sticky_p, wndpos + wndsz, IM_COL32(255,255,0,255));
      ImGui::SetCursorScreenPos(sticky_p + ImVec2{4,4});
      ImGui::Text("cursor_x = %.1f, drag = %.1f | scroll_x = %.1f max_x = %.1f | dragging = %d",
        cursor_x, storage.dragging_cursor_x,
         storage.scroll_x1, scroll_max_x1, storage.is_cursor_dragging);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
  }
}