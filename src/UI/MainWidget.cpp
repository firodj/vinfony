#include "MainWidget.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "Globals.hpp"

#include <fmt/core.h>
#include <regex>

namespace vinfony {

MainWidget::MainWidget()
{
    auto cb = [this](hscpp::SwapInfo& info) {
        // As an alternative to switching on info.Phase(), then Serializing and Unserializing, you
        // can use the "Save" function, which does the same thing internally.
        //info.Save("Widgets", m_Widgets);
        //info.Save("Title", m_Title);
        //info.Save("InputBuffer", m_InputBuffer);
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