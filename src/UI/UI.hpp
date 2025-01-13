#pragma once

#include <functional>
#include <imgui.h>
#include "hscpp/module/PreprocessorMacros.h"

namespace vinfony {
	extern const int SplitterThickness;
	using SplitterOnDraggingFunc = std::function<void()>;
	void VSplitter(ImVec2 pos, float avail_h, SplitterOnDraggingFunc func);
	void HSplitter(ImVec2 pos, float avail_w, SplitterOnDraggingFunc func);
};
