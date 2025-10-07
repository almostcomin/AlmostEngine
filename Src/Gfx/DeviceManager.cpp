#include "Gfx/DeviceManager.h"
#include "Gfx/Backend/dx12/DeviceManager.h"
#include "Core/Log.h"
#include <SDL3/SDL_video.h>
#include "Gfx/DataUploader.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/TextureCache.h"

st::gfx::DeviceManager::~DeviceManager()
{
}

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

bool st::gfx::DeviceManager::Init(const DeviceParams& params)
{
	bool ok = InternalInit(params);
	if (ok)
	{
		m_ShaderFactory = std::make_unique<st::gfx::ShaderFactory>(m_nvrhiDevice);
		m_DataUploader = std::make_unique<st::gfx::DataUploader>(m_nvrhiDevice);
		m_TextureCache = std::make_unique<st::gfx::TextureCache>(this);
	}
	return ok;
}

void st::gfx::DeviceManager::Shutdown()
{
	m_TextureCache.reset();
	m_DataUploader.reset();
	m_ShaderFactory.reset();

	m_SwapChainFramebuffers.clear();

	InternalShutdown();
}

void st::gfx::DeviceManager::Update()
{
	m_TextureCache->Update();
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
		LOG_ERROR("SDL_GetWindowSize failed");
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