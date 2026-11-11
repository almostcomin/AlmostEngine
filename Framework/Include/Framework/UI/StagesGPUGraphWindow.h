#pragma once

// "Stages GPU time graph" window: GPU time of every render stage over time,
// opened from the Statistics window buttons (not listed in the View menu).
// UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Framework/UI/TimePlot.h"

namespace alm::fw
{

class StagesGPUGraphWindow : public UIWindow
{
public:

	explicit StagesGPUGraphWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Stages GPU time graph"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::fw::TimePlot m_Plot;
	bool m_PlotReady = false;
};

} // namespace alm::fw
