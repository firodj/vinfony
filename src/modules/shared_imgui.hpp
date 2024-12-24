#pragma once


namespace vinfony {
struct ImGuiRemote {
	void (*SetNextWindowSize)(const ImVec2& size, ImGuiCond cond);
	bool (*Begin)(const char* name, bool* p_open, ImGuiWindowFlags flags);
	void (*End)();
	bool (*BeginChild)(const char* str_id, const ImVec2& size_arg, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags);
	void (*EndChild)();
	void (*Image)(ImTextureID user_texture_id, const ImVec2& image_size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col);
};
};
