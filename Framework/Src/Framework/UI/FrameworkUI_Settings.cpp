// FrameworkUI "Settings" window (View > Settings).
// This is part of the alm::fw::FrameworkUI class: its member functions are split
// across several FrameworkUI_*.cpp files, grouped by UI window.

#include "Framework/FrameworkPCH.h"
#include "Framework/CameraController.h"
#include "Framework/UI/FrameworkUI.h"
#include "Framework/UI/FrameworkUIWidgets.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneLights.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderGraph.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Camera.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/DepthPrepassRenderStage.h"
#include "Gfx/RenderStages/GBuffersRenderStage.h"
#include "Gfx/RenderStages/WBOITAccumRenderStage.h"
#include "Gfx/RenderStages/WireframeRenderStage.h"
#include "Gfx/RenderStages/GPUCullingRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderStages/GridRenderStage.h"
#include "Gfx/RenderStages/ObjectOutlineRenderStage.h"
#include "Gfx/RenderStages/ExposureRenderStage.h"
#include "RHI/Device.h"

void alm::fw::FrameworkUI::BuildSettingsWindow()
{
    if (!m_ShowSettings)
        return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Settings", &m_ShowSettings, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;

    BuildRenderModesSettings();
    BuildCameraSettings(availWidth);
    BuildDebugViewSettings(availWidth);
    BuildShadowmapSettings(availWidth);
    BuildDirectionalLightSettings(availWidth);
    BuildAmbientLightSettings(availWidth);
    BuildSkySettings(availWidth);
    BuildsCloudsSettings();
    BuildMaterialChannelsSettings(availWidth);
    BuildSSAOSettings(availWidth);
    BuildBloomSettings(availWidth);
    BuildExposureSettings(availWidth);
    BuildTonemappingSettings(availWidth);

    ImGui::End();
}

void alm::fw::FrameworkUI::BuildRenderModesSettings()
{
    if (ImGui::CollapsingHeader("Render modes", ImGuiTreeNodeFlags_None))
    {
        std::vector<std::string> renderModes = m_RenderViewUI->GetRenderGraph()->GetRenderModes();
        int selectedIdx = 0;
        for (int i = 0; i < renderModes.size(); ++i)
        {
            if (renderModes[i] == m_RenderViewUI->GetRenderGraph()->GetCurrentRenderMode())
            {
                selectedIdx = i;
                break;
            }
        }

        if (ImGui::BeginListBox("##RenderModesListBox"))
        {
            for (int n = 0; n < renderModes.size(); n++)
            {
                const bool is_selected = (n == selectedIdx);
                if (ImGui::Selectable(renderModes[n].c_str(), is_selected))
                    selectedIdx = n;
                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        if (renderModes[selectedIdx] != m_RenderViewUI->GetRenderGraph()->GetCurrentRenderMode())
            FrameworkData.RequestedRenderMode = renderModes[selectedIdx];
    }
}

void alm::fw::FrameworkUI::BuildCameraSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
    {
        auto camera = m_RenderViewUI->GetCamera();

        // Position
        float3 pos = camera->GetPosition();
        if (ImGui::InputFloat3("Position", (float*)&pos))
        {
            camera->SetPosition(pos);
        }

        // Direction
        float yaw = camera->GetYaw();
        float pitch = camera->GetPitch();
        float roll = camera->GetRoll();

        if (ImGui::SliderAngle("Yaw", &yaw, -180.0f, 180.0f))
            camera->SetYaw(yaw);
        if (ImGui::SliderAngle("Pitch", &pitch, -90.0f, 90))
            camera->SetPitch(pitch);
        if (ImGui::SliderAngle("Roll", &roll, -180, 180.f))
            camera->SetRoll(roll);

        // FOV
        float fov = camera->GetVerticalFOV();
        if (ImGui::SliderAngle("FOV", &fov, 0.f, 180.f))
        {
            camera->SetVerticalFov(fov);
        }

        // 3 elements in a row
        float itemWidth = availWidth / 4;

        // Near plane
        ImGui::SetNextItemWidth(itemWidth);
        float znear = camera->GetZNear();
        if (ImGui::InputFloat("Near", &znear, 0.f, 0.f, "%.3f", 0))
        {
            camera->SetZNear(znear);
        }

        // Speed 
        if (m_CameraController)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + itemWidth / 2.f);
            float speed = m_CameraController->GetSpeed();
            if (ImGui::InputFloat("Speed", &speed, 0.f, 0.f, "%.3f", 0))
            {
                m_CameraController->SetSpeed(speed);
            }
        }
    }
}

