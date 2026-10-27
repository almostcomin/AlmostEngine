// FrameworkUI "Atmosphere Panel" window (View > Atmosphere Panel).
// This is part of the alm::fw::FrameworkUI class: its member functions are split
// across several FrameworkUI_*.cpp files, grouped by UI window.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/FrameworkUIWidgets.h"
#include "Framework/UI/CloudsPresets.h"
#include "Gfx/Scene.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/RenderStages/SimpleSkyRenderStage.h"
#include "Gfx/RenderStages/CubemapSkyRenderStage.h"
#include "Gfx/RenderStages/AtmosSkyRenderStage.h"
#include "Gfx/RenderStages/CloudsRenderStage.h"
#include "Gfx/LoadedTexture.h"

void alm::fw::FrameworkUI::BuildAtmospherePanel()
{
    if (!m_ShowAtmospherePanel)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Atmosphere", &m_ShowAtmospherePanel, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

    BuildDirectionalLightSettings(availWidth);
    BuildAmbientLightSettings(availWidth);
    BuildSkySettings(availWidth);
    BuildsCloudsSettings();

    ImGui::End();
}

void alm::fw::FrameworkUI::BuildSkySettings(float availWidth)
{
    auto simpleSky = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::SimpleSkyRenderStage>();
    if (simpleSky)
    {
        if (ImGui::CollapsingHeader("Simple Sky"))
        {
            bool isEnabled = simpleSky->IsEnabled();
            ImGui::Checkbox("Enabled##SimpleSky", &isEnabled);
            simpleSky->SetEnabled(isEnabled);

            ImGui::ColorEdit3("Sky color##Sky", &FrameworkData.SimpleSkyParams.skyColor.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Horizon color##Sky", &FrameworkData.SimpleSkyParams.horizonColor.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Ground color##Sky", &FrameworkData.SimpleSkyParams.groundColor.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Brightness scaler", &FrameworkData.SimpleSkyParams.brightness, 0.f, 10.f);

            float horizonSizeRad = glm::radians(FrameworkData.SimpleSkyParams.horizonSize);
            if (ImGui::SliderAngle("Horizon size", &horizonSizeRad, 0.f, 90.f))
            {
                FrameworkData.SimpleSkyParams.horizonSize = glm::degrees(horizonSizeRad);
            }
            float glowSizeRad = glm::radians(FrameworkData.SimpleSkyParams.glowSize);
            if (ImGui::SliderAngle("Glow size", &glowSizeRad, 0.f, 90.f))
            {
                FrameworkData.SimpleSkyParams.glowSize = glm::degrees(glowSizeRad);
            }

            ImGui::SliderFloat("Glow intensity", &FrameworkData.SimpleSkyParams.glowIntensity, 0.f, 1.f);
            ImGui::SliderFloat("Glow sharpness", &FrameworkData.SimpleSkyParams.glowSharpness, 1.f, 10.f);
            ImGui::InputFloat("Max radiance", &FrameworkData.SimpleSkyParams.maxLightRadiance);
        }
    }

    auto cubemapSky = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::CubemapSkyRenderStage>();
    if (cubemapSky && ImGui::CollapsingHeader("Cubemap Sky"))
    {
        bool enabled = cubemapSky->IsEnabled();
        if (ImGui::Checkbox("Enabled##AtmosSky", &enabled))
        {
            cubemapSky->SetEnabled(enabled);
        }

        auto cubemapTexture = cubemapSky->GetCubemapTexture();
        if (BuildTextureShowBrowseClear("Cubemap Texture", cubemapTexture, alm::gfx::TextureLoadFlags::None))
        {
            if (cubemapTexture && cubemapTexture->texture->GetDesc().dimension != alm::rhi::TextureDimension::TextureCube)
            {
                ShowErrorModal(
                    std::string("Texture:\n\"") + cubemapTexture->texture->GetDebugName() + "\"\n\n"
                    "Is not a Cubemap texture");
            }
            else
            {
                cubemapSky->SetCubemapTexture(cubemapTexture);
            }
        }
    }

    auto atmosSky = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::AtmosSkyRenderStage>();
    if (atmosSky)
    {
        if (ImGui::CollapsingHeader("Atmospheric Sky"))
        {
            ImGui::Checkbox("Enabled##AtmosSky", &FrameworkData.AtmosSkyEnabled);

            ImGui::SameLine();
            float gpuMs = m_RenderViewUI->GetRenderGraph()->GetStageGPUTimeMs(alm::gfx::AtmosSkyRenderStage::StaticType());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            TextRightAligned("GPU: %1.2f ms", gpuMs);
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::InputFloat("Atmospheric depth", &FrameworkData.AtmosSkyParams.AtmosHeight);
            ImGui::InputFloat3("Rayleigh Wave Lengths", &FrameworkData.AtmosSkyParams.RayleighWaveLengths.x, "%.1f");
            ImGui::InputFloat("Turbidity factor", &FrameworkData.AtmosSkyParams.Turbidity);
            ImGui::SliderFloat("Mie Anisotropy##Sky", &FrameworkData.AtmosSkyParams.MieAnisotropy, 0.f, 1.f);
            ImGui::InputScalar("Atmos interations##Sky", ImGuiDataType_U32, &FrameworkData.AtmosSkySimParams.NumSteps);
            ImGui::InputScalar("Light interations##Sky", ImGuiDataType_U32, &FrameworkData.AtmosSkySimParams.NumLightSteps);
        }
    }
}

void alm::fw::FrameworkUI::BuildsCloudsSettings()
{
    const auto& scene = GetScene();
    auto cloudsRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::CloudsRenderStage>();
    gfx::AtmosphereConfig* atmos = scene ? scene->GetAtmosphereConfig() : nullptr;

    if(atmos && atmos->CloudsSubsystemInitialized() && cloudsRS && ImGui::CollapsingHeader("Clouds"))
    {
        ImGui::Checkbox("Enabled##Clouds", &FrameworkData.CloudsEnabled);

        ImGui::SameLine();
        float gpuMs = m_RenderViewUI->GetRenderGraph()->GetStageGPUTimeMs(alm::gfx::CloudsRenderStage::StaticType());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        TextRightAligned("GPU: %1.2f ms", gpuMs);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::SeparatorText("Preset");
        ImGui::Spacing();

        if (ImGui::BeginCombo("Preset##Clouds", CloudsPresetToString(m_CloudsPreset)))
        {
            for (int i = 0; i < (int)CloudsPreset::_Size; ++i)
            {
                if (ImGui::Selectable(CloudsPresetToString((CloudsPreset)i), i == (int)m_CloudsPreset))
                {
                    m_CloudsPreset = (CloudsPreset)i;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();

        if (ImGui::Button("Apply##CloudsPreset"))
        {
            ApplyCloudsPreset(m_CloudsPreset, *atmos);
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Velocity");
        ImGui::Spacing();

        float2 velDir = atmos->WindVelocity;
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
            atmos->WindVelocity.x = glm::sin(velYaw);
            atmos->WindVelocity.y = glm::cos(velYaw);
            atmos->WindVelocity *= velMag;
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Shape");
        ImGui::Spacing();

        ImGui::SliderFloat("Stratus Weight##Clouds", &atmos->CloudsShape.StratusWeight, 0.f, 1.f);
        ImGui::SliderFloat("Cumulus Weight##Clouds", &atmos->CloudsShape.CumulusWeight, 0.f, 1.f);
        ImGui::SliderFloat("Cumulonimbus Weight##Clouds", &atmos->CloudsShape.CumulonimbusWeight, 0.f, 1.f);

        ImGui::Spacing();

        ImGui::SliderFloat("Scale##Clouds", &atmos->CloudsShape.CloudsScale, 0.f, 1.f, "%.6f",
            ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        ImGui::Spacing();

        ImGui::SliderFloat("Coverage##Clouds", &atmos->CloudsShape.CloudsCoverage, 0.f, 1.f);
        ImGui::Spacing();

        ImGui::InputFloat("Detail Scale##Clouds", &atmos->CloudsShape.DetailScale);
        ImGui::SliderFloat("Detail Erosion Strength##Clouds", &atmos->CloudsShape.DetailErosionStrength, 0.f, 1.f);

        ImGui::Spacing();
        ImGui::SeparatorText("Animation");
        ImGui::Spacing();

        ImGui::InputFloat("Animation multiplier##Clouds", &atmos->CloudsShape.AnimationMult);
        ImGui::InputFloat("Shear Tilt##Clouds", &atmos->CloudsShape.ShearTiltMeters);
        ImGui::InputFloat("Sway Amplitude##Clouds", &atmos->CloudsShape.SwayAmpMeters);
        ImGui::SliderFloat("Sway Speed##Clouds", &atmos->CloudsShape.SwaySpeed, 0.f, 1.f);
        ImGui::SliderFloat("Swirl Speed##Clouds", &atmos->CloudsShape.SwirlSpeed, 0.f, 1.f);
        ImGui::SliderFloat("Swirl Radius##Clouds", &atmos->CloudsShape.SwirlRadius, 0.f, 1.f);
        ImGui::SliderFloat("Morph Speed##Clouds", &atmos->CloudsShape.MorphSpeed, 0.f, 1.f);

        ImGui::Spacing();
        ImGui::SeparatorText("Lighting");
        ImGui::Spacing();

        float muA_Km = atmos->CloudsShape.AbsorptionCoeff * 1000.f;
        if (ImGui::SliderFloat("Absorption Coeff 1/km (norm)##Clouds", &muA_Km, 0.f, 10.f))
            atmos->CloudsShape.AbsorptionCoeff = muA_Km / 1000.f;

        float muS_Km = atmos->CloudsShape.ScatteringCoeff * 1000.f;
        if(ImGui::SliderFloat("Scattering Coeff 1/km (norm)##Clouds", &muS_Km, 0.f, 10.f))
            atmos->CloudsShape.ScatteringCoeff = muS_Km / 1000.f;

        ImGui::Spacing();
        ImGui::SliderFloat("MultiScatter Contribution##Clouds", &atmos->Clouds.MultiScatterContribution, 0.f, 1.f);
        ImGui::SliderFloat("MultiScatter Occlusion##Clouds", &atmos->Clouds.MultiScatterOcclusion, 0.f, 1.f);
        ImGui::SliderFloat("MultiScatter Eccentricity##Clouds", &atmos->Clouds.MultiScatterEccentricity, 0.f, 1.f);

        ImGui::Spacing();
        ImGui::SliderFloat("Phase G Forward##Clouds", &atmos->Clouds.PhaseGForward, -1.f, 1.f);
        ImGui::SliderFloat("Phase G Backward##Clouds", &atmos->Clouds.PhaseGBackward, -1.f, 1.f);
        ImGui::SliderFloat("Multiscatter Base G##Clouds", &atmos->Clouds.MultiScatterBaseG, 0.f, 1.f);

        ImGui::Spacing();
        ImGui::SliderFloat("Powder Strength##Clouds", &atmos->Clouds.PowderStrength, 0.f, 1.f);
        ImGui::SliderFloat("Powder Edge Width##Clouds", &atmos->Clouds.PowderEdgeWidth, 0.f, 1.f);

        ImGui::Spacing();
        ImGui::ColorEdit3("Ambient Top Color", (float*)&(atmos->Clouds.AmbientTop.x), ImGuiColorEditFlags_Float);
        ImGui::ColorEdit3("Ambient Bottom Color", (float*)&(atmos->Clouds.AmbientBottom.x), ImGuiColorEditFlags_Float);

        ImGui::SliderFloat("Ambient Strength##Clouds", &atmos->Clouds.AmbientStrength, 0.f, 1.f);

        ImGui::Spacing();
        ImGui::SeparatorText("Layer");
        ImGui::Spacing();

        ImGui::InputFloat("Clouds Bottom##Clouds", &atmos->CloudsShape.CloudsLayerMinH);
        ImGui::InputFloat("Clouds Top##Clouds", &atmos->CloudsShape.CloudsLayerMaxH);
        ImGui::Spacing();
        ImGui::InputFloat("Fade Distance##Clouds", &atmos->Clouds.CloudsFadeDistance);

        ImGui::Spacing();
        ImGui::SeparatorText("Performance");
        ImGui::Spacing();

        ImGui::InputScalar("Density interations##Clouds", ImGuiDataType_U32, &FrameworkData.CloudsSimParams.CloudRaymarchIterations);
        ImGui::Checkbox("Volumetric shadows##Clouds", &FrameworkData.CloudsSimParams.VolumetricShadows);
        ImGui::InputScalar("Light interations##Clouds", ImGuiDataType_U32, &FrameworkData.CloudsSimParams.LightRaymarchIterations);
        ImGui::SliderInt("Multi scatter octaves##Clouds", (int*)&FrameworkData.CloudsSimParams.MultiScatterOctaves, 0, 8);

        ImGui::Spacing();

        if (ImGui::RadioButton("Render Target 1/1##Clouds", FrameworkData.CloudsRTDenom == 1))
            FrameworkData.CloudsRTDenom = 1;
        ImGui::SameLine();
        if (ImGui::RadioButton("Render Target 1/2##Clouds", FrameworkData.CloudsRTDenom == 2))
            FrameworkData.CloudsRTDenom = 2;
        ImGui::SameLine();
        if (ImGui::RadioButton("Render Target 1/4##Clouds", FrameworkData.CloudsRTDenom == 4))
            FrameworkData.CloudsRTDenom = 4;

        ImGui::Spacing();
        ImGui::SeparatorText("Debug");
        ImGui::Spacing();

        if (ImGui::Combo("Channel##Debug", (int*)&m_CloudsDebugChannel,
            "Disabled\0Transmittance\0\0"))
        {
            auto debugChannel = (alm::gfx::CloudsRenderStage::DebugChannel)m_CloudsDebugChannel;
            cloudsRS->SetDebugChannel(debugChannel);
        }

        ImGui::Spacing();

        if (ImGui::Button("Open Shape Texture"))
        {
            auto tex = atmos->GetCloudsShapeTexture();
            AddTextureWindow(tex->GetDebugName(), tex);
        }

        ImGui::SameLine();

        if (ImGui::Button("Open Detail Texture"))
        {
            auto tex = atmos->GetCloudsDetailTexture();
            AddTextureWindow(tex->GetDebugName(), tex);
        }
    }
}
