#pragma once

#include <imgui/imgui.h>

namespace alm::fw
{

class FrameworkUI;

// Base class of the FrameworkUI windows (Settings, World Outliner, Statistics, ...).
// Each window class owns its local UI state (filters, selections, view caches) as
// member variables, instead of everything living in the FrameworkUI class. Windows
// are owned and built every frame by FrameworkUI; those shown in the "View" menu
// are auto-registered there, along with their open hotkey.
class UIWindow
{
public:

	virtual ~UIWindow() = default;

	// Window title: ImGui::Begin() name and "View" menu entry label.
	virtual const char* GetName() const = 0;

	// Hotkey that opens (not toggles) the window, or ImGuiKey_None.
	virtual ImGuiKeyChord GetOpenHotkey() const { return ImGuiKey_None; }

	// Whether the window is listed in the "View" menu. Secondary windows (the
	// stats graphs, the luminance histogram) are opened from other windows instead.
	virtual bool ShowInViewMenu() const { return true; }

	// Draws the window. Implementations keep the usual early-return pattern:
	// if (!m_Open) return; ... ImGui::Begin(GetName(), &m_Open) ...
	virtual void Build() = 0;

	bool IsOpen() const { return m_Open; }

	void SetOpen(bool open)
	{
		// NOTE: m_Open is assigned *before* calling OnOpen(): an OnOpen() override
		// may call SetOpen(true) again (e.g. WorldOutlinerWindow::OpenWithSelectedNodeFocus),
		// and it must observe the new state to avoid infinite recursion.
		const bool wasOpen = m_Open;
		m_Open = open;
		if (open && !wasOpen)
			OnOpen();
	}

	void Toggle() { SetOpen(!m_Open); }

protected:

	explicit UIWindow(FrameworkUI& ui) : m_UI(ui) {}

	// Called when the window transitions from closed to open (the World Outliner,
	// for example, expands and scrolls to the selected node).
	virtual void OnOpen() {}

	FrameworkUI& m_UI;

	bool m_Open = false;
};

} // namespace alm::fw
