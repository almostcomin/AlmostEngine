#include "Gfx/ImGUIViewportsRenderer.h"
#include <imgui/imgui.h>
#include <SDL3/SDL_video.h>
#include "Gfx/DeviceManager.h"
#include "Gfx/RenderStages/ImGuiViewportRenderStage.h"
#include "Gfx/RenderView.h"

namespace
{

struct ImGuiImplData
{
	st::gfx::DeviceManager* DeviceManager;
	std::shared_ptr<st::gfx::ImGuiRenderStage> MainRenderStage;
};

struct ImGuiImplViewportData
{
	st::gfx::ViewportSwapChainId ViewportId;
	st::unique<st::gfx::RenderView> RenderView;
};

ImGuiImplData* GetImplData()
{
	return ImGui::GetCurrentContext() ? (ImGuiImplData*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

void ImGui_Impl_CreateWindow(ImGuiViewport* viewport)
{
	ImGuiImplData* bd = GetImplData();

	SDL_Window* window = SDL_GetWindowFromID((SDL_WindowID)(uintptr_t)viewport->PlatformHandle);
	assert(window != nullptr);
	
	st::gfx::ViewportSwapChainId viewportId = bd->DeviceManager->CreateViewportSwapChain({ window }, SDL_GetWindowTitle(window));
	
	auto renderStage = std::make_shared<st::gfx::ImGuiViewportRenderStage>(bd->MainRenderStage.get(), viewport);

	auto renderView = st::make_unique_with_weak<st::gfx::RenderView>(viewportId, bd->DeviceManager, "ImGui Viewport");
	renderView->SetRenderStages({ renderStage });
	renderView->SetRenderMode("Default", { renderStage.get() });

	viewport->RendererUserData = new ImGuiImplViewportData{ 
		.ViewportId = viewportId,
		.RenderView = std::move(renderView)
	};
}

void ImGui_Impl_DestroyWindow(ImGuiViewport* viewport)
{
	ImGuiImplData* bd = GetImplData();
	ImGuiImplViewportData* viewportData = (ImGuiImplViewportData*)viewport->RendererUserData;
	if (viewportData)
	{
		bd->DeviceManager->DestroyViewportSwapChain(viewportData->ViewportId);
		delete viewportData;
		viewport->RendererUserData = nullptr;
	}
}

void ImGui_ImplDX12_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	ImGuiImplData* bd = GetImplData();
	ImGuiImplViewportData* viewportData = (ImGuiImplViewportData*)viewport->RendererUserData;

	bd->DeviceManager->ResizeViewportSwapChain(viewportData->ViewportId);
}

void ImGui_Impl_RenderWindow(ImGuiViewport* viewport, void*)
{
	ImGuiImplData* bd = GetImplData();
	ImGuiImplViewportData* viewportData = (ImGuiImplViewportData*)viewport->RendererUserData;
	st::gfx::RenderView* renderView = viewportData->RenderView.get();

	renderView->Render(ImGui::GetIO().DeltaTime);
}

void ImGui_Impl_SwapBuffers(ImGuiViewport*, void*)
{
	// no-op
}

} // anonymous namespace

void st::gfx::InitImGuiViewportsRenderer(std::shared_ptr<st::gfx::ImGuiRenderStage> mainRenderStage, st::gfx::DeviceManager* deviceManager)
{
	ImGuiImplData* implData = new ImGuiImplData;
	implData->MainRenderStage = mainRenderStage;
	implData->DeviceManager = deviceManager;

	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererUserData = implData;
	io.BackendRendererName = "imgui_impl_dx12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // We can honor ImGuiPlatformIO::Textures[] requests during render.
	io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

	ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
	platformIO.Renderer_CreateWindow = ImGui_Impl_CreateWindow;			// Creates the swap chain for the new window
	platformIO.Renderer_DestroyWindow = ImGui_Impl_DestroyWindow;		// Destroys the swap chain
	platformIO.Renderer_SetWindowSize = ImGui_ImplDX12_SetWindowSize;	// Resize the swap chain
	platformIO.Renderer_RenderWindow = ImGui_Impl_RenderWindow;			// Render the viewport
	platformIO.Renderer_SwapBuffers = ImGui_Impl_SwapBuffers;			// Present
}

void st::gfx::ReleaseImGuiViewportsRenderer()
{
	ImGuiImplData* bd = GetImplData();
	delete bd;
	ImGui::GetIO().BackendRendererUserData = nullptr;
}