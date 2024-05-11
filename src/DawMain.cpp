#include "DawMain.hpp"

#include <map>
#include <imgui.h>

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }

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
};

DawMain::DawMain() {
  m_impl = std::make_unique<Impl>();

  NewProp("No", [](DawPropDrawParam * param) { ImGui::Text("%d", param->r); });
  NewProp("Name", [](DawPropDrawParam * param) { ImGui::Text("%s", param->track->name.c_str() ); });
  NewProp("Channel", nullptr);
  NewProp("Instrument", nullptr);

  NewTrack("Piano");
  NewTrack("Bass");
  NewTrack("Drum");
}

DawMain::~DawMain() {
}

void DawMain::Begin() {

}

void DawMain::End() {
  static float w = 200.0f;

  ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
  // DAW test
  auto conavail = ImGui::GetContentRegionAvail();

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::SetNextWindowScroll(ImVec2{m_impl->scroll_x0, m_impl->scroll_y});
  if (ImGui::BeginChild("child_1", {w, 0.0f}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Border, ImGuiWindowFlags_HorizontalScrollbar)) {
    auto draw_list = ImGui::GetWindowDrawList();
    static int w0 = 200;
    static float f = 0.0f;

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
      pos_x += prop->w + 8;
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

        pos_x += prop->w + 8;
      }
      pos_y += track->h + 8;
    }

    //ImGui::SliderFloat("float", &f, 0.0f, 1.0f);           // Edit 1 float using a slider from 0.0f to 1.0f

    // Borders C
    pos_x = xy0.x;
    ImGui::SetCursorPosY(xy0.y);
    for (int c=0; c<m_impl->prop_nums.size(); c++) {
      int prop_id = m_impl->prop_nums[c];
      DawProp * prop = m_impl->props[prop_id].get();
      if (c > 0) ImGui::SameLine();
      pos_x += prop->w;
      ImGui::SetCursorPosX(pos_x);

      ImGui::PushID(c);
      ImGui::InvisibleButton("csplitter", ImVec2(8, avail.y));
      auto rcmin = ImGui::GetItemRectMin();
      auto rcmax = ImGui::GetItemRectMax();
      if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      }
      if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          prop->w += ImGui::GetIO().MouseDelta.x;
      }
      rcmin.x += 3;
      rcmax.x -= 3;
      draw_list->AddRectFilled(rcmin, rcmax, color_border);
      ImGui::PopID();

      pos_x += 8;
    }

    // Borders R
    pos_x = xy0.x;
    pos_y = xy0.y + ImGui::GetFrameHeightWithSpacing();
    for (int r=0; r<m_impl->track_nums.size(); r++) {
      int track_id = m_impl->track_nums[r];
      DawTrack * track = m_impl->tracks[track_id].get();

      ImGui::PushID(r);
      ImGui::SetCursorPosX(pos_x);
      pos_y += track->h;
      ImGui::SetCursorPosY(pos_y);
      ImGui::InvisibleButton("rsplitter", ImVec2(avail.x, 8));
      auto rcmin = ImGui::GetItemRectMin();
      auto rcmax = ImGui::GetItemRectMax();
      if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
      }
      if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          track->h += ImGui::GetIO().MouseDelta.y;
      }

      rcmin.y += 3;
      rcmax.y -= 3;
      draw_list->AddRectFilled(rcmin, rcmax, color_border);

      //draw_list->AddLine(cursor + ImVec2{0, (r+2) * ImGui::GetFrameHeightWithSpacing()},
      //  cursor + ImVec2{legend.x, (r+2) * ImGui::GetFrameHeightWithSpacing()}, IM_COL32(255,  0, 0, 255));
      ImGui::PopID();

      pos_y += 8;
    }

    ImGui::SetCursorPos({0, 1500});
    m_impl->scroll_y = ImGui::GetScrollY();
    m_impl->scroll_x0 = ImGui::GetScrollX();
  }
  ImGui::EndChild();

  ImGui::SameLine();
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
      ImGui::SetCursorPosX(pos_x);
      pos_y += track->h;
      ImGui::SetCursorPosY(pos_y);
      ImGui::InvisibleButton("rsplitter", ImVec2(avail.x, 8));
      auto rcmin = ImGui::GetItemRectMin();
      auto rcmax = ImGui::GetItemRectMax();
      if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
      }
      if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
          track->h += ImGui::GetIO().MouseDelta.y;
      }

      rcmin.y += 3;
      rcmax.y -= 3;
      draw_list->AddRectFilled(rcmin, rcmax, color_border);

      //draw_list->AddLine(cursor + ImVec2{0, (r+2) * ImGui::GetFrameHeightWithSpacing()},
      //  cursor + ImVec2{legend.x, (r+2) * ImGui::GetFrameHeightWithSpacing()}, IM_COL32(255,  0, 0, 255));
      ImGui::PopID();

      pos_y += 8;
    }

    ImGui::SetCursorPos({1500, 1500});
    m_impl->scroll_y = ImGui::GetScrollY();
    m_impl->scroll_x1 = ImGui::GetScrollX();
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
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