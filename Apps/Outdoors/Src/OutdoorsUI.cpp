#include "Framework/FrameworkPCH.h"
#include "OutdoorsUI.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapInstance.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/RenderView.h"
#include "Gfx/TerrainMaterial.h"
#include "Gfx/LoadedTexture.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/TextureCache.h"
#include "Gfx/HeightmapSource.h"
#include "Gfx/RenderStages/GBuffersRenderStage.h"

void OutdoorsUI::Init(SDL_Window* window, alm::weak<alm::gfx::Scene> scene, alm::weak<alm::gfx::RenderView> renderView,
	alm::fw::CameraController* cameraController)
{
	FrameworkUI::Init(window, scene, renderView, cameraController);
	
	RegisterMainMenuItem("Heightmap", [this]() { BuildHeightmapMenuItem(); });
}

void OutdoorsUI::BuildUI()
{
	alm::fw::FrameworkUI::BuildUI();

	if (m_ShowHeightmapSettings)
	{
		ImGui::SetNextWindowSize(ImVec2(500, 900), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Heightmap Settings", &m_ShowHeightmapSettings, ImGuiWindowFlags_None))
		{
			ImGui::End();
			return;
		}

		const ImGuiStyle& style = ImGui::GetStyle();
		const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;
		alm::gfx::HeightmapInstance* heightmapInstace = m_RenderViewUI->GetHeightmapInstance(m_SceneHeightmap.get());
		alm::gfx::Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();
		const auto& dataSource = heightmap->GetSource();
		auto sourceType = dataSource->GetType();

		if (ImGui::CollapsingHeader("Data source", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (m_ComboDataSource == -1)
			{
				m_ComboDataSource = (int)sourceType;
				if (m_ComboDataSource == (int)alm::gfx::IHeightmapSource::Type::Noise)
				{
					auto* noiseSource = static_cast<alm::gfx::NoiseHeightmapSource*>(dataSource.get());

					m_NoiseHeightmapParams = noiseSource->GetParams();
					m_NoiseTextureSize = heightmap->GetTextureResolution().x;
				}
			}

			if (ImGui::Combo("Type", &m_ComboDataSource, "Noise\0Image\0\0"))
			{
				// TODO
			}

			ImGui::Spacing();

			if (m_ComboDataSource == (int)alm::gfx::IHeightmapSource::Type::Noise)
			{
				ImGui::InputFloat("Frequency##NoiseParams", &m_NoiseHeightmapParams.Frequency, 0.0, 0.0, "%.2f", ImGuiInputTextFlags_None);
				ImGui::InputInt("Octaves##NoiseParams", &m_NoiseHeightmapParams.Octaves);
				ImGui::InputFloat2("Offset##NoiseParams", &m_NoiseHeightmapParams.OffsetX, "%.2f");

				ImGui::SetCursorPosX(style.ItemSpacing.x);
				if (ImGui::Button("Apply##NoiseParams", ImVec2(availWidth / 2, 0.f)))
				{
					static_cast<alm::gfx::NoiseHeightmapSource*>(dataSource.get())->SetParams(m_NoiseHeightmapParams);
					heightmap->RefreshHeightsTexture();
				}

				ImGui::SameLine();
				if (ImGui::Button("Reset##NoiseParams", ImVec2(availWidth / 2, 0.f)))
				{
					m_NoiseHeightmapParams = static_cast<alm::gfx::NoiseHeightmapSource*>(dataSource.get())->GetParams();
				}
			}
		}

		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const alm::gfx::Transform& nodeTransform = m_SceneHeightmap->GetNode()->GetLocalTransform();
			float3 pos = nodeTransform.GetTranslation();
			float3 scale = nodeTransform.GetScale();
			bool transformUpdated = false;
			transformUpdated |= ImGui::InputFloat3("Position##Heightmap", (float*)&pos.x, "%.2f", ImGuiInputTextFlags_None);
			transformUpdated |= ImGui::InputFloat3("Scale##Heightmap", (float*)&scale, "%.2f", ImGuiInputTextFlags_None);

			if (transformUpdated)
			{
				alm::gfx::Transform newTransform = nodeTransform;
				newTransform.SetTranslation(pos);
				newTransform.SetScale(scale);
				m_SceneHeightmap->GetNode()->SetLocalTransform(newTransform);
			}
		}
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Tesselation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto lodFactor = heightmapInstace->GetLODDistanceFactor();
			if(ImGui::InputFloat("LOD factor##Heightmap", &lodFactor, 0.f, 0.f, "%.2f", ImGuiInputTextFlags_None))
				heightmapInstace->SetLODDistanceFactor(lodFactor);

			ImGui::Spacing();

			ImGui::BeginDisabled(sourceType != alm::gfx::IHeightmapSource::Type::Noise);

			float itemWidth = availWidth / 4;

			ImGui::SetNextItemWidth(itemWidth);
			ImGui::InputInt("Texture size (Noise only)", (int*)&m_NoiseTextureSize);

			ImGui::SetCursorPosX(style.ItemSpacing.x);
			if (ImGui::Button("Apply##TextureResolution", ImVec2(availWidth / 3, 0.f)))
			{
				heightmap->SetTextureResolution(uint2{ m_NoiseTextureSize, m_NoiseTextureSize });
				heightmapInstace->SetMaxDepthLevel(heightmap->GetMaxDepthLevel());
			}

			ImGui::SameLine();
			if (ImGui::Button("Reset##TextureResolution", ImVec2(availWidth / 3, 0.f)))
			{
				m_NoiseTextureSize = heightmap->GetTextureResolution().x;
			}

			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("View##TextureResolution", ImVec2(availWidth / 3, 0.f)))
			{
				alm::rhi::TextureHandle texture = heightmap->GetHeightsTexture();
				AddTextureWindow(texture->GetDebugName(), texture);
			}

			ImGui::Spacing();

			// Depth level
			{
				float itemWidth = availWidth / 4;

				int maxDephLevel = heightmapInstace->GetMaxDepthLevel();
				ImGui::SetNextItemWidth(itemWidth);
				std::stringstream ss;
				ss << "Max depth level (max " << heightmap->GetMaxDepthLevel() << ")##heightmap";
				std::string label = ss.str();
				if (ImGui::InputInt(label.c_str(), &maxDephLevel))
				{
					if (m_ForceSetMaxDepth)
					{
						maxDephLevel = std::max(0, maxDephLevel);
					}
					else
					{
						uint32_t upLimit = heightmap->GetMaxDepthLevel();
						maxDephLevel = std::clamp(maxDephLevel, 0, (int)upLimit);
					}
					heightmapInstace->SetMaxDepthLevel(maxDephLevel);
				}

				ImGui::SameLine();
				ImGui::SetCursorPosX(style.ItemSpacing.x + availWidth * 4 / 5);
				ImGui::Checkbox("Force set", &m_ForceSetMaxDepth);
			}
		}
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto buildTex = [&](const char* id, std::shared_ptr<alm::gfx::LoadedTexture>& loadedTex, bool sRGB, bool isNormalTex) -> bool
			{
				bool matDirty = false;

				ImGui::PushID(id);
				TextRightAlignedPosX(availWidth / 3, id);

				ImGui::SameLine();
				ImGui::SetCursorPosX(availWidth / 3 + style.ItemSpacing.x);
				ImGui::BeginDisabled(!loadedTex);
				if (ImGui::Button("Show"))
				{
					AddTextureWindow(loadedTex->id, loadedTex->texture.get_weak());
				}
				ImGui::EndDisabled();

				ImGui::SameLine();
				if (ImGui::Button("Open"))
				{
					std::string path = OpenFileNativeDialog(loadedTex ? loadedTex->id : std::string{}, {
						{ "Image files", "*.png;*.tga;*.dds" },
						{ "TGA", "*.tga" },
						{ "PNG", "*.png" },
						{ "DDS", "*.dds" } });
					if (!path.empty())
					{
						alm::gfx::TextureCache::Flags flags = alm::gfx::TextureCache::Flags::GenerateMips;
						if (isNormalTex)
							flags |= alm::gfx::TextureCache::Flags::IsNormalMap;

						auto loadResult = GetDeviceManager()->GetTextureCache()->Load(path, flags);
						if (loadResult)
						{
							loadResult->second.Wait();
							loadedTex = loadResult->first;
							matDirty = true;
						}
					}
				}

				ImGui::SameLine();
				ImGui::BeginDisabled(!loadedTex);
				if (ImGui::Button("Clear"))
				{
					loadedTex.reset();
					matDirty = true;
				}
				ImGui::EndDisabled();

				ImGui::PopID();
				return matDirty;
			};

			auto buildMatLayer = [&](const char* id, alm::gfx::TerrainMaterialLayer& layer) -> bool
			{
				ImGui::PushID(id);
				ImGui::SeparatorText(id);

				bool matDirty = false;
				const float itemWidth = (availWidth - style.ItemSpacing.x * 4) / 3;

				matDirty |= buildTex("BaseColorTexture", layer.BaseColorTexture, true, false);
				matDirty |= buildTex("NormalTexture", layer.NormalTexture, false, true);
				matDirty |= buildTex("MetalRoughTexture", layer.MetalRoughTexture, false, false);

				matDirty |= ImGui::ColorEdit3("BaseColorTint", &layer.BaseColorTint.x, ImGuiColorEditFlags_Float);
				matDirty |= ImGui::SliderFloat("Roughness", &layer.Roughness, 0.f, 1.f, "%.2f");
				matDirty |= ImGui::SliderFloat("Metallic", &layer.Metallic, 0.f, 1.f, "%.2f");
				matDirty |= ImGui::InputFloat("UVScale", &layer.UVScale, 0.0, 0.0, "%.2f", ImGuiInputTextFlags_None);

				ImGui::PopID();
				return matDirty;
			};

			auto mat = heightmap->GetMaterial();
			bool matDirty = false;
			
			matDirty |= buildMatLayer("Ground", mat->Ground);
			matDirty |= buildMatLayer("Peak", mat->Peak);
			matDirty |= buildMatLayer("Slope", mat->Slope);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::SliderFloat("Height Transition Start", &mat->HeightTransitionStart, 0.f, 1.f, "%.2f"))
			{
				mat->HeightTransitionEnd = std::max(mat->HeightTransitionEnd, mat->HeightTransitionStart);
				matDirty = true;
			}
			if (ImGui::SliderFloat("Height Transition End", &mat->HeightTransitionEnd, 0.f, 1.f, "%.2f"))
			{
				mat->HeightTransitionStart = std::min(mat->HeightTransitionStart, mat->HeightTransitionEnd);
				matDirty = true;
			}

			float slopeAngleStart = glm::radians(mat->SlopeAngleStartDeg);
			float slopeAngleEnd = glm::radians(mat->SlopeAngleEndDeg);
			if (ImGui::SliderAngle("Slope Transition Start", &slopeAngleStart, 0.f, 90.f))
			{
				slopeAngleEnd = std::max(slopeAngleEnd, slopeAngleStart);
				mat->SlopeAngleStartDeg = glm::degrees(slopeAngleStart);
				mat->SlopeAngleEndDeg = glm::degrees(slopeAngleEnd);
				matDirty = true;
			}
			if (ImGui::SliderAngle("Slope Transition End", &slopeAngleEnd, 0.f, 90.f))
			{
				slopeAngleStart = std::min(slopeAngleStart, slopeAngleEnd);
				mat->SlopeAngleStartDeg = glm::degrees(slopeAngleStart);
				mat->SlopeAngleEndDeg = glm::degrees(slopeAngleEnd);
				matDirty = true;
			}

			if (matDirty)
			{
				GetDeviceManager()->GetGpuSceneBuffers()->SetDirtyTerrainMaterial(mat.get());
			}
		}

		if (ImGui::CollapsingHeader("Debug visualization", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Combo("Channel", &m_DebugChannel, "Disabled\0Heights\0Slope\0Normals\0\0"))
			{
				auto gBuffersRS = m_RenderViewUI->GetRenderGraph()->GetRenderStage<alm::gfx::GBuffersRenderStage>();
				gBuffersRS->SetDebugChannel((alm::gfx::GBuffersRenderStage::DebugChannel)m_DebugChannel);
			}
		}

		ImGui::End();
	}
}

void OutdoorsUI::BuildHeightmapMenuItem()
{
	if (ImGui::MenuItem("Settings", nullptr, m_ShowHeightmapSettings, m_SceneHeightmap != nullptr))
		m_ShowHeightmapSettings = !m_ShowHeightmapSettings;

	if (ImGui::MenuItem("View source data", nullptr, false, m_SceneHeightmap != nullptr))
	{
		alm::rhi::TextureHandle texture = m_SceneHeightmap->GetHeightmap()->GetHeightsTexture();
		AddTextureWindow(texture->GetDebugName(), texture);		
	}
}