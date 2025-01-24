#include "DawSoundFont.hpp"

#include <functional>

#include "TsfDev.hpp"
#include <tsf.h>
#include <tsf_internal.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <kosongg/IconsFontAwesome6.h>
#include <fmt/core.h>
#include "PianoButton.hpp"
#include "UI.hpp"

#include "../Globals.hpp"

hscpp_require_include_dir("${projPath}/src")
hscpp_require_include_dir("${projPath}/kosongg/cpp")
hscpp_require_include_dir("${projPath}/ext/jdksmidi/include")
hscpp_require_include_dir("${projPath}/ext/imgui-docking")
hscpp_require_include_dir("${projPath}/ext/fmt/include")
hscpp_require_include_dir("${projPath}/ext/tsf")
hscpp_require_include_dir("${projPath}/ext/hscpp/extensions/mem/include")

hscpp_require_source("PianoButton.cpp")
hscpp_require_source("Splitter.cpp")
hscpp_require_preprocessor_def("IMGUI_USER_CONFIG=\\\"${imguiUserConfig}\\\"", "imgui_IMPORTS")

hscpp_if (os == "Windows")
	hscpp_require_library("${buildPath}/kosongg/cmake/imgui/Debug/imgui.lib")
	hscpp_require_library("${buildPath}/ext/tsf/Debug/tsf.lib")
	hscpp_require_library("${buildPath}/ext/fmt/Debug/fmtd.lib")
	hscpp_require_library("${buildPath}/ext/hscpp/extensions/mem/Debug/hscpp-mem.lib")
hscpp_elif (os == "Posix")
	hscpp_require_library("${projPath}/bin/libimgui.dylib")
	hscpp_require_library("${buildPath}/ext/tsf/Debug/libtsf.a")
	hscpp_require_library("${projPath}/bin/libfmtd.dylib")
	hscpp_require_library("${projPath}/bin/libhscpp-mem.dylib")
hscpp_else()
	// Diagnostic messages can be printed to the build output with hscpp_message.
	hscpp_message("Unknown OS ${os}.")
hscpp_end()

