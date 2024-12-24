#include "shared.hpp"
#include <vector>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "PianoButton.hpp"
#include <iostream>
#include "shared_imgui.hpp"

DYNALO_EXPORT int32_t DYNALO_CALL add_integers(const int32_t a, const int32_t b)
{
	return a + b;
}

DYNALO_EXPORT void DYNALO_CALL print_message(const char* str)
{
	std::cout << "Hello [" << str << "]" << std::endl;
}

DYNALO_EXPORT int32_t DYNALO_CALL DrawPianoRoll(vinfony::ImGuiRemote *imgui, vinfony::DrawPianoRollParam *param)
{
	imgui->SetNextWindowSize({640, 480}, ImGuiCond_Once);
	if (imgui->Begin("Piano", nullptr, 0)) {
		imgui->BeginChild("piano", ImVec2{0.0, 200.0}, ImGuiChildFlags_ResizeY | ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysHorizontalScrollbar);
		//vinfony::PianoButtonV("piano");
		imgui->EndChild();

		if (param->texPiano)
			imgui->Image((ImTextureID)param->texPiano, ImVec2(param->pianoWidth, param->pianoHeight),
				ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(0, 0, 0, 0));
	}
	imgui->End();
	return 0;
}