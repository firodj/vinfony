#include "MainWidget.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "../Globals.hpp"

#include <kosongg/IconsFontAwesome6.h>
#include "kosongg/Component.h"

#include "Interfaces/DawSeq.hpp"

#include <fmt/core.h>
#include <regex>

hscpp_require_include_dir("${projPath}/src")
hscpp_require_include_dir("${projPath}/kosongg/cpp")
hscpp_require_include_dir("${projPath}/ext/imgui-docking")
hscpp_require_include_dir("${projPath}/ext/fmt/include")

hscpp_if (os == "Windows")
    hscpp_require_library("${buildPath}/Debug/imgui.lib")
    hscpp_require_library("${buildPath}/Debug/ext/fmt/Debug/fmtd.lib")
hscpp_elif (os == "Posix")
    //hscpp_require_library("${buildPath}/Debug/libimgui.a")
    hscpp_require_library("${projPath}/libimgui.dylib")
    hscpp_require_library("${buildPath}/ext/fmt/Debug/libfmtd.a")
hscpp_else()
    // Diagnostic messages can be printed to the build output with hscpp_message.
    hscpp_message("Unknown OS ${os}.")
hscpp_end()

namespace vinfony {

MainWidget::MainWidget()
{
    auto cb = [this](hscpp::SwapInfo& info) {
        switch (info.Phase())
        {
        case hscpp::SwapPhase::BeforeSwap:
            // This object is about to be deleted, so serialize out its state. Any type can be
            // serialized so long as it has a default constructor and is copyable.
            //info.Serialize("Name", m_Name);
            //info.Serialize("Index", m_Index);
            break;
        case hscpp::SwapPhase::AfterSwap:
            // This object has just been created, replacing an older version.
            //info.Unserialize("Name", m_Name);
            //info.Unserialize("Index", m_Index);

            // This new object is in a new dynamic library, but it still has access to global data
            // in GlobalUserData.
            vinfony::Globals* globals = vinfony::Globals::Resolve();

            // After recompiling Printer.cpp, old instances of the Printer class will have been deleted,
            // so the entries in the global printers array point to invalid data. Replace these instances
            // with the newly constructed Printers.
            globals->pMainWidget = this;
            break;
        }
    };

    Hscpp_SetSwapHandler(cb);
}

MainWidget::~MainWidget()
{
    if (Hscpp_IsSwapping())
    {
        return;
    }

}

// https://github.com/tpecholt/imrad/blob/main/src/imrad.cpp
void MainWidget::DockSpaceUI()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos + ImVec2(0, Globals::Resolve()->toolbarSize));
	ImGui::SetNextWindowSize(viewport->Size - ImVec2(0, Globals::Resolve()->toolbarSize));
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("Master DockSpace", NULL, window_flags);
	ImGuiID dockspace_id = ImGui::GetID("MyDockspace");

	// Save off menu bar height for later.
	Globals::Resolve()->menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight;

	ImGui::DockSpace(dockspace_id);

	//ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
	//ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

	//ImGui::DockBuilderFinish(dockspace_id);

	ImGui::End();

	ImGui::PopStyleVar(3);
}

void MainWidget::ToolbarUI()
{
	vinfony::Globals *globals = vinfony::Globals::Resolve();

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + globals->menuBarHeight));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, globals->toolbarSize));
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = 0
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoSavedSettings
		;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::Begin("TOOLBAR", NULL, window_flags);
	ImGui::PopStyleVar();

	const bool disabled = !globals->sequencer->IsFileLoaded();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
	ImGui::PushItemFlag(ImGuiItemFlags_Disabled, disabled);

	if (disabled) {
		ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0xFF, 0xFF, 0xFF, 0x80));
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x00, 0x00, 0x00, 0x40));
	} else {
		ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0x00, 0x7A, 0xFF, 0xFF));
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
	}

	if (ImGui::ColoredButtonV1(ICON_FA_BACKWARD_FAST " Rewind", ImVec2{})) {
		globals->sequencer->SetMIDITimeBeat(0);
	}
	ImGui::SameLine();
	if (ImGui::ColoredButtonV1(ICON_FA_PLAY " Play")) {
		globals->sequencer->AsyncPlayMIDI();
	}
	ImGui::SameLine();
	if (ImGui::ColoredButtonV1(ICON_FA_STOP " Stop")) {
		globals->sequencer->StopMIDI();
	}
	ImGui::SameLine();

	std::string label = fmt::format(ICON_FA_GAUGE " {:.2f}", globals->sequencer->GetTempoBPM());
	ImGui::ColoredButtonV1(label.c_str());
	ImGui::SameLine();

	int curM, curB, curT;
	globals->sequencer->GetCurrentMBT(curM, curB, curT);
	label = fmt::format(ICON_FA_CLOCK " {:3d}:{:01d}:{:03d}", curM, curB, curT);

    static ImVec2 labelMBTsize{};
	if (labelMBTsize.x == 0) {
		labelMBTsize = ImGui::CalcButtonSizeWithText(ICON_FA_CLOCK " 000:0:000", NULL, true);
	}
	ImGui::ColoredButtonV1(label.c_str(), labelMBTsize);

	ImGui::PopItemFlag();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(2);

	ImGui::End();
}

void MainWidget::Update() {
    // When a module is recompiled, ImGui's static context will be empty. Setting it every frame
    // ensures that the context remains set.
    ImGui::SetCurrentContext( Globals::Resolve()->pImGuiContext );

    DockSpaceUI();
    ToolbarUI();
}

}