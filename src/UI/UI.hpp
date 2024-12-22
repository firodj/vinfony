#pragma once

#include <functional>
#include <imgui.h>

namespace vinfony {
	extern const int SplitterThickness;
	using SplitterOnDraggingFunc = std::function<void()>;
	void VSplitter(ImVec2 pos, float avail_h, SplitterOnDraggingFunc func);
	void HSplitter(ImVec2 pos, float avail_w, SplitterOnDraggingFunc func);
};
