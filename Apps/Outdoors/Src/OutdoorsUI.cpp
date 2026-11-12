#include "Framework/FrameworkPCH.h"
#include "OutdoorsUI.h"
#include "HeightmapSettingsWindow.h"
#include "Gfx/SceneHeightmap.h"
#include "Gfx/Heightmap.h"
#include "Gfx/DeviceManager.h"

OutdoorsUI::OutdoorsUI()
{
	// App-specific window, drawn after the framework windows. It opens from the
	// "Heightmap" main menu item instead of the View menu.
	m_HeightmapWindow = static_cast<HeightmapSettingsWindow*>(
		RegisterWindow(std::make_unique<HeightmapSettingsWindow>(*this)));
}

void OutdoorsUI::Init(SDL_Window* window, alm::weak<alm::gfx::Scene> scene, alm::weak<alm::gfx::RenderView> renderView,
	alm::fw::CameraController* cameraController)
{
	FrameworkUI::Init(window, scene, renderView, cameraController);
	
	RegisterMainMenuItem("Heightmap", [this]() { BuildHeightmapMenuItem(); });
}

void OutdoorsUI::SetHeightmap(alm::weak<alm::gfx::SceneHeightmap> sceneHeightmap)
{
	m_SceneHeightmap = sceneHeightmap;

	if (m_HeightmapWindow)
		m_HeightmapWindow->SetHeightmap(sceneHeightmap);
}

void OutdoorsUI::BuildHeightmapMenuItem()
{
	const bool hasHeightmap = m_SceneHeightmap != nullptr;
	const bool showWindow = m_HeightmapWindow && m_HeightmapWindow->IsOpen();

	if (ImGui::MenuItem("Settings", nullptr, showWindow, hasHeightmap))
	{
		if (m_HeightmapWindow)
			m_HeightmapWindow->Toggle();
	}

	if (ImGui::MenuItem("View source data", nullptr, false, hasHeightmap))
	{
		alm::rhi::TextureHandle texture = m_SceneHeightmap->GetHeightmap()->GetHeightsTexture();
		AddTextureWindow(texture);
	}
}
