#include "DawMain.hpp"

#include <map>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

struct DawMain::Impl {
  std::map<int, std::unique_ptr<DawTrack>> tracks;
  std::map<int, std::unique_ptr<DawProp>> props;
  std::vector<int> track_nums;
  std::vector<int> prop_nums;

  float scroll_y = 0.0f;
  float scroll_x0 = 0.0f;
  float scroll_x1 = 0.0f;

  int last_tracks_id = 0;
  int last_props_id = 0;
  bool main_opened = true;
  float split_thick = 8;
};

DawMain::DawMain() {
  m_impl = std::make_unique<Impl>();

  NewProp("No", [](DawPropDrawParam * param) { ImGui::Text("%d", param->r); });
  NewProp("Name", [](DawPropDrawParam * param) {
    auto p1 = ImGui::GetCursorScreenPos();
    auto p2 = p1 + ImVec2{(float)param->self->w, (float)param->track->h};
    ImGui::PushClipRect(p1, p2, false);
    // ImGui::GetWindowDrawList()->AddRectFilled(p1, p2, IM_COL32(255, 0,0, 255));
    ImGui::Text("%s", param->track->name.c_str() );
    ImGui::PopClipRect();
  });
  NewProp("Channel", [](DawPropDrawParam * param) {
    auto p1 = ImGui::GetCursorScreenPos();
    auto p2 = p1 + ImVec2{(float)param->self->w, (float)param->track->h};

    const char* items[] = {"-", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
    static int item_current = 0;
    ImGui::SetNextItemWidth(param->self->w);
    ImGui::PushID(param->track->id);
    ImGui::Combo("##channel", &item_current, items, IM_ARRAYSIZE(items));
    ImGui::PopID();
  });
  NewProp("Instrument", nullptr);

  NewTrack("Piano");
  NewTrack("Bass");
  NewTrack("Drum");
}

DawMain::~DawMain() {
}

bool DawMain::Begin() {
  ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
  m_impl->main_opened = ImGui::Begin("Vinfony Project");
  return m_impl->main_opened;
}

void DawMain::VSplitter(float pos_x, float pos_y, float avail_h, SplitterOnDraggingFunc func) {
  auto draw_list     = ImGui::GetWindowDrawList();
  ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
  ImU32 color_hover  = ImGui::GetColorU32(ImGuiCol_SeparatorHovered, 1.0);
  ImU32 color_active  = ImGui::GetColorU32(ImGuiCol_SeparatorActive, 1.0);

  ImGui::SetCursorPos({ pos_x, pos_y });
  ImGui::InvisibleButton("vsplitter", ImVec2{m_impl->split_thick, (float)ImMax(avail_h, 1.0f)});
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
  rcmin.x += m_impl->split_thick/2;
  rcmax.x = rcmin.x;
  draw_list->AddLine(rcmin, rcmax, color_border);
}

void DawMain::HSplitter(float pos_x, float pos_y, float avail_w, SplitterOnDraggingFunc func) {
  auto draw_list     = ImGui::GetWindowDrawList();
  ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
  ImU32 color_hover  = ImGui::GetColorU32(ImGuiCol_SeparatorHovered, 1.0);
  ImU32 color_active = ImGui::GetColorU32(ImGuiCol_SeparatorActive, 1.0);

  ImGui::SetCursorPos({pos_x, pos_y});
  ImGui::InvisibleButton("hsplitter", ImVec2{(float)ImMax(avail_w, 1.0f), m_impl->split_thick});
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
  rcmin.y += m_impl->split_thick/2;
  rcmax.y = rcmin.y;
  draw_list->AddLine(rcmin, rcmax, color_border);
}

void DawMain::End() {
  static float w = 200.0f;

  if (m_impl->main_opened) {
    ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
    // DAW test
    auto conavail = ImGui::GetContentRegionAvail();

    //ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    ImGui::SetNextWindowScroll(ImVec2{m_impl->scroll_x0, m_impl->scroll_y});
    if (ImGui::BeginChild("child_1", {w, 0.0f}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar)) {
      auto draw_list = ImGui::GetWindowDrawList();

      auto cursor = ImGui::GetCursorScreenPos();
      auto legend = ImGui::GetWindowSize();

      auto xy0 = ImGui::GetCursorPos();
      auto avail = ImGui::GetContentRegionAvail();

      // Row 0: Header
      int pos_x = xy0.x;
      for (int c=0; c<m_impl->prop_nums.size(); c++) {
        int prop_id = m_impl->prop_nums[c];
        DawProp * prop = m_impl->props[prop_id].get();
        if (c > 0) ImGui::SameLine();
        ImGui::SetCursorPosX(pos_x);
        ImGui::Button(prop->name.c_str(), {(float)prop->w, 0});
        pos_x += prop->w + m_impl->split_thick;
      }

      // Row N: Track
      int pos_y = xy0.y + ImGui::GetFrameHeightWithSpacing();
      for (int r=0; r<m_impl->track_nums.size(); r++) {
        int track_id = m_impl->track_nums[r];
        DawTrack * track = m_impl->tracks[track_id].get();
        pos_x = xy0.x;
        ImGui::SetCursorPosY(pos_y);

        for (int c=0; c<m_impl->prop_nums.size(); c++) {
          int prop_id = m_impl->prop_nums[c];
          DawProp * prop = m_impl->props[prop_id].get();

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

          pos_x += prop->w + m_impl->split_thick;
        }
        pos_y += track->h + m_impl->split_thick;
      }

      //ImGui::SliderFloat("float", &f, 0.0f, 1.0f);           // Edit 1 float using a slider from 0.0f to 1.0f

      // Borders C
      pos_x = xy0.x;
      pos_y = xy0.y;
      ImGui::SetCursorPosY(xy0.y);
      for (int c=0; c<m_impl->prop_nums.size(); c++) {
        int prop_id = m_impl->prop_nums[c];
        DawProp * prop = m_impl->props[prop_id].get();
        pos_x += prop->w;
        ImGui::PushID(c);
        VSplitter(pos_x, pos_y, avail.y, [&]() { prop->w = ImMax(prop->w + ImGui::GetIO().MouseDelta.x, 20.0f); });
        ImGui::PopID();
        pos_x += m_impl->split_thick;
      }

      // Borders R
      pos_x = xy0.x;
      pos_y = xy0.y + ImGui::GetFrameHeightWithSpacing();
      for (int r=0; r<m_impl->track_nums.size(); r++) {
        int track_id = m_impl->track_nums[r];
        DawTrack * track = m_impl->tracks[track_id].get();
        pos_y += track->h;
        ImGui::PushID(r);
        HSplitter(pos_x, pos_y, avail.x, [&]() { track->h = ImMax(track->h + ImGui::GetIO().MouseDelta.y, 20.0f); });
        ImGui::PopID();
        pos_y += m_impl->split_thick;
      }

      ImGui::SetCursorPos({0, 1500});
      m_impl->scroll_y = ImGui::GetScrollY();
      m_impl->scroll_x0 = ImGui::GetScrollX();
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 0);
  #if 0 // WITHOUT  SPLITTER
    {
      auto drawlist = ImGui::GetWindowDrawList();
      auto cursor = ImGui::GetCursorScreenPos();
      ImGui::InvisibleButton("v_splitter", {8.0, (float)ImMax(ImGui::GetWindowSize().y - ImGui::GetCursorPosY() - 10.0, 1.0)});
      if (ImGui::IsItemHovered()) {
        //if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      }
      if(ImGui::IsItemActive()) {
          w += ImGui::GetIO().MouseDelta.x;
      }
      auto button = ImGui::GetItemRectSize();
      auto rcmax = ImGui::GetItemRectMax(); // = cursor+button ?
      drawlist->AddRect(cursor, cursor+button, IM_COL32(255, 0,0, 255));
    }
    ImGui::SameLine();
  #endif

    ImGui::SetNextWindowScroll(ImVec2{m_impl->scroll_x1, m_impl->scroll_y});
    if (ImGui::BeginChild("child_2", {0, 0}, true, ImGuiWindowFlags_HorizontalScrollbar)) {
      auto draw_list = ImGui::GetWindowDrawList();
      auto cursor = ImGui::GetCursorScreenPos();
      auto xy0 = ImGui::GetCursorPos();
      auto avail = ImGui::GetContentRegionAvail();

      // Borders R
      int pos_x = xy0.x;
      int pos_y = xy0.y + ImGui::GetFrameHeightWithSpacing();
      for (int r=0; r<m_impl->track_nums.size(); r++) {
        int track_id = m_impl->track_nums[r];
        DawTrack * track = m_impl->tracks[track_id].get();

        ImGui::PushID(r);
        pos_y += track->h;
        HSplitter(pos_x, pos_y, avail.x, [&]() { track->h = ImMax(track->h + ImGui::GetIO().MouseDelta.y, 20.0f); });
        ImGui::PopID();
        pos_y += 8;
      }

      ImGui::SetCursorPos({1500, 1500});
      m_impl->scroll_y = ImGui::GetScrollY();
      m_impl->scroll_x1 = ImGui::GetScrollX();
    }
    ImGui::EndChild();

    // ImGui::PopStyleVar();
  }

  ImGui::End();
}

int DawMain::NewTrack(std::string name) {
  m_impl->last_tracks_id++;
  m_impl->tracks[m_impl->last_tracks_id] = std::make_unique<DawTrack>();

  auto track = m_impl->tracks[m_impl->last_tracks_id].get();
  track->id = m_impl->last_tracks_id;
  track->name = name;
  track->h = 20;

  m_impl->track_nums.push_back(track->id);

  return track->id;
}

int DawMain::NewProp(std::string name, DawPropDrawFunc func) {
  m_impl->last_props_id++;
  m_impl->props[m_impl->last_props_id] = std::make_unique<DawProp>();

  auto prop = m_impl->props[m_impl->last_props_id].get();
  prop->id = m_impl->last_props_id;
  prop->name = name;
  prop->w = 100;
  prop->DrawTrack = func;

  m_impl->prop_nums.push_back(prop->id);

  return prop->id;
}