void alm::fw::FrameworkUI::BuildDebugViewSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Debug view", ImGuiTreeNodeFlags_None))
    {
        ImGui::Checkbox("Mesh BBoxes", &FrameworkData.Debug.ShowMeshBBoxes);
        ImGui::SameLine(availWidth / 2);
        ImGui::Checkbox("Light BBoxes", &FrameworkData.Debug.ShowLightBBoxes);
        ImGui::Checkbox("Heightmap BBoxes", &FrameworkData.Debug.ShowHeightmapBBoxes);
    }
}

void alm::fw::FrameworkUI::BuildShadowmapSettings(float availWidth)
{
    const ImGuiStyle& style = ImGui::GetStyle();

    if (ImGui::CollapsingHeader("Shadowmap", ImGuiTreeNodeFlags_None))
    {
        auto shadowmapRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::ShadowmapRenderStage>();
        if (!shadowmapRS)
            return;

        auto lightingRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::DeferredLightingRenderStage>();

        ImGui::SetNextItemWidth(availWidth / 2);
        bool isEnabled = shadowmapRS->IsEnabled();
        if(ImGui::Checkbox("Enabled##Shadowmap", &isEnabled))
            shadowmapRS->SetEnabled(isEnabled);

        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 2);
        bool showShadowmap = lightingRS->GetShowShadowmap();
        if(ImGui::Checkbox("Show##Shadowmap", &showShadowmap))
            lightingRS->ShowShadowmap(showShadowmap);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetNextItemWidth(availWidth / 4);
        int depthBias = shadowmapRS->GetDepthBias();
        if(ImGui::InputInt("Depth Bias##Shadowmap", &depthBias))
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

        if (m_ShadowmapSizeCached.x < 0)
            m_ShadowmapSizeCached = shadowmapRS->GetSize();

        ImGui::SetNextItemWidth(itemWidth);
        ImGui::InputInt("##sizex", (int*)&m_ShadowmapSizeCached.x, ImGuiInputTextFlags_None);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(itemWidth);
        ImGui::InputInt("##sizey", (int*)&m_ShadowmapSizeCached.y, ImGuiInputTextFlags_None);
        ImGui::SameLine();

        if (ImGui::Button("Reset", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
            m_ShadowmapSizeCached = shadowmapRS->GetSize();

        ImGui::SameLine();
        if (ImGui::Button("Apply", ImVec2{ itemWidth - style.ItemSpacing.x / 2, 0 }))
        {
            FrameworkData.ShadowmapSize = m_ShadowmapSizeCached;
        }
    }
}

void alm::fw::FrameworkUI::BuildDirectionalLightSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Directional light"))
    {
        const float minAzimuth = -180.0;
        const float maxAzimuth = 180.0;
        const float minElevation = -90.0;
        const float maxElevation = 90.0;

        FrameworkData.SunParamsUpdated |= ImGui::SliderScalar(
            "Azimuth", ImGuiDataType_Float, &FrameworkData.SunParams.AzimuthDeg, &minAzimuth, &maxAzimuth, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        FrameworkData.SunParamsUpdated |= ImGui::SliderScalar(
            "Elevation", ImGuiDataType_Float, &FrameworkData.SunParams.ElevationDeg, &minElevation, &maxElevation, "%.1f deg", ImGuiSliderFlags_NoRoundToFormat);
        FrameworkData.SunParamsUpdated |= ImGui::ColorEdit3(
            "Color", &FrameworkData.SunParams.Color.x, ImGuiColorEditFlags_Float);
        FrameworkData.SunParamsUpdated |= ImGui::SliderFloat(
            "Irradiance", &FrameworkData.SunParams.Irradiance, 0.f, 100.f, "%.2f", ImGuiSliderFlags_Logarithmic);
        FrameworkData.SunParamsUpdated |= ImGui::SliderFloat(
            "Angular Size", &FrameworkData.SunParams.AngularSizeDeg, 0.0f, 20.f);
    }
}

void alm::fw::FrameworkUI::BuildAmbientLightSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Ambient"))
    {
        FrameworkData.AmbientParamsUpdated |= ImGui::SliderFloat("Intensity", &FrameworkData.AmbientParams.Intensity, 0.f, 1.f);
        FrameworkData.AmbientParamsUpdated |= ImGui::ColorEdit3("Sky Color", &FrameworkData.AmbientParams.SkyColor.x, ImGuiColorEditFlags_Float);
        FrameworkData.AmbientParamsUpdated |= ImGui::ColorEdit3("Ground Color", &FrameworkData.AmbientParams.GroundColor.x, ImGuiColorEditFlags_Float);
    }
}

