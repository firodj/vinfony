#include "DawPianoRoll.hpp"

#include <fmt/core.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include "PianoButton.hpp"
#include "UI.hpp"
#include "../Globals.hpp"

hscpp_require_include_dir("${projPath}/src")
hscpp_require_include_dir("${projPath}/kosongg/cpp")
hscpp_require_include_dir("${projPath}/ext/jdksmidi/include")
hscpp_require_include_dir("${projPath}/ext/imgui-docking")
hscpp_require_include_dir("${projPath}/ext/fmt/include")
hscpp_require_include_dir("${projPath}/ext/hscpp/extensions/mem/include")

hscpp_require_source("PianoButton.cpp")
hscpp_require_source("Splitter.cpp")

hscpp_if (os == "Windows")
	hscpp_require_library("${buildPath}/kosongg/cmake/imgui/Debug/imgui.lib")
	hscpp_require_library("${buildPath}/ext/jdksmidi/Debug/jdksmidi.lib")
	hscpp_require_library("${buildPath}/ext/fmt/Debug/fmtd.lib")
	hscpp_require_library("${buildPath}/ext/hscpp/extensions/mem/Debug/hscpp-mem.lib")
hscpp_elif (os == "Posix")
	hscpp_require_library("${projPath}/bin/libimgui.dylib")
	hscpp_require_library("${projPath}/bin/libjdksmidi.dylib")
	hscpp_require_library("${projPath}/bin/libfmtd.dylib")
	hscpp_require_library("${buildPath}/ext/hscpp/extensions/mem/Debug/libhscpp-mem.a")
hscpp_else()
	// Diagnostic messages can be printed to the build output with hscpp_message.
	hscpp_message("Unknown OS ${os}.")
hscpp_end()

namespace vinfony
{

DawPianoRoll::DawPianoRoll() {
	auto cb = [this](hscpp::SwapInfo& info) {
		info.SaveMove("pPianoButton", m_pPianoButton);
	};

	Hscpp_SetSwapHandler(cb);

	if (Hscpp_IsSwapping())
	{
		return;
	}

	Creating();
}

DawPianoRoll::~DawPianoRoll() {
	if (Hscpp_IsSwapping())
	{
		return;
	}

	Destroying();
}

void DawPianoRoll::Update() {
	vinfony::Globals *globals = vinfony::Globals::Resolve();

	if (globals->showPianoRoll)
	{
		ImGui::SetCurrentContext( Globals::Resolve()->pImGuiContext );

		ImGui::SetNextWindowSize({640, 480}, ImGuiCond_Once);
		if (ImGui::Begin("PianoRoll", &globals->showPianoRoll)) {
			ImGui::Text("Piano Roll");

			m_pPianoButton->DrawV();
		}
		ImGui::End();

	}
}

void DawPianoRoll::Creating() {
	fmt::println( "DawPianoRoll::Creating" );

	m_pPianoButton = Globals::Resolve()->pMemoryManager->Allocate<vinfony::PianoButton>();
}

void DawPianoRoll::Destroying() {
	fmt::println( "DawPianoRoll::Destroying" );
}

}