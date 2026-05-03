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

    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Mie Anisotropy##Sky", &m_Data.SkyParams.MieAnisotropy, 0.f, 1.f);
        ImGui::InputScalar("Atmos interations##Sky", ImGuiDataType_U32, &m_Data.SkyParams.NumSteps);
        ImGui::InputScalar("Light interations##Sky", ImGuiDataType_U32, &m_Data.SkyParams.NumLightSteps);
    }

    if (ImGui::CollapsingHeader("Clouds", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float2 velDir = m_Data.CloudsParams.WindVelocity;
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
            m_Data.CloudsParams.WindVelocity.x = glm::sin(velYaw);
            m_Data.CloudsParams.WindVelocity.y = glm::cos(velYaw);
            m_Data.CloudsParams.WindVelocity *= velMag;
        }

        ImGui::SliderFloat("Scale##Clouds", &m_Data.CloudsParams.CloudsScale, 0.f, 1.f, "%.6f", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Coverage##Clouds", &m_Data.CloudsParams.CloudsCoverage, 0.f, 1.f);
        ImGui::SliderFloat("Absorption##Clouds", &m_Data.CloudsParams.AbsorptionCoeff, 0.f, 1.f);
        ImGui::InputFloat("Clouds Bottom##Clouds", &m_Data.CloudsParams.CloudsLayerMin);
        ImGui::InputFloat("Clouds Top##Clouds", &m_Data.CloudsParams.CloudsLayerMax);
        ImGui::InputFloat("Fade Distance##Clouds", &m_Data.CloudsParams.CloudsFadeDistance);
        ImGui::InputFloat("Earth Radius##Clouds", &m_Data.CloudsParams.EarthRadius);
        ImGui::InputScalar("Density interations##Clouds", ImGuiDataType_U32, &m_Data.CloudsParams.CloudRaymarchIterations);
        ImGui::InputScalar("Light interations##Clouds", ImGuiDataType_U32, &m_Data.CloudsParams.LightRaymarchIterations);
    }

    ImGui::End();
}