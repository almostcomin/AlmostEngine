#pragma once

// Internal widgets shared by the split FrameworkUI .cpp files. This header is an
// implementation detail of the FrameworkUI class and is not part of the public API:
// it must only be included from FrameworkUI*.cpp sources, after FrameworkUI.h.

#include <imgui/imgui.h>
#include <cstdio>
#include <cstdarg>

namespace alm::fw
{
	// Appends right-aligned formatted text in the current ImGui column/window.
	inline void TextRightAligned(const char* fmt, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		float textWidth = ImGui::CalcTextSize(buffer).x;
		float colWidth = ImGui::GetColumnWidth();
		float cursorX = ImGui::GetCursorPosX();

		ImGui::SetCursorPosX(cursorX + colWidth - textWidth);
		ImGui::TextUnformatted(buffer);
	}
}