namespace vinfony {

void DawSoundFont::Draw(tsf *g_TinySoundFont)
{
	if (!g_TinySoundFont)
	{
		ImGui::Text("SoundFont missing");
		return;
	}

	if (ImGui::BeginChild("List", {200, 0}, ImGuiChildFlags_ResizeX | ImGuiChildFlags_Border))
	{
		ImGuiContext& g = *GImGui;
		auto wndsz = ImGui::GetWindowSize();

		std::string label1 = fmt::format("Samples ({})", g_TinySoundFont->sampleNum);
		if (ImGui::TreeNode(label1.c_str()))
		{
			const int max = g_TinySoundFont->sampleNum;
			if (ImGui::BeginChild("childSamples", {0, m_heightSamples}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
			{
				ImGuiListClipper clipper;
				clipper.Begin(max);
				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
						std::string label = fmt::format(ICON_FA_FILE_AUDIO " [{}] {}", i+1, g_TinySoundFont->samples[i].sampleName);
						if (ImGui::Selectable(label.c_str(), m_selectedId == i && m_selectedType == 0)) {
							m_selectedId = i;
							m_selectedType = 0;
						}
					}
				}
			}
			ImGui::EndChild();

			ImGui::TreePop();
		}

		{
			auto cursor = ImGui::GetCursorPos();
			HSplitter(cursor, wndsz.x - g.Style.WindowPadding.x*2, [&]() { m_heightSamples = ImMax(m_heightSamples + ImGui::GetIO().MouseDelta.y, 200.0f); });
		}

		std::string label2 = fmt::format("Presets ({})", g_TinySoundFont->presetNum);
		if (ImGui::TreeNode(label2.c_str()))
		{
			auto cursor = ImGui::GetCursorPos();
			float hPresets = wndsz.y - cursor.y - g.Style.WindowPadding.y;

			const int max = g_TinySoundFont->presetNum;
			if (ImGui::BeginChild("childPresets", {0, hPresets}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
			{
				ImGuiListClipper clipper;
				clipper.Begin(max);
				while (clipper.Step())
				{
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
						std::string label = fmt::format(ICON_FA_MUSIC " [{}:{}] {}",
							g_TinySoundFont->presets[i].bank,
							g_TinySoundFont->presets[i].preset+1,
							g_TinySoundFont->presets[i].presetName);
						if (ImGui::Selectable(label.c_str(), m_selectedId == i && m_selectedType == 1)) {
							m_selectedId = i;
							m_selectedType = 1;
							m_regionSelected = -1;
						}
					}
				}
			}
			ImGui::EndChild();
			ImGui::TreePop();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	if (ImGui::BeginChild("Content", {}, ImGuiChildFlags_None, 0))
	{
		bool noselected = true;
		do {
			if (m_selectedType == 1) {
				if (m_selectedId < 0 || m_selectedId >= g_TinySoundFont->presetNum) break;

				tsf_preset & curPreset = g_TinySoundFont->presets[m_selectedId];

				auto TableRowCustom = [&](std::string label, std::function<void()> draw) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(label.c_str());

					ImGui::TableNextColumn();

					draw();
				};

				if (ImGui::BeginTable("##properties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerH))
            	{
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 2.0f); // Default twice larger

					TableRowCustom("Bank", [&]() {
						if (ImGui::InputScalar("##Bank", ImGuiDataType_U16, &curPreset.bank, NULL, NULL, "%d")) {
							if (curPreset.bank < 0) curPreset.bank = 0;
						}
					});
					TableRowCustom("Program", [&]() {
						if (ImGui::InputScalar("##Program", ImGuiDataType_U16, &curPreset.preset, NULL, NULL, "%d")) {
							if (curPreset.preset < 0) curPreset.preset = 0;
						}
					});
					TableRowCustom("Name", [&]() {
						ImGui::InputText("##Name", curPreset.presetName, 20);
					});
					TableRowCustom("Regions", [&]() {
						ImGui::Text("%d", curPreset.regionNum);
					});

					ImGui::EndTable();
				}

				if (ImGui::BeginChild("regionList", {120, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX, ImGuiWindowFlags_None))
				{
					ImVec2 wndpos = ImGui::GetWindowPos();
					ImVec2 wndmax = wndpos + ImGui::GetWindowSize();

					ImVec2 pianopos = wndpos - ImVec2{ImGui::GetScrollX(), 0};
					ImGui::SetCursorScreenPos(pianopos);
					m_pPianoButton->DrawH();
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
						if (m_pPianoButton->DrawRegion("region", curRegion.lokey, curRegion.hikey, curRegion.pitch_keycenter, m_regionSelected == i)) {
							m_regionSelected = i;
						}
						ImGui::PopID();
					}

					ImVec2 barpos = wndpos + instpos + ImVec2{0, 18};
					ImGui::GetWindowDrawList()->AddLine(barpos, barpos + ImVec2{pianoSz.x, 0}, IM_COL32(128, 128,128, 255));

					ImGui::PopClipRect();

					ImGui::Text("DEBUG: %f", pianoSz.x);
		#if 0
					if (ImGui::TreeNode("Regions")) {
						if (ImGui::BeginChild("childRegions", {0, 200}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
						{
							ImGuiListClipper clipper;
							clipper.Begin(curPreset.regionNum);
							while (clipper.Step())
							{
								for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
									tsf_region & curRegion = curPreset.regions[i];
									std::string label4 = fmt::format("[{}]", i);
									if (ImGui::Selectable(label4.c_str(), i == m_regionSelected)) {
										m_regionSelected = i;
									}
									if (curRegion.sampleID >= 0) {
										tsf_sample & regionSample = g_TinySoundFont->samples[curRegion.sampleID];
										ImGui::SameLine(); ImGui::Text("sampleRegion"); ImGui::SameLine();
										std::string label3 = fmt::format("{} {}", curRegion.sampleID, regionSample.sampleName);
										if (ImGui::Button(label3.c_str())) {
											m_selectedId = curRegion.sampleID;
											m_selectedType = 0;
										}
									}
								}
							}
						}
						ImGui::EndChild();
						ImGui::TreePop();
					}
		#endif
				}
				ImGui::EndChild();

				ImGui::SameLine();
				if (ImGui::BeginChild("regionContent", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
				{
					if (m_regionSelected >= 0) {
						if (m_regionSelected >= curPreset.regionNum) m_regionSelected = 0;
						tsf_region & curRegion = curPreset.regions[m_regionSelected];

						ImGui::LabelText("InsturmentID", "%d", curRegion.instrumentID);
						ImGui::LabelText("Key", "%d - %d", curRegion.lokey, curRegion.hikey);
						ImGui::LabelText("Vel", "%d - %d", curRegion.lovel, curRegion.hivel);
						ImGui::LabelText("OverridingRootKey", "%d", curRegion.pitch_keycenter);
						ImGui::LabelText("CoarseTune", "%d", curRegion.transpose);
						ImGui::LabelText("FineTune", "%d", curRegion.tune);
						ImGui::LabelText("ScaleTuning", "%d", curRegion.pitch_keytrack);
						ImGui::LabelText("ExclusiveClass", "%d", curRegion.group);

						if (ImGui::TreeNode("Sample")) {
							ImGui::LabelText("StartAddrsOffset", "%d", curRegion.offset);
							ImGui::LabelText("EndAddrsOffset", "%d", curRegion.end);
							ImGui::LabelText("StartloopAddrsOffset", "%d", curRegion.loop_start);
							ImGui::LabelText("EndloopAddrsOffset", "%d", curRegion.loop_end);
							ImGui::LabelText("SampleModes", "%d", curRegion.loop_mode);

							if (curRegion.sampleID >= 0) {
								tsf_sample & regionSample = g_TinySoundFont->samples[curRegion.sampleID];

								std::string label3 = fmt::format("{} {}", curRegion.sampleID, regionSample.sampleName);
								if (ImGui::Button(label3.c_str())) {
									m_selectedId = curRegion.sampleID;
									m_selectedType = 0;
								}
								ImGui::LabelText("SampleRate", "%d", curRegion.sample_rate);
							}
							ImGui::TreePop();
						}

						ImGui::LabelText("Attenuation", "%f", curRegion.attenuation);
						ImGui::LabelText("Pan", "%f", curRegion.pan);
						ImGui::LabelText("InitialFilterFc", "%.3f Hz (%d)", tsf_cents2Hertz(curRegion.initialFilterFc), curRegion.initialFilterFc);
						ImGui::LabelText("InitialFilterQ", "%d", curRegion.initialFilterQ);
						ImGui::LabelText("ChorusSend", "%f", curRegion.chorusSend);
						ImGui::LabelText("ReverbSend", "%f", curRegion.reverbSend);

						if (ImGui::TreeNode("ModLfo")) {
							ImGui::LabelText("Delay", "%f", curRegion.delayModLFO);
							ImGui::LabelText("Freq", "%.3f Hz (%d)", tsf_cents2Hertz(curRegion.freqModLFO), curRegion.freqModLFO);
							ImGui::TreePop();
						}

						if (ImGui::TreeNode("VibLfo")) {
							ImGui::LabelText("Delay", "%f", curRegion.delayVibLFO);
							ImGui::LabelText("Freq", "%.3f Hz (%d)", tsf_cents2Hertz(curRegion.freqVibLFO), curRegion.freqVibLFO);
							ImGui::TreePop();
						}

						ImGui::LabelText("ModLfoToFilterFc", "%d cent fs", curRegion.modLfoToFilterFc);
						ImGui::LabelText("ModEnvToFilterFc", "%d cent fs", curRegion.modEnvToFilterFc);
						ImGui::LabelText("ModLfoToPitch", "%d cent fs", curRegion.modLfoToPitch);
						ImGui::LabelText("VibLfoToPitch", "%d cent fs", curRegion.vibLfoToPitch);
						ImGui::LabelText("ModEnvToPitch", "%d cent fs", curRegion.modEnvToPitch);
						ImGui::LabelText("ModLfoToVolume", "%d cB fs", curRegion.modLfoToVolume);

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
							ImGui::LabelText("DelayVolEnv", "%f s", curRegion.ampenv.delay);
							ImGui::LabelText("AttackVolEnv", "%f s", curRegion.ampenv.attack);
							ImGui::LabelText("HoldVolEnv", "%f s", curRegion.ampenv.hold);
							ImGui::LabelText("DecayVolEnv", "%f s", curRegion.ampenv.decay);
							ImGui::LabelText("SustainVolEnv", "%.2f dB", tsf_gainToDecibels(curRegion.ampenv.sustain));
							ImGui::LabelText("ReleaseVolEnv", "%f s", curRegion.ampenv.release);
							ImGui::LabelText("KeynumToVolEnvHold", "%f", curRegion.ampenv.keynumToHold);
							ImGui::LabelText("KeynumToVolEnvDecay", "%f", curRegion.ampenv.keynumToDecay);
							ImGui::TreePop();
						}

						for (int i=0; i <curRegion.modulatorNum; i++) {

							tsf_modulator & curModulator = curRegion.modulators[i];
							std::string label = fmt::format("Modulator {}", i);
							if (ImGui::TreeNode(label.c_str())) {
								if (ImGui::TreeNode("Source1")) {
									ImGui::LabelText("Value", "0x%x", curModulator.modSrcOper);
									if (curModulator.modSrcOperDetails.cc) {
										ImGui::LabelText("CC", "%d", curModulator.modSrcOperDetails.index);
									} else {
										ImGui::LabelText("GC", "%d", curModulator.modSrcOperDetails.index);
									}
									ImGui::LabelText("Direction", "%s", curModulator.modSrcOperDetails.d ? "Negative": "Positive");
									ImGui::LabelText("Polarity", "%s", curModulator.modSrcOperDetails.p ? "Bipolar": "Unipolar");
									ImGui::LabelText("Type", "%s", GetCurveTypeNames(curModulator.modSrcOperDetails.type));
									ImGui::TreePop();
								}
								if (curModulator.modAmtSrcOper) {
									if (ImGui::TreeNode("Source2")) {
										ImGui::LabelText("Value", "0x%x", curModulator.modAmtSrcOper);
										if (curModulator.modAmtSrcOperDetails.cc) {
											ImGui::LabelText("CC", "%d", curModulator.modAmtSrcOperDetails.index);
										} else {
											ImGui::LabelText("GC", "%d", curModulator.modAmtSrcOperDetails.index);
										}
										ImGui::LabelText("Direction", "%s", curModulator.modAmtSrcOperDetails.d ? "Negative": "Positive");
										ImGui::LabelText("Polarity", "%s", curModulator.modAmtSrcOperDetails.p ? "Bipolar": "Unipolar");
										ImGui::LabelText("Type", "%s", GetCurveTypeNames(curModulator.modAmtSrcOperDetails.type));
										ImGui::TreePop();
									}
								}

								ImGui::LabelText("Dest", "%d", curModulator.modDestOper);

								ImGui::LabelText("Amount", "%d", curModulator.modAmount);

								ImGui::LabelText("Trans", "%s", curModulator.modTransOper == 0 ? "Linear" : "Absolute");

								ImGui::TreePop();
							}
						}
					}
				}
				ImGui::EndChild();

			} else {
				if (m_selectedId < 0 || m_selectedId >= g_TinySoundFont->sampleNum) break;

				tsf_sample & curSample = g_TinySoundFont->samples[m_selectedId];

				auto TableRow = [&](std::string label, std::string value) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(label.c_str());

					ImGui::TableNextColumn();
					ImGui::TextUnformatted(value.c_str());
				};

				if (ImGui::BeginTable("##properties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerH))
            	{
					TableRow("Name", curSample.sampleName);
					TableRow("Start", fmt::format("{}", curSample.start));
					TableRow("End", fmt::format("{}", curSample.end));

					TableRow("Start Loop", fmt::format("{}", curSample.startLoop));
					TableRow("End Loop", fmt::format("{}", curSample.endLoop));
					TableRow("Original Pitch", fmt::format("{}", curSample.originalPitch));
					TableRow("Pitch Correction", fmt::format("{}", curSample.pitchCorrection));
					TableRow("Sample Rate", fmt::format("{}", curSample.sampleRate));
					TableRow("Sample Type", fmt::format("{}", curSample.sampleType));

					ImGui::EndTable();
				}


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
						m_selectedId = sampleLinkIdx;
						m_selectedType = 0;
					}
				}
			}
			noselected = false;
		} while(noselected);
		if (noselected) {
			ImGui::Text("Please select something.");
		}
	}
	ImGui::EndChild();
}

DawSoundFont::DawSoundFont()
{
	auto cb = [this](hscpp::SwapInfo& info) {
		info.Save("selectedId", m_selectedId);
		info.Save("selectedType", m_selectedType);
		info.Save("regionSelected", m_regionSelected);
		info.Save("heightSamples", m_heightSamples);
		info.SaveMove("pPianoButton", m_pPianoButton);
	};

	Hscpp_SetSwapHandler(cb);

	if (Hscpp_IsSwapping())
	{
		return;
	}

	Creating();
}

DawSoundFont::~DawSoundFont()
{
	if (Hscpp_IsSwapping())
	{
			return;
	}

	Destroying();
}

void DawSoundFont::Update()
{
	vinfony::Globals *globals = vinfony::Globals::Resolve();

	if (globals->showSoundFont)
	{
		ImGui::SetCurrentContext( Globals::Resolve()->pImGuiContext );

		ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
		if (ImGui::Begin("SoundFont", &globals->showSoundFont)) {
			Draw( Globals::Resolve()->pTinySoundFont );
		}
		ImGui::End();
	}
}

void DawSoundFont::Creating()
{
	fmt::println( "DawSoundFont::Creating" );

	m_selectedId = -1;
	m_selectedType = 0;
	m_regionSelected = -1;
	m_heightSamples = 200.0f;

	m_pPianoButton = Globals::Resolve()->pMemoryManager->Allocate<vinfony::PianoButton>();
}

void DawSoundFont::Destroying()
{
	fmt::println( "DawSoundFont::Destroying" );
}

}