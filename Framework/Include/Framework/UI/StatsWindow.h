#pragma once

// "Statistics" window (View > Statistics): culling stats and the rendered
// workload per pass. UIWindow subclass owned by FrameworkUI (see UIWindow.h).

#include "Framework/UI/UIWindow.h"

namespace alm::fw
{

class StatsWindow : public UIWindow
{
public:

	explicit StatsWindow(FrameworkUI& ui) : UIWindow(ui) {}

	const char* GetName() const override { return "Statistics"; }
	void Build() override;

private:

	// Whether this window holds a GPUCullingRenderStage stats request
	// (RequestStats/UnrequestStats lifecycle follows the window being open)
	bool m_CullingStatsRequested = false;
};

} // namespace alm::fw
