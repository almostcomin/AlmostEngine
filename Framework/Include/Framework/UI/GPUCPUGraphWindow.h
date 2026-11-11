#pragma once

// "CPU/GPU time graph" window: frame CPU/GPU times over time, opened from the
// Statistics window buttons (not listed in the View menu). UIWindow subclass
// owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"
#include "Framework/UI/TimePlot.h"

namespace alm::fw
{

class GPUCPUGraphWindow : public UIWindow
{
public:

	explicit GPUCPUGraphWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "CPU/GPU time graph"; }
	bool ShowInViewMenu() const override { return false; }
	void Build() override;

private:

	alm::fw::TimePlot m_Plot;
	bool m_PlotReady = false;
};

} // namespace alm::fw
