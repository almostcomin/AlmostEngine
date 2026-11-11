#pragma once

// "Stages CPU time graph" window: CPU time of every render stage over time,
// opened from the Statistics window buttons (not listed in the View menu).
// UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Framework/UI/TimePlot.h"

namespace alm::fw
{

class StagesCPUGraphWindow : public UIWindow
{
public:

	explicit StagesCPUGraphWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Stages CPU time graph"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::fw::TimePlot m_Plot;
	bool m_PlotReady = false;
};

} // namespace alm::fw
