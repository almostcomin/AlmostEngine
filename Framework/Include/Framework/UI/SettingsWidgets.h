#pragma once

#include "Core/Math/types.h"

namespace alm::fw
{

class FrameworkUI;

// UI sections shared by more than one window: the Settings window and the
// Atmosphere/Lighting window both show the Shadowmap, Directional light and
// Ambient sections. Both edit the same FrameworkData fields, so each section is
// a single widget object owned by FrameworkUI (never duplicated state).
class ShadowmapSettingsWidget
{
public:

	explicit ShadowmapSettingsWidget(FrameworkUI& ui) : m_UI(ui) {}

	void Build(float availWidth);

private:

	FrameworkUI& m_UI;

	// Shadowmap size being typed by the user, applied to FrameworkData.ShadowmapSize
	// on "Apply" (FrameworkApp polls it and resizes the stage).
	int2 m_SizeCached = { -1, -1 };
};

class DirectionalLightWidget
{
public:

	explicit DirectionalLightWidget(FrameworkUI& ui) : m_UI(ui) {}

	void Build(float availWidth);

private:

	FrameworkUI& m_UI;
};

class AmbientLightWidget
{
public:

	explicit AmbientLightWidget(FrameworkUI& ui) : m_UI(ui) {}

	void Build(float availWidth);

private:

	FrameworkUI& m_UI;
};

} // namespace alm::fw
