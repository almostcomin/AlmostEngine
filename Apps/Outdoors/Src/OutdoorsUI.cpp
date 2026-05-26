#include "Framework/FrameworkPCH.h"
#include "OutdoorsUI.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/Heightmap.h"
#include "Gfx/HeightmapInstance.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/RenderView.h"

void OutdoorsUI::Init(SDL_Window* window, alm::weak<alm::gfx::Scene> scene, alm::weak<alm::gfx::RenderView> renderView,
	alm::fw::CameraController* cameraController)
{
	FrameworkUI::Init(window, scene, renderView, cameraController);
	
	RegisterMainMenuItem("Heightmap", [this]() { BuildHeightmapMeniItem(); });
}

void OutdoorsUI::BuildUI()
{
	alm::fw::FrameworkUI::BuildUI();

	if (m_ShowHeightmapSettings)
	{
		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Heightmap Settings", &m_ShowHeightmapSettings, ImGuiWindowFlags_None))
		{
			ImGui::End();
			return;
		}

		const ImGuiStyle& style = ImGui::GetStyle();
		const float availWidth = ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2;
		alm::gfx::HeightmapInstance* heightmapInstace = m_RenderViewUI->GetHeightmapInstance(m_SceneHeightmap.get());
		const alm::gfx::Heightmap* heightmap = m_SceneHeightmap->GetHeightmap().get();

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

		ImGui::Spacing();

		auto lodFactor = heightmapInstace->GetLODDistanceFactor();
		if(ImGui::InputFloat("LOD factor##Heightmap", &lodFactor, 0.f, 0.f, "%.2f", ImGuiInputTextFlags_None))
			heightmapInstace->SetLODDistanceFactor(lodFactor);

		ImGui::Spacing();

		// Depth level
		{
			float itemWidth = availWidth / 4;

			int maxDephLevel = heightmapInstace->GetMaxDepthLevel();
			ImGui::SetNextItemWidth(itemWidth);
			if (ImGui::InputInt("Max depth level", &maxDephLevel))
			{
				if (heightmap->InfiniteDepthLevel() || m_ForceSetMaxDepth)
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
			ImGui::SetNextItemWidth(itemWidth);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + itemWidth / 2.f);
			ImGui::Checkbox("Force set", &m_ForceSetMaxDepth);

			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
			if (heightmap->InfiniteDepthLevel())
			{
				ImGui::Text("Max: infinite");
			}
			else
			{
				ImGui::Text("Max: %d", heightmap->GetMaxDepthLevel());
			}
			ImGui::PopStyleColor();
		}

		ImGui::Spacing();

		ImGui::End();
	}
}

void OutdoorsUI::BuildHeightmapMeniItem()
{
	if (ImGui::MenuItem("Settings", nullptr, m_ShowHeightmapSettings, m_SceneHeightmap != nullptr))
		m_ShowHeightmapSettings = !m_ShowHeightmapSettings;

	if (ImGui::MenuItem("View source data", nullptr, false, m_SceneHeightmap != nullptr))
	{
		alm::rhi::TextureHandle texture = m_SceneHeightmap->GetHeightmap()->GetHeightsTexture();
		AddTextureWindow(texture->GetDebugName(), texture);		
	}
}