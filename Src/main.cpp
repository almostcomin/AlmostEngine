#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include "Core/Core.h"
#include "Core/Log.h"
#include "Graphics/DeviceManager.h"
#include "Graphics/ShaderFactory.h"
#include "Graphics/Frame.h"
#include "StructureUI.h"
#include <thread>

int SDL_main(int argc, char* argv[]) 
{
	// Your SDL application code goes here
	// For example, initializing SDL and creating a window
	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) 
	{
		st::log::Fatal("Error initializing SDL.");
		return -1; // Initialization failed
	}

	// Create a window
	SDL_Window* window = SDL_CreateWindow("Structure", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (!window) 
	{
		SDL_Quit();
		return -1; // Window creation failed
	}

	// Init device manager
	std::unique_ptr<st::gfx::DeviceManager> deviceManager{ st::gfx::DeviceManager::Create(st::gfx::GraphicsAPI::D3D12) };
	st::gfx::DeviceManager::DeviceParams initParams{
		.WindowHandle = window,
		.DebugRuntime = true,
		.NvrhiValidationLayer = true,
		.VSyncEnabled = false
	};
	deviceManager->Init(initParams);

	// Create shader factory
	std::unique_ptr<st::gfx::ShaderFactory> shaderFactory{ new st::gfx::ShaderFactory(deviceManager->GetDevice()) };

	// Create UI render pass
	std::unique_ptr<StructureUI> uiRenderPass{ new StructureUI };
	uiRenderPass->Init(window, deviceManager.get(), shaderFactory.get());

	// Create frame
	std::unique_ptr<st::gfx::Frame> frame{ new st::gfx::Frame };
	frame->Init(deviceManager.get(), { uiRenderPass.get() });

	// Main loop
	bool running = true;
	uint32_t nFrames = 0;
	auto lastTime = std::chrono::steady_clock::now();
	while (running) 
	{
		SDL_Event event;
		while (SDL_PollEvent(&event)) 
		{
			switch (event.type)
			{
			case SDL_EVENT_MOUSE_MOTION:
				uiRenderPass->OnMouseMove(event.motion.x, event.motion.y);
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				bool validButton = true;
				st::ui::MouseButton button;
				switch (event.button.button)
				{
				case SDL_BUTTON_LEFT: button = st::ui::MouseButton::LEFT; break;
				case SDL_BUTTON_MIDDLE: button = st::ui::MouseButton::MIDDLE; break;
				case SDL_BUTTON_RIGHT: button = st::ui::MouseButton::RIGHT; break;
				default: validButton = false;
				}
				if (validButton)
				{
					uiRenderPass->OnMouseButtonUpdate(button, event.button.down ? st::ui::KeyAction::PRESS : st::ui::KeyAction::RELEASE);
				}				
				break;
			}
			case SDL_EVENT_QUIT:
				running = false;
				break;
			}
		}

		nFrames++;
		auto currentTime = std::chrono::steady_clock::now();
		std::chrono::duration<float> elapsed = currentTime - lastTime;
		if (elapsed.count() > 1.f)
		{
			uiRenderPass->m_Data.FPS = nFrames / elapsed.count();
			nFrames = 0;
			lastTime = currentTime;
		}

		if (deviceManager->UpdateWindowSize())
		{
			frame->OnBackbufferResize(deviceManager->GetWindowDimensions());
		}

		if (deviceManager->IsWindowVisible())
		{
			deviceManager->BeginFrame();
			frame->Render(deviceManager->GetCurrentFrameBuffer());
			bool presentOk = deviceManager->Present();
			assert(presentOk);
		}

		std::this_thread::yield();
		deviceManager->GetDevice()->runGarbageCollection();
	}

	// Clean up

	deviceManager->Shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0; // Exit successfully
}