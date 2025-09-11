#include "Graphics/DeviceManager.h"
#include "Graphics/Backend/dx12/DeviceManager.h"
#include "Core/Log.h"
#include <SDL3/SDL_video.h>

st::gfx::DeviceManager* st::gfx::DeviceManager::Create(st::gfx::GraphicsAPI api)
{
	switch(api)
	{
	case st::gfx::GraphicsAPI::D3D12:
		return new st::gfx::dx12::DeviceManager;
		break;
	//default:
		// TODO: assert
	}

	return nullptr;
}

void st::gfx::DeviceManager::Shutdown()
{
	m_SwapChainFramebuffers.clear();
}

nvrhi::IFramebuffer* st::gfx::DeviceManager::GetCurrentFrameBuffer()
{
	return m_SwapChainFramebuffers[GetCurrentBackBufferIndex()];
}

bool st::gfx::DeviceManager::UpdateWindowSize()
{
	int width, height;
	if (SDL_GetWindowSize(m_DeviceParams.WindowHandle, (int*)&width, (int*)&height) == false)
	{
		st::log::Error("SDL_GetWindowSize failed");
		return false;
	}

	if (width == 0 || height == 0)
	{
		// window is minimized
		m_WindowVisible = false;
		return false;
	}
	m_WindowVisible = true;

	if (int(m_BackBufferWidth) != width || int(m_BackBufferHeight) != height)
	{
		m_SwapChainFramebuffers.clear();

		m_BackBufferWidth = width;
		m_BackBufferHeight = height;

		ResizeSwapChain();

		uint32_t backBufferCount = m_DeviceParams.SwapChainBufferCount;
		m_SwapChainFramebuffers.resize(backBufferCount);
		for (uint32_t index = 0; index < backBufferCount; index++)
		{
			m_SwapChainFramebuffers[index] = m_nvrhiDevice->createFramebuffer(
				nvrhi::FramebufferDesc().addColorAttachment(GetBackBuffer(index)));
		}

		return true;
	}

	return false;
}