#include "Framework/FrameworkPCH.h"
#include "OutdoorsUI.h"

void OutdoorsUI::BuildUI()
{
	alm::fw::FrameworkUI::BuildUI();

    if (!m_Data.ShowUI)
        return;

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
        float2 velDir = m_Data.SkyParams.WindVelocity;
        float velMag = glm::length(velDir);
        float velYaw = 0.f;
        if (velMag > 0.0001f)
        {
            velDir /= velMag;
            velYaw = atan2(velDir.x, velDir.y);
        }
        bool windUpdated = false;
        windUpdated |= ImGui::SliderAngle("Wind Velocity Yaw##Clouds", &velYaw, -180.f, 180.f);
        windUpdated |= ImGui::InputFloat("Wind Magnitude", &velMag);
        if (windUpdated)
        {
            m_Data.SkyParams.WindVelocity.x = glm::sin(velYaw);
            m_Data.SkyParams.WindVelocity.y = glm::cos(velYaw);
            m_Data.SkyParams.WindVelocity *= velMag;
        }

        ImGui::SliderFloat("Scale##Clouds", &m_Data.SkyParams.CloudsScale, 0.f, 1.f, "%.6f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Coverage##Clouds", &m_Data.SkyParams.CloudsCoverage, 0.f, 1.f);
        ImGui::SliderFloat("Absorption##Clouds", &m_Data.SkyParams.AbsorptionCoeff, 0.f, 1.f);
        ImGui::InputFloat("Clouds Bottom##Clouds", &m_Data.SkyParams.CloudsLayerMin);
        ImGui::InputFloat("Clouds Top##Clouds", &m_Data.SkyParams.CloudsLayerMax);
        ImGui::InputFloat("Fade Distance##Clouds", &m_Data.SkyParams.CloudsFadeDistance);
        ImGui::InputFloat("Earth Radius##Clouds", &m_Data.SkyParams.EarthRadius);
        ImGui::InputScalar("Density interations##Clouds", ImGuiDataType_U32, &m_Data.SkyParams.CloudRaymarchIterations);
        ImGui::InputScalar("Light interations##Clouds", ImGuiDataType_U32, &m_Data.SkyParams.LightRaymarchIterations);
    }

    ImGui::End();
}