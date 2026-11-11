// "Atmosphere / Lighting" window (View > Atmosphere / Lighting). UIWindow
// subclass of the FrameworkUI window set (see Framework/UI/UIWindows.h). The
// Shadowmap, Directional light and Ambient sections are the shared widgets of
// SettingsWidgets.h, shown by this window and by the Settings window.

#include "Framework/FrameworkPCH.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/UIWindows.h"
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
#include "Gfx/SceneIBL.h"

void alm::fw::AtmosphereWindow::Build()
{
    if (!m_Open)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Atmosphere / Lighting", &m_Open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    auto renderView = m_UI.GetUIRenderView();
    auto rg = renderView->GetRenderGraph();
    auto& data = m_UI.FrameworkData;
    const bool hasSimpleSky = rg->GetRenderStage<gfx::SimpleSkyRenderStage>() != nullptr;
    const bool hasAtmosSky = rg->GetRenderStage<gfx::AtmosSkyRenderStage>() != nullptr;
    const bool hasEnvSky = rg->GetRenderStage<gfx::CubemapSkyRenderStage>() != nullptr;
    const bool atmosphereAvailable = hasSimpleSky || hasAtmosSky;
    const bool environmentAvailable = hasEnvSky;

    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;
    const float startX = ImGui::GetCursorPosX();
    const float halfWidth = availWidth / 2.0f;

    ImGui::SeparatorText("Lighting Mode");
    gfx::LightingMode lm = m_UI.GetScene()->GetLightingMode();

    ImGui::SetCursorPosX(startX + (halfWidth - ImGui::CalcTextSize("Atmosphere").x - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::BeginDisabled(!atmosphereAvailable);
    if (ImGui::RadioButton("Atmosphere##Atmos", lm == gfx::LightingMode::Atmosphere))
        lm = gfx::LightingMode::Atmosphere;
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + halfWidth + (halfWidth - ImGui::CalcTextSize("Environment").x - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::BeginDisabled(!environmentAvailable);
    if (ImGui::RadioButton("Environment##Atmos", lm == gfx::LightingMode::Environment))
        lm = gfx::LightingMode::Environment;
    ImGui::EndDisabled();

    if (lm != m_UI.GetScene()->GetLightingMode())
    {
        // Stage enables + Sky Source validation refresh per-frame via FrameworkApp::UpdateRenderStagesState
        m_UI.GetScene()->SetLightingMode(lm);
    }

    // Sky background source (constrained by lighting mode: Atmosphere -> Simple/Atmos; Environment -> Panorama)
    const bool simpleSkyAvailable = atmosphereAvailable && hasSimpleSky && lm == gfx::LightingMode::Atmosphere;
    const bool atmosSkyAvailable = atmosphereAvailable && hasAtmosSky && lm == gfx::LightingMode::Atmosphere;
    const bool cubemapSkyAvailable = environmentAvailable && hasEnvSky && lm == gfx::LightingMode::Environment;

    ImGui::SeparatorText("Sky Background");
    const float skyThirdWidth = availWidth / 3.0f;
    gfx::SkySource skySource = data.SkySourceOption;

    ImGui::SetCursorPosX(startX + (skyThirdWidth - ImGui::CalcTextSize("Simple").x - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::BeginDisabled(!simpleSkyAvailable);
    if (ImGui::RadioButton("Simple##SkyBg", skySource == gfx::SkySource::Simple))
        skySource = gfx::SkySource::Simple;
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + skyThirdWidth + (skyThirdWidth - ImGui::CalcTextSize("Atmospheric").x - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::BeginDisabled(!atmosSkyAvailable);
    if (ImGui::RadioButton("Atmospheric##SkyBg", skySource == gfx::SkySource::Atmos))
        skySource = gfx::SkySource::Atmos;
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + skyThirdWidth * 2.0f + (skyThirdWidth - ImGui::CalcTextSize("Panorama").x - ImGui::GetFrameHeight()) * 0.5f);
    ImGui::BeginDisabled(!cubemapSkyAvailable);
    if (ImGui::RadioButton("Panorama##SkyBg", skySource == gfx::SkySource::Cubemap))
        skySource = gfx::SkySource::Cubemap;
    ImGui::EndDisabled();

    data.SkySourceOption = skySource;

    m_UI.GetDirectionalLightWidget().Build(availWidth);
    m_UI.GetAmbientLightWidget().Build(availWidth);
    BuildSimpleSkySettings();
    BuildSimSkySettings();
    BuildCloudsSettings();
    BuildCubemapSettings(availWidth);
    m_UI.GetShadowmapWidget().Build(availWidth);

    ImGui::End();
}

void alm::fw::AtmosphereWindow::BuildSimpleSkySettings()
{
    if (ImGui::CollapsingHeader("Simple Sky"))
    {
        if (m_UI.GetScene()->GetLightingMode() != gfx::LightingMode::Atmosphere)
        {
            ImGui::TextUnformatted("Unavailable - Change Lighting Mode in Atmosphere panel");
            return;
        }
        if (m_UI.FrameworkData.SkySourceOption != gfx::SkySource::Simple)
        {
            ImGui::TextUnformatted("Unavailable - Set Sky Background to Simple");
            return;
        }

        auto simpleSky = m_UI.GetUIRenderView()->GetRenderGraph()->GetRenderStage<alm::gfx::SimpleSkyRenderStage>();
        if (simpleSky)
        {
            auto& params = m_UI.FrameworkData.SimpleSkyParams;
            ImGui::ColorEdit3("Sky color##Sky", &params.skyColor.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Horizon color##Sky", &params.horizonColor.x, ImGuiColorEditFlags_Float);
            ImGui::ColorEdit3("Ground color##Sky", &params.groundColor.x, ImGuiColorEditFlags_Float);
            ImGui::SliderFloat("Brightness scaler", &params.brightness, 0.f, 10.f);

            float horizonSizeRad = glm::radians(params.horizonSize);
            if (ImGui::SliderAngle("Horizon size", &horizonSizeRad, 0.f, 90.f))
            {
                params.horizonSize = glm::degrees(horizonSizeRad);
            }
            float glowSizeRad = glm::radians(params.glowSize);
            if (ImGui::SliderAngle("Glow size", &glowSizeRad, 0.f, 90.f))
            {
                params.glowSize = glm::degrees(glowSizeRad);
            }

            ImGui::SliderFloat("Glow intensity", &params.glowIntensity, 0.f, 1.f);
            ImGui::SliderFloat("Glow sharpness", &params.glowSharpness, 1.f, 10.f);
            ImGui::InputFloat("Max radiance", &params.maxLightRadiance);
        }
        else
        {
            ImGui::TextUnformatted("No SimpleSkyRenderStage present");
        }
    }
}

void alm::fw::AtmosphereWindow::BuildCubemapSettings(float availWidth)
{
    auto scene = m_UI.GetScene();
    gfx::SceneIBL* sceneIBL = scene ? scene->GetSceneIBL() : nullptr;

    if (sceneIBL && ImGui::CollapsingHeader("IBL"))
    {
        if (m_UI.GetScene()->GetLightingMode() != gfx::LightingMode::Environment)
        {
            ImGui::TextUnformatted("Unavailable - Change Lighting Mode in Atmosphere panel");
            return;
        }

        if (m_UI.FrameworkData.SkySourceOption != gfx::SkySource::Cubemap)
        {
            ImGui::TextUnformatted("SkyBox background disabled - Set Sky Background to Panorama");
        }

        ImGui::SeparatorText("Panorama HDR Texture");

        std::string currentPanorama = sceneIBL->GetPanoramaPath();
        std::string title;
        if (currentPanorama.empty())
            title = "Browse";
        else
            title = GetFilenameFromPath(currentPanorama);

        if (ImGui::Button(title.c_str(), ImVec2(-1, 0)))
        {
            std::string newPanorama = m_UI.OpenFileNativeDialog(currentPanorama, {
                { "Image files", "*.png;*.tga;*.jpg;*.jpeg;*.dds;*.hdr;*.ktx2" },
                { "TGA", "*.tga" },
                { "PNG", "*.png" },
                { "JPEG", "*.jpg;*.jpeg" },
                { "DDS", "*.dds" },
                { "KTX2", "*.ktx2" },
                { "Radiance", "*.hdr" } });
            if (!newPanorama.empty())
            {
                auto result = sceneIBL->SetPanorama(newPanorama);
                if (!result)
                    m_UI.ShowErrorModal(result.error());
            }
        }

        ImGui::Separator();
        float buttonWidth = (availWidth - ImGui::GetStyle().ItemSpacing.x) / 4;

        auto skyTexture = sceneIBL->GetSkyTexture();
        ImGui::BeginDisabled(!(skyTexture && skyTexture->IsValid()));
        if (ImGui::Button("SkyBox", ImVec2(buttonWidth, 0)))
        {
            m_UI.AddTextureWindow(skyTexture->id, skyTexture->texture.get_weak());
        };
        ImGui::EndDisabled();

        ImGui::SameLine();
        auto lambertTexture = sceneIBL->GetLambertTexture();
        ImGui::BeginDisabled(!(lambertTexture && lambertTexture->IsValid()));
        if (ImGui::Button("Lambert", ImVec2(buttonWidth, 0)))
        {
            m_UI.AddTextureWindow(lambertTexture->id, lambertTexture->texture.get_weak());
        };
        ImGui::EndDisabled();

        ImGui::SameLine();
        auto ggxTexture = sceneIBL->GetGGXTexture();
        ImGui::BeginDisabled(!(ggxTexture && ggxTexture->IsValid()));
        if (ImGui::Button("GGX", ImVec2(buttonWidth, 0)))
        {
            m_UI.AddTextureWindow(ggxTexture->id, ggxTexture->texture.get_weak());
        };
        ImGui::EndDisabled();

        ImGui::SameLine();
        auto dfgTexture = sceneIBL->GetDfgTexture();
        ImGui::BeginDisabled(!(dfgTexture && dfgTexture->IsValid()));
        if (ImGui::Button("DFG LUT", ImVec2(buttonWidth, 0)))
        {
            m_UI.AddTextureWindow(dfgTexture->id, dfgTexture->texture.get_weak());
        };
        ImGui::EndDisabled();
    }
}

void alm::fw::AtmosphereWindow::BuildSimSkySettings()
{
    if (ImGui::CollapsingHeader("Atmospheric Sky"))
    {
        if (m_UI.GetScene()->GetLightingMode() != gfx::LightingMode::Atmosphere)
        {
            ImGui::TextUnformatted("Unavailable - Change Lighting Mode in Atmosphere panel");
            return;
        }
        if (m_UI.FrameworkData.SkySourceOption != gfx::SkySource::Atmos)
        {
            ImGui::TextUnformatted("Unavailable - Set Sky Background to Atmospheric");
            return;
        }

        auto rg = m_UI.GetUIRenderView()->GetRenderGraph();
        auto atmosSky = rg->GetRenderStage<alm::gfx::AtmosSkyRenderStage>();
        if (atmosSky)
        {
            float gpuMs = rg->GetStageGPUTimeMs(alm::gfx::AtmosSkyRenderStage::StaticType());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            TextRightAligned("GPU: %1.2f ms", gpuMs);
            ImGui::PopStyleColor();

            ImGui::Spacing();

            auto& skyParams = m_UI.FrameworkData.AtmosSkyParams;
            auto& simParams = m_UI.FrameworkData.AtmosSkySimParams;
            ImGui::InputFloat("Atmospheric depth", &skyParams.AtmosHeight);
            ImGui::InputFloat3("Rayleigh Wave Lengths", &skyParams.RayleighWaveLengths.x, "%.1f");
            ImGui::InputFloat("Turbidity factor", &skyParams.Turbidity);
            ImGui::SliderFloat("Mie Anisotropy##Sky", &skyParams.MieAnisotropy, 0.f, 1.f);
            ImGui::InputScalar("Atmos interations##Sky", ImGuiDataType_U32, &simParams.NumSteps);
            ImGui::InputScalar("Light interations##Sky", ImGuiDataType_U32, &simParams.NumLightSteps);
        }
        else
        {
            ImGui::TextUnformatted("No AtmosSkyRenderStage present");
        }
    }
}

void alm::fw::AtmosphereWindow::BuildCloudsSettings()
{
    if (ImGui::CollapsingHeader("Clouds"))
    {
        if (m_UI.GetScene()->GetLightingMode() != gfx::LightingMode::Atmosphere)
        {
            ImGui::TextUnformatted("Unavailable - Change Lighting Mode in Atmosphere panel");
            return;
        }
        if (m_UI.FrameworkData.SkySourceOption != gfx::SkySource::Atmos)
        {
            ImGui::TextUnformatted("Unavailable - Set Sky Background to Atmospheric");
            return;
        }

        auto& data = m_UI.FrameworkData;
        const auto& scene = m_UI.GetScene();
        auto rg = m_UI.GetUIRenderView()->GetRenderGraph();
        auto cloudsRS = rg->GetRenderStage<alm::gfx::CloudsRenderStage>();
        gfx::AtmosphereConfig* atmos = scene ? scene->GetAtmosphereConfig() : nullptr;

        if (!cloudsRS)
        {
            ImGui::TextUnformatted("No CloudsRenderStage present");
            return;
        }
        if (!atmos->CloudsSubsystemInitialized())
        {
            ImGui::TextUnformatted("Clouds subsystem not initialized");
            if (ImGui::Button("Initialize!", ImVec2(-1, 0)))
            {
                atmos->InitCloudsSubsystem();
            }
            return;
        }

        ImGui::Checkbox("Enabled##Clouds", &data.CloudsEnabled);

        ImGui::SameLine();
        float gpuMs = rg->GetStageGPUTimeMs(alm::gfx::CloudsRenderStage::StaticType());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        TextRightAligned("GPU: %1.2f ms", gpuMs);
        ImGui::PopStyleColor();

        ImGui::BeginDisabled(!data.CloudsEnabled);
        ImGui::Checkbox("Shadows##Clouds", &data.CloudsShadowsEnabled);
        ImGui::SetItemTooltip("Clouds cast shadows onto the scene (CloudsShadowmapRenderStage)");

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
        if (ImGui::SliderFloat("Scattering Coeff 1/km (norm)##Clouds", &muS_Km, 0.f, 10.f))
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

        ImGui::InputScalar("Density interations##Clouds", ImGuiDataType_U32, &data.CloudsSimParams.CloudRaymarchIterations);
        ImGui::Checkbox("Volumetric shadows##Clouds", &data.CloudsSimParams.VolumetricShadows);
        ImGui::InputScalar("Light interations##Clouds", ImGuiDataType_U32, &data.CloudsSimParams.LightRaymarchIterations);
        ImGui::SliderInt("Multi scatter octaves##Clouds", (int*)&data.CloudsSimParams.MultiScatterOctaves, 0, 8);

        ImGui::Spacing();

        if (ImGui::RadioButton("Render Target 1/1##Clouds", data.CloudsRTDenom == 1))
            data.CloudsRTDenom = 1;
        ImGui::SameLine();
        if (ImGui::RadioButton("Render Target 1/2##Clouds", data.CloudsRTDenom == 2))
            data.CloudsRTDenom = 2;
        ImGui::SameLine();
        if (ImGui::RadioButton("Render Target 1/4##Clouds", data.CloudsRTDenom == 4))
            data.CloudsRTDenom = 4;

        ImGui::Spacing();
        ImGui::SeparatorText("Debug");
        ImGui::Spacing();

        if (ImGui::Combo("Channel##Debug", (int*)&m_CloudsDebugChannel,
            "Disabled\0Transmittance\0\0"))
        {
            cloudsRS->SetDebugChannel(m_CloudsDebugChannel);
        }

        ImGui::Spacing();

        if (ImGui::Button("Open Shape Texture"))
        {
            auto tex = atmos->GetCloudsShapeTexture();
            m_UI.AddTextureWindow(tex->GetDebugName(), tex);
        }

        ImGui::SameLine();

        if (ImGui::Button("Open Detail Texture"))
        {
            auto tex = atmos->GetCloudsDetailTexture();
            m_UI.AddTextureWindow(tex->GetDebugName(), tex);
        }

        ImGui::EndDisabled();
    }
}
