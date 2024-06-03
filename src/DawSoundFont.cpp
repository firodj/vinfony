#include "DawSoundFont.hpp"
#include "TsfDev.hpp"
#include <tsf.h>
#include <tsf_internal.h>
#include <imgui.h>
#include <kosongg/IconsFontAwesome6.h>
#include <fmt/core.h>

namespace vinfony {

void DawSoundFont(TinySoundFontDevice * device) {
  tsf * g_TinySoundFont = device->GetTSF();

  static int selectedId = -1;
  static int selectedType = 0;

  ImGui::BeginChild("List", {200, 0}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar);
  std::string label1 = fmt::format("Samples ({})", g_TinySoundFont->sampleNum);
  if (ImGui::TreeNode(label1.c_str()))
  {
    const int max = g_TinySoundFont->sampleNum;
    ImGui::BeginChild("childSamples", {0, 200}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(max);
    while (clipper.Step())
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        std::string label = fmt::format(ICON_FA_FILE_AUDIO " [{}] {}", i+1, g_TinySoundFont->samples[i].sampleName);
        if (ImGui::Selectable(label.c_str(), selectedId == i && selectedType == 0)) {
          selectedId = i;
          selectedType = 0;
        }
      }

    ImGui::EndChild();
    ImGui::TreePop();
  }

  std::string label2 = fmt::format("Presets ({})", g_TinySoundFont->presetNum);
  if (ImGui::TreeNode("Presets"))
  {
    const int max = g_TinySoundFont->presetNum;
    ImGui::BeginChild("childPresets", {0, 200}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(max);
    while (clipper.Step())
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        std::string label = fmt::format(ICON_FA_MUSIC " [{}:{}] {}",
          g_TinySoundFont->presets[i].bank,
          g_TinySoundFont->presets[i].preset+1,
          g_TinySoundFont->presets[i].presetName);
        if (ImGui::Selectable(label.c_str(), selectedId == i && selectedType == 1)) {
          selectedId = i;
          selectedType = 1;
        }
      }

    ImGui::EndChild();
    ImGui::TreePop();
  }

  ImGui::EndChild();

  ImGui::SameLine();

  ImGui::BeginChild("Content", {}, ImGuiChildFlags_None, 0);
  bool noselected = true;
  do {
    if (selectedType == 1) {
      if (selectedId < 0 || selectedId >= g_TinySoundFont->presetNum) break;

      tsf_preset & curPreset = g_TinySoundFont->presets[selectedId];
      ImGui::Text("bank %d", curPreset.bank);
      ImGui::Text("preset %d", curPreset.preset);
      ImGui::Text("name %s", curPreset.presetName);
      ImGui::Text("regionNum %d", curPreset.regionNum);

    } else {
      if (selectedId < 0 || selectedId >= g_TinySoundFont->sampleNum) break;

      tsf_sample & curSample = g_TinySoundFont->samples[selectedId];
      ImGui::Text("name %s", curSample.sampleName);
      ImGui::Text("start %d", curSample.start);
      ImGui::Text("end %d", curSample.end);
      ImGui::Text("startLoop %d", curSample.startLoop);
      ImGui::Text("endLoop %d", curSample.endLoop);
      ImGui::Text("originalPitch %d", curSample.originalPitch);
      ImGui::Text("pitchCorrection %d", curSample.pitchCorrection);
      ImGui::Text("sampleRate %d", curSample.sampleRate);
      ImGui::Text("sampleType %d", curSample.sampleType);
      if (curSample.sampleType & 0x8000) {
        ImGui::SameLine(); ImGui::Text("rom");
      }
      int sampleLinkIdx = -1;
      if ((curSample.sampleType & 0xF) == 1) {
        ImGui::SameLine(); ImGui::Text("mono");
      }
      if ((curSample.sampleType & 0xF) == 2) {
        ImGui::SameLine(); ImGui::Text("right");
        sampleLinkIdx = curSample.sampleLink;
      }
      if ((curSample.sampleType & 0xF) == 4) {
        ImGui::SameLine(); ImGui::Text("left");
        sampleLinkIdx = curSample.sampleLink;
      }
      if ((curSample.sampleType & 0xF) == 8) {
        ImGui::SameLine(); ImGui::Text("linked");
      }
      if (sampleLinkIdx != -1 && sampleLinkIdx < g_TinySoundFont->sampleNum) {
        tsf_sample & linkedSample = g_TinySoundFont->samples[sampleLinkIdx];
        ImGui::Text("sampleLink"); ImGui::SameLine();
        std::string label3 = fmt::format("{} {}", sampleLinkIdx, linkedSample.sampleName);
        if (ImGui::Button(label3.c_str())) {
          selectedId = sampleLinkIdx;
          selectedType = 0;
        }
      }
    }
    noselected = false;
  } while(noselected);
  if (noselected) {
    ImGui::Text("Please select something.");
  }

  ImGui::EndChild();


}

}