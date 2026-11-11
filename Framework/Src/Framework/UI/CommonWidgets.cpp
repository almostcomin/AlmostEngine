// Shared widgets (Shadowmap, Directional light, Ambient) used by more than one
// window: the Settings window and the Atmosphere/Lighting window both show them
// (see CommonWidgets.h).

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/CommonWidgets.h"
#include "Framework/UI/FrameworkUI.h"
#include "Gfx/Scene.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"

void alm::fw::ShadowmapSettingsWidget::Build(float availWidth)
{
	const ImGuiStyle& style = ImGui::GetStyle();

	if (ImGui::CollapsingHeader("Shadowmap", ImGuiTreeNodeFlags_None))
	{
		auto shadowmapRS = m_UI.GetUIRenderView()->GetRenderGraph()->GetRenderStage<alm::gfx::ShadowmapRenderStage>();
		if (!shadowmapRS)
			return;

		auto lightingRS = m_UI.GetUIRenderView()->GetRenderGraph()->GetRenderStage<alm::gfx::DeferredLightingRenderStage>();

		ImGui::SetNextItemWidth(availWidth / 2);
		ImGui::Checkbox("Enabled##Shadowmap", &m_UI.FrameworkData.ShadowmapEnabled);

		ImGui::SameLine(availWidth / 2);
		ImGui::SetNextItemWidth(availWidth / 2);
		bool showShadowmap = lightingRS->GetShowShadowmap();
		if (ImGui::Checkbox("Show##Shadowmap", &showShadowmap))
			lightingRS->ShowShadowmap(showShadowmap);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::SetNextItemWidth(availWidth / 4);
		int depthBias = shadowmapRS->GetDepthBias();
		if (ImGui::InputInt("Depth Bias##Shadowmap", &depthBias))
			shadowmapRS->SetDepthBias(depthBias);

		ImGui::SameLine(availWidth / 2);
		ImGui::SetNextItemWidth(availWidth / 4);
		float slopeBias = shadowmapRS->GetSlopeScaledDepthBias();
		if (ImGui::InputFloat("Slope scaled Depth Bias##Shadowmap", &slopeBias))
			shadowmapRS->SetSlopeScaledDepthBias(slopeBias);

		ImGui::Spacing();

		float itemWidth = availWidth / 5;
		ImGui::SetCursorPosX(style.ItemSpacing.x);

		ImGui::Text("Size");
		ImGui::SameLine();
		ImGui::SetCursorPosX(itemWidth + style.ItemSpacing.x);

		if (m_SizeCached.x < 0)
			m_SizeCached = shadowmapRS->GetSize();

		ImGui::SetNextItemWidth(itemWidth);
		ImGui::InputInt("##sizex", (int*)&m_SizeCached.x, ImGuiInputTextFlags_None);
		ImGui::SameLine();

		ImGui::SetNextItemWidth(itemWidth);
		ImGui::InputInt("##sizey", (int*)&m_SizeCached.y, ImGuiInputTextFlags_None);
		ImGui::SameLine();

		if (ImGui::Button("Reset", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
			m_SizeCached = shadowmapRS->GetSize();

		ImGui::SameLine();
		if (ImGui::Button("Apply", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
		{
			m_UI.FrameworkData.ShadowmapSize = m_SizeCached;
		}
	}
}

void alm::fw::DirectionalLightWidget::Build(float availWidth)
{
	if (ImGui::CollapsingHeader("Directional light"))
	{
		if (m_UI.GetScene()->GetLightingMode() != gfx::LightingMode::Atmosphere)
		{
			ImGui::TextUnformatted("Unavailable - Change Lighting Mode in Atmosphere panel");
			return;
		}

		const float minAzimuth = -180.0;
		const float maxAzimuth = 180.0;
		const float minElevation = -90.0;
		const float maxElevation = 90.0;

		m_UI.FrameworkData.SunParamsUpdated |= ImGui::Checkbox(
			"Enabled", &m_UI.FrameworkData.SunLightEnabled);
		m_UI.FrameworkData.SunParamsUpdated |= ImGui::SliderScalar(
			"Azimuth", ImGuiDataType_Float, &m_UI.FrameworkData.SunParams.AzimuthDeg, &minAzimuth, &maxAzimuth, "%.1f deg",
			ImGuiSliderFlags_NoRoundToFormat);
		m_UI.FrameworkData.SunParamsUpdated |= ImGui::SliderScalar(
			"Elevation", ImGuiDataType_Float, &m_UI.FrameworkData.SunParams.ElevationDeg, &minElevation, &maxElevation, "%.1f deg",
			ImGuiSliderFlags_NoRoundToFormat);
		m_UI.FrameworkData.SunParamsUpdated |= ImGui::ColorEdit3(
			"Color", &m_UI.FrameworkData.SunParams.Color.x, ImGuiColorEditFlags_Float);
		m_UI.FrameworkData.SunParamsUpdated |= ImGui::SliderFloat(
			"Irradiance", &m_UI.FrameworkData.SunParams.Irradiance, 0.f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
		m_UI.FrameworkData.SunParamsUpdated |= ImGui::SliderFloat(
			"Angular Size", &m_UI.FrameworkData.SunParams.AngularSizeDeg, 0.0f, 20.f);
	}
}

void alm::fw::AmbientLightWidget::Build(float availWidth)
{
	if (ImGui::CollapsingHeader("Ambient"))
	{
		if (m_UI.GetScene()->GetLightingMode() != gfx::LightingMode::Atmosphere)
		{
			ImGui::TextUnformatted("Unavailable - Change Lighting Mode in Atmosphere panel");
			return;
		}

		m_UI.FrameworkData.AmbientParamsUpdated |= ImGui::SliderFloat("Intensity", &m_UI.FrameworkData.AmbientParams.Intensity, 0.f, 1.f);
		m_UI.FrameworkData.AmbientParamsUpdated |= ImGui::ColorEdit3("Sky Color", &m_UI.FrameworkData.AmbientParams.SkyColor.x, ImGuiColorEditFlags_Float);
		m_UI.FrameworkData.AmbientParamsUpdated |= ImGui::ColorEdit3("Ground Color", &m_UI.FrameworkData.AmbientParams.GroundColor.x, ImGuiColorEditFlags_Float);
	}
}
