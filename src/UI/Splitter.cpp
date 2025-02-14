#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include "UI.hpp"

hscpp_require_include_dir("${projPath}/ext/imgui-docking")

hscpp_if (os == "Windows")
    hscpp_require_library("${buildPath}/kosongg/cmake/imgui/Debug/imgui.lib")
hscpp_elif (os == "Posix")
    //hscpp_require_library("${buildPath}/Debug/libimgui.a")
    hscpp_require_library("${projPath}/bin/libimgui.dylib")
hscpp_else()
    // Diagnostic messages can be printed to the build output with hscpp_message.
    hscpp_message("Unknown OS ${os}.")
hscpp_end()

namespace vinfony
{
	const int SplitterThickness = 8;

	bool VSplitter(ImVec2 pos, float avail_h) // , SplitterOnDraggingFunc func)
	{
		ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
		ImU32 color_hover  = ImGui::GetColorU32(ImGuiCol_SeparatorHovered, 1.0);
		ImU32 color_active  = ImGui::GetColorU32(ImGuiCol_SeparatorActive, 1.0);

		ImGui::SetCursorPos(pos);
		ImGui::InvisibleButton("vsplitter", ImVec2{SplitterThickness, (float)ImMax(avail_h, 1.0f)});
		auto rcmin = ImGui::GetItemRectMin();
		auto rcmax = ImGui::GetItemRectMax();
		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			color_border = color_hover;
		}
		bool pressed = false;
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			pressed = true; //if (func) func();
			color_border = color_active;
		}

		rcmin.x += SplitterThickness/2;
		rcmax.x = rcmin.x;
		ImGui::GetWindowDrawList()->AddLine(rcmin, rcmax, color_border);
		return pressed;
	}

	bool HSplitter(ImVec2 pos, float avail_w) //, SplitterOnDraggingFunc func)
	{
		ImU32 color_border = ImGui::GetColorU32(ImGuiCol_Separator, 1.0);
		ImU32 color_hover  = ImGui::GetColorU32(ImGuiCol_SeparatorHovered, 1.0);
		ImU32 color_active = ImGui::GetColorU32(ImGuiCol_SeparatorActive, 1.0);

		ImGui::SetCursorPos(pos);
		ImGui::InvisibleButton("hsplitter", ImVec2{(float)ImMax(avail_w, 1.0f), SplitterThickness});
		auto rcmin = ImGui::GetItemRectMin();
		auto rcmax = ImGui::GetItemRectMax();
		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			color_border = color_hover;
		}
		bool pressed = false;
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			pressed = true; //if (func) func();
			color_border = color_active;
		}
		rcmin.y += SplitterThickness/2;
		rcmax.y = rcmin.y;
		ImGui::GetWindowDrawList()->AddLine(rcmin, rcmax, color_border);
		return pressed;
	}
}
