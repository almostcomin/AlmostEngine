#include "Framework/FrameworkPCH.h"
#include "OutdoorsUI.h"

void OutdoorsUI::BuildUI()
{
	alm::fw::FrameworkUI::BuildUI();

    if (!ImGui::Begin("Params", &m_Data.ShowUI, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float minAzimuth = -180.0;
        const float maxAzimuth = 180.0;
        const float minElevation = -90.0;
        const float maxElevation = 90.0;

        m_Data.SunParamsUpdated |= ImGui::SliderScalar(
            "Azimuth", ImGuiDataType_Float, &m_Data.SunParams.AzimuthDeg, &minAzimuth, &maxAzimuth, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        m_Data.SunParamsUpdated |= ImGui::SliderScalar(
            "Elevation", ImGuiDataType_Float, &m_Data.SunParams.ElevationDeg, &minElevation, &maxElevation, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        m_Data.SunParamsUpdated |= ImGui::ColorEdit3(
            "Color", &m_Data.SunParams.Color.x, ImGuiColorEditFlags_Float);
        m_Data.SunParamsUpdated |= ImGui::SliderFloat(
            "Irradiance", &m_Data.SunParams.Irradiance, 0.f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
        m_Data.SunParamsUpdated |= ImGui::SliderFloat(
            "Angular Size", &m_Data.SunParams.AngularSizeDeg, 0.0f, 20.f);
    }

    if (ImGui::CollapsingHeader("Clouds", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputFloat("Scale##Clouds", &m_Data.SkyParams.CloudsScale);
        ImGui::InputFloat("Coverage##Clouds", &m_Data.SkyParams.CloudsCoverage);
        ImGui::InputFloat("Absorption##Clouds", &m_Data.SkyParams.AbsorptionCoeff);
        ImGui::InputFloat("MinHeight##Clouds", &m_Data.SkyParams.CloudsLayerMin);
        ImGui::InputFloat("MaxHeight##Clouds", &m_Data.SkyParams.CloudsLayerMax);
        ImGui::InputFloat("FadeDistance", &m_Data.SkyParams.CloudsFadeDistance);
        ImGui::InputScalar("Density interations", ImGuiDataType_U32, &m_Data.SkyParams.CloudRaymarchIterations);
        ImGui::InputScalar("Light interations", ImGuiDataType_U32, &m_Data.SkyParams.LightRaymarchIterations);
    }

    ImGui::End();
}