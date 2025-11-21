#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include "Core/Core.h"
#include "Core/Log.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Gfx/GltfImporter.h"
#include "Gfx/DataUploader.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/Camera.h"
#include "Gfx/ForwardRenderPass.h"
#include "StructureUI.h"
#include <thread>
#include <sstream>

namespace
{
void PrintSceneGraph(const st::weak<st::gfx::SceneGraphNode>& root)
{
    st::gfx::SceneGraph::Walker walker(root);
    int depth = 0;
    while(walker)
    {
        std::stringstream ss;

        for (int i = 0; i < depth; i++)
            ss << "   ";

        if (walker->GetName().empty())
            ss << "<Unnamed>";
        else
            ss << walker->GetName();

        if (walker->HasBounds())
        {
            const auto& bbox = walker->GetBounds();
            ss << " [" << bbox.min.x << ", " << bbox.min.y << ", " << bbox.min.z << " .. "
                << bbox.max.x << ", " << bbox.max.y << ", " << bbox.max.z << "]";
        }

        if (walker->GetLeaf())
        {
            ss << " : LEAF ";
        }

		if (!ss.str().empty())
			st::log::Info("{}", ss.str());

        depth += walker.Next();
    }
}

} // anonymous namespace

int SDL_main(int argc, char* argv[]) 
{
	// Your SDL application code goes here
	// For example, initializing SDL and creating a window
	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) 
	{
		LOG_FATAL("Error initializing SDL.");
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
		.VSyncEnabled = false
	};
	deviceManager->Init(initParams);

	// Create forward render pass
	std::shared_ptr<st::gfx::ForwardRenderPass> fwdRenderPass{ new st::gfx::ForwardRenderPass };

	st::unique<st::gfx::SceneGraph> sceneGraph;

	// Create UI render pass
	std::shared_ptr<StructureUI> uiRenderPass{ new StructureUI{window} };
	uiRenderPass->m_RequestLoadFile = [&deviceManager, &sceneGraph, &fwdRenderPass](const char* filename) {
		auto importResult = st::gfx::ImportGlTF(filename, deviceManager.get());
		if (importResult)
		{
			sceneGraph = std::move(*importResult);
			fwdRenderPass->SetSceneGraph(sceneGraph.get_weak());
			PrintSceneGraph(sceneGraph->GetRoot());
		}
		else
		{
			LOG_ERROR("Error importing file '{}': {}", filename, importResult.error());
		}
	};

	// Create camera
	auto camera = std::make_shared<st::gfx::Camera>();
	camera->SetPosition({ 0.f, 0.f, -100.f });

	// Create RenderView
	auto renderView = st::make_unique_with_weak<st::gfx::RenderView>(deviceManager.get());
	renderView->SetRenderPasses({ fwdRenderPass, uiRenderPass });
	renderView->SetCamera(camera);

	// Main loop
	bool running = true;
	uint32_t nFrames = 0;
	auto lastTime = std::chrono::steady_clock::now();
	while (running) 
	{
		// Input
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

		// Scene graph update
		if (sceneGraph)
			sceneGraph->Refresh();

		// Update FPS counter
		{
			auto currentTime = std::chrono::steady_clock::now();
			std::chrono::duration<float> elapsed = currentTime - lastTime;
			if (elapsed.count() > 1.f)
			{
				uiRenderPass->m_Data.FPS = nFrames / elapsed.count();
				nFrames = 0;
				lastTime = currentTime;
			}
		}

		deviceManager->Render([&renderView]()
		{
			renderView->Render();
		});

		std::this_thread::yield();
		nFrames++;
	}

	// Clean up

	deviceManager->Shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0; // Exit successfully
}