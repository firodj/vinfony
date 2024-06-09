#include "DawSoundFont.hpp"
#include "TsfDev.hpp"
#include <tsf.h>
#include <tsf_internal.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <kosongg/IconsFontAwesome6.h>
#include <fmt/core.h>
#include "PianoButton.hpp"
namespace vinfony {

void DawSoundFont(TinySoundFontDevice * device) {
  tsf * g_TinySoundFont = device->GetTSF();
  if (!g_TinySoundFont) {
    ImGui::Text("SoundFont missing");
    return;
  }

  static int selectedId = -1;
  static int selectedType = 0;
  static int regionSelected = -1;

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
          regionSelected = -1;
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
      if (ImGui::InputScalar("Bank", ImGuiDataType_U16, &curPreset.bank, NULL, NULL, "%d")) {
        if (curPreset.bank < 0) curPreset.bank = 0;
      }
      if (ImGui::InputScalar("Program", ImGuiDataType_U16, &curPreset.preset, NULL, NULL, "%d")) {
        if (curPreset.preset < 0) curPreset.preset = 0;
      }
      ImGui::InputText("Name", curPreset.presetName, 20);
      ImGui::LabelText("Regions #", "%d", curPreset.regionNum);

      ImGui::BeginChild("regionList", {0, 0}, ImGuiChildFlags_ResizeX, ImGuiWindowFlags_None);
      ImVec2 wndpos = ImGui::GetWindowPos();
      ImVec2 wndmax = wndpos + ImGui::GetWindowSize();

      ImVec2 pianopos = wndpos - ImVec2{ImGui::GetScrollX(), 0};
      ImGui::SetCursorScreenPos(pianopos);
      PianoButton("piano");
      ImVec2 pianoSz = ImGui::GetItemRectSize();

      //ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetCursorScreenPos(), wndmax, IM_COL32_WHITE);
      ImGui::PushClipRect(ImGui::GetCursorScreenPos(), wndmax, true);
      ImVec2 instpos = ImVec2{0, ImGui::GetCursorPos().y - ImGui::GetScrollY()};

      int lastInstrumentID = -1; // HackyWay
      for (int i = 0; i < curPreset.regionNum; i++) {
        tsf_region & curRegion = curPreset.regions[i];

        if (lastInstrumentID == -1) lastInstrumentID = curRegion.instrumentID;
        else if (lastInstrumentID != curRegion.instrumentID) {
          ImVec2 barpos = wndpos + instpos + ImVec2{0, 18};
          ImGui::GetWindowDrawList()->AddLine(barpos, barpos + ImVec2{pianoSz.x, 0}, IM_COL32(128, 128,128, 255));
          lastInstrumentID = curRegion.instrumentID;
          instpos.y += 20;
        }

        ImGui::SetCursorPos(instpos);
        ImGui::PushID(i);
        if (PianoRegion("region", curRegion.lokey, curRegion.hikey, curRegion.pitch_keycenter, regionSelected == i)) {
          regionSelected = i;
        }
        ImGui::PopID();
      }

      ImVec2 barpos = wndpos + instpos + ImVec2{0, 18};
      ImGui::GetWindowDrawList()->AddLine(barpos, barpos + ImVec2{pianoSz.x, 0}, IM_COL32(128, 128,128, 255));

      ImGui::PopClipRect();

      ImGui::Text("DEBUG: %f", pianoSz.x);
#if 0
      if (ImGui::TreeNode("Regions")) {
        ImGui::BeginChild("childRegions", {0, 200}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGuiListClipper clipper;
        clipper.Begin(curPreset.regionNum);
        while (clipper.Step())
          for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            tsf_region & curRegion = curPreset.regions[i];
            std::string label4 = fmt::format("[{}]", i);
            if (ImGui::Selectable(label4.c_str(), i == regionSelected)) {
              regionSelected = i;
            }
            if (curRegion.sampleID >= 0) {
              tsf_sample & regionSample = g_TinySoundFont->samples[curRegion.sampleID];
              ImGui::SameLine(); ImGui::Text("sampleRegion"); ImGui::SameLine();
              std::string label3 = fmt::format("{} {}", curRegion.sampleID, regionSample.sampleName);
              if (ImGui::Button(label3.c_str())) {
                selectedId = curRegion.sampleID;
                selectedType = 0;
              }
            }
          }
        ImGui::EndChild();
        ImGui::TreePop();
      }
#endif
      ImGui::EndChild();
      ImGui::SameLine();
      ImGui::BeginChild("regionContent", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);
      if (regionSelected >= 0) {
        if (regionSelected >= curPreset.regionNum) regionSelected = 0;
        tsf_region & curRegion = curPreset.regions[regionSelected];

        ImGui::LabelText("InsturmentID", "%d", curRegion.instrumentID);
        ImGui::LabelText("Key", "%d - %d", curRegion.lokey, curRegion.hikey);
        ImGui::LabelText("Vel", "%d - %d", curRegion.lovel, curRegion.hivel);
        ImGui::LabelText("Attenuation", "%f", curRegion.attenuation);
        ImGui::LabelText("Pan", "%f", curRegion.pan);
        ImGui::LabelText("CoarseTune", "%d", curRegion.transpose);
        ImGui::LabelText("FineTune", "%d", curRegion.tune);
        ImGui::LabelText("ExclusiveClass", "%d", curRegion.group);
        ImGui::LabelText("ScaleTuning", "%d", curRegion.pitch_keytrack);
        ImGui::LabelText("OverridingRootKey", "%d", curRegion.pitch_keycenter);

        ImGui::LabelText("StartAddrsOffset", "%d", curRegion.offset);
        ImGui::LabelText("EndAddrsOffset", "%d", curRegion.end);
        ImGui::LabelText("StartloopAddrsOffset", "%d", curRegion.loop_start);
        ImGui::LabelText("EndloopAddrsOffset", "%d", curRegion.loop_end);
        ImGui::LabelText("SampleModes", "%d", curRegion.loop_mode);

        if (curRegion.sampleID >= 0) {
          tsf_sample & regionSample = g_TinySoundFont->samples[curRegion.sampleID];

          std::string label3 = fmt::format("{} {}", curRegion.sampleID, regionSample.sampleName);
          if (ImGui::Button(label3.c_str())) {
            selectedId = curRegion.sampleID;
            selectedType = 0;
          }
          ImGui::LabelText("SampleRate", "%d", curRegion.sample_rate);
        }

        ImGui::LabelText("ModLfoToPitch", "%d", curRegion.modLfoToPitch);
        ImGui::LabelText("VibLfoToPitch", "%d", curRegion.vibLfoToPitch);
        ImGui::LabelText("ModEnvToPitch", "%d", curRegion.modEnvToPitch);
        ImGui::LabelText("InitialFilterFc", "%d", curRegion.initialFilterFc);
        ImGui::LabelText("InitialFilterQ", "%d", curRegion.initialFilterQ);
        ImGui::LabelText("ModLfoToFilterFc", "%d", curRegion.modLfoToFilterFc);
        ImGui::LabelText("ModEnvToFilterFc", "%d", curRegion.modEnvToFilterFc);
        ImGui::LabelText("ModLfoToVolume", "%d", curRegion.modLfoToVolume);

        ImGui::LabelText("ChorusSend", "%f", curRegion.chorusSend);
        ImGui::LabelText("ReverbSend", "%f", curRegion.reverbSend);

        ImGui::LabelText("DelayModLFO", "%f", curRegion.delayModLFO);
        ImGui::LabelText("FreqModLFO", "%d", curRegion.freqModLFO);
        ImGui::LabelText("DelayVibLFO", "%f", curRegion.delayVibLFO);
        ImGui::LabelText("FreqVibLFO", "%d", curRegion.freqVibLFO);

        if (ImGui::TreeNode("ModEnv")) {
          ImGui::LabelText("DelayModEnv", "%f", curRegion.modenv.delay);
          ImGui::LabelText("AttackModEnv", "%f", curRegion.modenv.attack);
          ImGui::LabelText("HoldModEnv", "%f", curRegion.modenv.hold);
          ImGui::LabelText("DecayModEnv", "%f", curRegion.modenv.decay);
          ImGui::LabelText("SustainModEnv", "%f", curRegion.modenv.sustain);
          ImGui::LabelText("ReleaseModEnv", "%f", curRegion.modenv.release);
          ImGui::LabelText("KeynumToModEnvHold", "%f", curRegion.modenv.keynumToHold);
          ImGui::LabelText("KeynumToModEnvDecay", "%f", curRegion.modenv.keynumToDecay);
          ImGui::TreePop();
        }

        if (ImGui::TreeNode("VolEnv")) {
          ImGui::LabelText("DelayVolEnv", "%f", curRegion.ampenv.delay);
          ImGui::LabelText("AttackVolEnv", "%f", curRegion.ampenv.attack);
          ImGui::LabelText("HoldVolEnv", "%f", curRegion.ampenv.hold);
          ImGui::LabelText("DecayVolEnv", "%f", curRegion.ampenv.decay);
          ImGui::LabelText("SustainVolEnv", "%f", curRegion.ampenv.sustain);
          ImGui::LabelText("ReleaseVolEnv", "%f", curRegion.ampenv.release);
          ImGui::LabelText("KeynumToVolEnvHold", "%f", curRegion.ampenv.keynumToHold);
          ImGui::LabelText("KeynumToVolEnvDecay", "%f", curRegion.ampenv.keynumToDecay);
          ImGui::TreePop();
        }


        ImGui::LabelText("Modulators #", "%d", curRegion.modulatorNum);

        for (int i=0; i <curRegion.modulatorNum; i++) {
          tsf_modulator & curModulator = curRegion.modulators[i];
          ImGui::Text("0x%x (index:%d cc:%d d:%d p:%d type:%d) gen:%d %d amount:%d trans:%d", curModulator.modSrcOper,
            curModulator.modSrcOperDetails.index,
            curModulator.modSrcOperDetails.cc,
            curModulator.modSrcOperDetails.d,
            curModulator.modSrcOperDetails.p,
            curModulator.modSrcOperDetails.type,
            curModulator.modDestOper, curModulator.modAmtSrcOper, curModulator.modAmount, curModulator.modTransOper);
        }
      }
      ImGui::EndChild();

    } else {
      if (selectedId < 0 || selectedId >= g_TinySoundFont->sampleNum) break;

      tsf_sample & curSample = g_TinySoundFont->samples[selectedId];
      ImGui::LabelText("Name", "%s", curSample.sampleName);
      ImGui::LabelText("Start", "%d", curSample.start);
      ImGui::LabelText("End", "%d", curSample.end);
      ImGui::LabelText("Start Loop", "%d", curSample.startLoop);
      ImGui::LabelText("End Loop", "%d", curSample.endLoop);
      ImGui::LabelText("Original Pitch", "%d", curSample.originalPitch);
      ImGui::LabelText("Pitch Correction", "%d", curSample.pitchCorrection);
      ImGui::LabelText("Sample Rate", "%d", curSample.sampleRate);
      ImGui::LabelText("Sample Type", "%d", curSample.sampleType);
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