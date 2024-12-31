#include "MainWidget.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "../Globals.hpp"

#include <fmt/core.h>
#include <regex>

hscpp_require_include_dir("${srcPath}/../ext/imgui-docking")
hscpp_require_include_dir("${srcPath}/../ext/fmt/include")

hscpp_if (os == "Windows")
    hscpp_require_library("${libPath}/imgui.lib")
hscpp_elif (os == "Posix")
    hscpp_require_library("${libPath}/libimgui.a")
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

void MainWidget::Update() {
    // When a module is recompiled, ImGui's static context will be empty. Setting it every frame
    // ensures that the context remains set.
    ImGui::SetCurrentContext( Globals::Resolve()->pImGuiContext );

    DockSpaceUI();
}

}