void alm::fw::FrameworkUI::BuildSkySettings(float availWidth)
{
    auto simpleSky = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::SimpleSkyRenderStage>();
    if (simpleSky)
    {
        if (ImGui::CollapsingHeader("Simple Sky"))
        {
            bool isEnabled = simpleSky->IsEnabled();
            ImGui::Checkbox("Enabled##Sky", &isEnabled);
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
            ImGui::SliderFloat("Glow sharpness", &FrameworkData.SimpleSkyParams.glowIntensity, 1.f, 10.f);
            ImGui::InputFloat("Max radiance", &FrameworkData.SimpleSkyParams.maxLightRadiance);
        }
    }

    auto sky = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::SkyRenderStage>();
    if (sky)
    {
        if (ImGui::CollapsingHeader("Sky"))
        {
            ImGui::Checkbox("Enabled##Sky", &FrameworkData.SkyEnabled);

            ImGui::SameLine();
            float gpuMs = m_RenderViewUI->GetRenderGraph()->GetStageGPUTimeMs(alm::gfx::SkyRenderStage::StaticType());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            TextRightAligned("GPU: %1.2f ms", gpuMs);
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::InputFloat("Atmospheric depth", &FrameworkData.SkyParams.AtmosHeight);
            ImGui::InputFloat3("Rayleigh Wave Lengths", &FrameworkData.SkyParams.RayleighWaveLengths.x, "%.1f");
            ImGui::InputFloat("Turbidity factor", &FrameworkData.SkyParams.Turbidity);
            ImGui::SliderFloat("Mie Anisotropy##Sky", &FrameworkData.SkyParams.MieAnisotropy, 0.f, 1.f);
            ImGui::InputScalar("Atmos interations##Sky", ImGuiDataType_U32, &FrameworkData.SkySimParams.NumSteps);
            ImGui::InputScalar("Light interations##Sky", ImGuiDataType_U32, &FrameworkData.SkySimParams.NumLightSteps);
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

void alm::fw::FrameworkUI::BuildMaterialChannelsSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Material channels"))
    {
        if (ImGui::RadioButton("Disabled##matDisabled", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Disabled;
        if (ImGui::RadioButton("Base Color##matBaseColor", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::BaseColor;
        if (ImGui::RadioButton("Metalness##matMetalness", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Metalness;
        if (ImGui::RadioButton("Anisotropy##matAnisotropy", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Anisotropy;
        if (ImGui::RadioButton("Roughness##matRoughness", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Roughness;
        if (ImGui::RadioButton("Scattering##matScattering", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Scattering;
        if (ImGui::RadioButton("Translucency##matTranslucency", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Translucency;
        if (ImGui::RadioButton("Normal Map##matNormalMap", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::NormalMap;
        if (ImGui::RadioButton("Occlusion Map##matOcclusionMap", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::OcclusionMap;
        if (ImGui::RadioButton("Emissive##matEmissive", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::Emissive;
        if (ImGui::RadioButton("Specular F0##matSpecularF0", FrameworkData.MatChannel == alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0))
            FrameworkData.MatChannel = alm::gfx::DeferredLightingRenderStage::MaterialChannel::SpecularF0;
    }
}

void alm::fw::FrameworkUI::BuildSSAOSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("SSAO"))
    {
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Enabled##SSAO", &FrameworkData.SSAO.Enabled);
        ImGui::SameLine(availWidth / 2);
        ImGui::SetNextItemWidth(availWidth / 2);
        ImGui::Checkbox("Show##SSAO", &FrameworkData.SSAO.View);

        ImGui::InputFloat("Radius##SSAO", &FrameworkData.SSAO.Radius);
        ImGui::InputFloat("Power##SSAO", &FrameworkData.SSAO.Power);
        ImGui::InputFloat("Bias##Bias", &FrameworkData.SSAO.Bias);
    }
}

void alm::fw::FrameworkUI::BuildBloomSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Bloom"))
    {
        ImGui::Checkbox("Enabled##Bloom", &FrameworkData.Bloom.Enabled);
        ImGui::InputFloat("Radius##Bloom", &FrameworkData.Bloom.Radius);
        ImGui::InputFloat("Strength##Bloom", &FrameworkData.Bloom.Strength);

        // Calc whats the max mip chain
        alm::rhi::TextureHandle tex = m_RenderViewUI->GetRenderGraph()->GetTexture("SceneColor");
        int maxMips = static_cast<int>(std::floor(std::log2((std::max)(tex->GetDesc().width, tex->GetDesc().height))));
        ImGui::SliderInt("MaxMip##Bloom", &FrameworkData.Bloom.MaxMip, 1, maxMips);
    }
}

void alm::fw::FrameworkUI::BuildExposureSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Exposure"))
    {
        // Luminance

        const float minLogLuminance = -20.f;
        const float maxLogLuminance = 0.f;
        ImGui::SliderScalar("Min log luminance", ImGuiDataType_Float, &FrameworkData.Exposure.MinLogLuminance,
            &minLogLuminance, &maxLogLuminance, "%.3f");

        const float minLogRange = 0.f;
        const float maxLogRange = 36.f;
        ImGui::SliderScalar("Log luminance range", ImGuiDataType_Float, &FrameworkData.Exposure.LogLuminanceRange,
            &minLogRange, &maxLogRange, "%.3f");

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        const float minLum = exp2(FrameworkData.Exposure.MinLogLuminance);
        const float maxLum = minLum * std::exp2(FrameworkData.Exposure.LogLuminanceRange);
        ImGui::Text("Luminance [%1.6f .. %1.6f]", minLum, maxLum);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::SliderFloat("Middle Gray Nits", &FrameworkData.Exposure.MiddleGrayNits, 0.f, 1000.f);

        const float minExposureBias = 0.f;
        const float maxExposureBias = 1.f;
        ImGui::SliderScalar("SDR Exposure Bias", ImGuiDataType_Float, &FrameworkData.Exposure.SdrExposureBias,
            &minExposureBias, &maxExposureBias, "%.3f");

        ImGui::Spacing();

        ImGui::InputFloat("Dark to light adaptation speed", &FrameworkData.Exposure.AdaptationUpSpeed);
        ImGui::InputFloat("Light to dark adaptation speed", &FrameworkData.Exposure.AdaptationDownSpeed);

        ImGui::Spacing();

        m_ShowLuminanceHistogram |= ImGui::Button("View Histogram");
    }
}

void alm::fw::FrameworkUI::BuildTonemappingSettings(float availWidth)
{
    if (ImGui::CollapsingHeader("Tonemapping"))
    {
        ImGui::Checkbox("Enabled##Tonemapping", &FrameworkData.Tonemapping.Enabled);

        alm::rhi::ColorSpace colorSpace = GetDeviceManager()->GetColorSpace();
        ImGui::SameLine(0.f, availWidth / 8);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        TextRightAligned("Color Space: %s", colorSpace == alm::rhi::ColorSpace::HDR10_ST2084 ? "HDR10" : "SDR");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SliderFloat("Paper White Nits", &FrameworkData.Tonemapping.PaperWhiteNits, 0.f, 1000.f);
    }
}

void alm::fw::FrameworkUI::BuildLumninanceHistogram()
{
    auto exposureRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::ExposureRenderStage>();

    if (!exposureRS || !m_ShowLuminanceHistogram)
        return;

    alm::gfx::RenderGraph* renderGraph = m_RenderViewUI->GetRenderGraph().get();

    if (!m_LumHistogramBufferTicket.IsValid())
    {
        m_LumHistogramBufferTicket = renderGraph->RequestBufferView(
            exposureRS->GetType(), alm::gfx::RenderGraph::AccessMode::Write, renderGraph->GetBufferHandle("LuminanceHistogram"));
    }
    if (!m_LumHistogramBufferTicket.IsValid())
    {
        LOG_ERROR("Failed to requesting histogram buffer ticket");
        return;
    }

    ImGui::SetNextWindowSize(ImVec2{ 600, 200 }, ImGuiCond_Once);
    if (!ImGui::Begin("Luminance histogram", &m_ShowLuminanceHistogram, ImGuiWindowFlags_None))
    {
        ImGui::End();
        if (!m_ShowLuminanceHistogram)
        {
            renderGraph->ReleaseBufferView(m_LumHistogramBufferTicket);
            m_LumHistogramBufferTicket = { nullptr };
        }
        return;
    }

    alm::gfx::ExposureRenderStage::Stats stats = exposureRS->GetStats();
    alm::rhi::BufferHandle buffer = renderGraph->GetBufferView(m_LumHistogramBufferTicket);
    if (buffer)
    {
        void* data_ptr = buffer->Map();
        auto getValueLog2 = [](void* data, int idx) -> float
            {
                uint32_t* arr = (uint32_t*)data;
                return log2f((float)arr[idx] + 1.0f);
            };
        auto getValueLinear = [](void* data, int idx) -> float
            {
                uint32_t* arr = (uint32_t*)data;
                return (float)arr[idx];
            };

        ImVec2 size = ImGui::GetContentRegionAvail();
        size.y -= ImGui::GetFrameHeightWithSpacing();
        if (m_LumHistogramMode == 0)
        {
            ImGui::PlotHistogram("##LuminanceHistogram",
                getValueLog2, data_ptr, buffer->GetDesc().sizeBytes / 4, 0, nullptr, 0.f, log2f(stats.totalPixels), size);
        }
        else
        {
            ImGui::PlotHistogram("##LuminanceHistogram",
                getValueLinear, data_ptr, buffer->GetDesc().sizeBytes / 4, 0, nullptr, 0.f, stats.totalPixels, size);
        }

        buffer->Unmap();

        if (ImGui::RadioButton("Log2", m_LumHistogramMode == 0))
            m_LumHistogramMode = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Linear", m_LumHistogramMode == 1))
            m_LumHistogramMode = 1;
        ImGui::SameLine();
        ImGui::Text("| Min: %1.3f | Max: %1.3f | Avg: %1.3f | Bin: %1.1f", stats.minLuminance, stats.maxLuminance, stats.avgLuminance, stats.avgBin);
    }

    ImGui::End();
}

