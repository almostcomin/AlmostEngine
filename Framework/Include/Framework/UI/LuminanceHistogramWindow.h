#pragma once

// "Luminance histogram" window, opened from the Exposure section of the Settings
// window (not listed in the View menu). UIWindow subclass owned by FrameworkUI
// (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Gfx/RenderGraphTypes.h"

namespace alm::fw
{

class LuminanceHistogramWindow : public UIWindow
{
public:

	explicit LuminanceHistogramWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Luminance histogram"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::gfx::RGBufferViewTicket m_BufferTicket;
	int m_Mode = 0;   // 0: log2 scale, 1: linear scale
};

} // namespace alm::fw
