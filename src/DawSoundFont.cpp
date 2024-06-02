#include "DawSoundFont.hpp"
#include "TsfDev.hpp"
#include <tsf.h>
#include <tsf_internal.h>
#include <imgui.h>

namespace vinfony {

void DawSoundFont(TinySoundFontDevice * device) {
  tsf * g_TinySoundFont = device->GetTSF();
  const int max = tsf_get_presetcount(g_TinySoundFont);
  ImGui::Text("presetNum: %d", g_TinySoundFont->presetNum);
  for (int i=0; i<max; i++) {
    ImGui::Text("%d) %s) bank=%d preset=%d regionNum=%d", i,
      g_TinySoundFont->presets[i].presetName,
      g_TinySoundFont->presets[i].bank,
      g_TinySoundFont->presets[i].preset,
      g_TinySoundFont->presets[i].regionNum
    );
  }
}

}