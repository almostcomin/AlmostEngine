#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include "Core/Core.h"
#include "Core/Log.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Gfx/GltfImporter.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/Camera.h"
#include "Gfx/RenderStages/OpaqueRenderStage.h"
#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "StructureUI.h"
#include <thread>
#include <sstream>
#include <iomanip>

namespace
{
void PrintSceneGraph(const st::weak<st::gfx::SceneGraphNode>& root)
{
    st::gfx::SceneGraph::Walker walker(root);
    int depth = 0;
    while(walker)
    {
        std::stringstream ss;
		ss << std::fixed << std::setprecision(2);

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
		.GPUValidation = true,
		.VSyncEnabled = false
	};
	deviceManager->Init(initParams);

	// Our scene
	st::unique<st::gfx::Scene> scene;

	// Create opaque render stage
	std::shared_ptr<st::gfx::OpaqueRenderStage> opaqueRS{ new st::gfx::OpaqueRenderStage };

	// Create composite render stage
	std::shared_ptr<st::gfx::CompositeRenderStage> compositeRS{ new st::gfx::CompositeRenderStage };

	// Create UI render stage
	std::shared_ptr<StructureUI> uiRS{ new StructureUI{window} };
	uiRS->m_RequestLoadFile = [&deviceManager, &scene, &opaqueRS](const char* filename) {
		auto importResult = st::gfx::ImportGlTF(filename, deviceManager.get());
		if (importResult)
		{
			scene.reset(new st::gfx::Scene{ deviceManager.get() });
			scene->SetSceneGraph(std::move(*importResult));

			opaqueRS->SetScene(scene.get_weak());
			PrintSceneGraph(scene->GetSceneGraph()->GetRoot());
		}
		else
		{
			LOG_ERROR("Error importing file '{}': {}", filename, importResult.error());
		}
	};

	// Create camera
	auto camera = std::make_shared<st::gfx::Camera>();
	camera->SetPosition({ 0.f, 0.f, -5.f });

	// Create RenderView
	auto renderView = st::make_unique_with_weak<st::gfx::RenderView>(deviceManager.get(), "Main view");
	renderView->SetRenderStages({ opaqueRS, compositeRS, uiRS });
	renderView->SetCamera(camera);

	// Main loop
	bool running = true;
	auto lastTime = std::chrono::steady_clock::now();
	auto fpsLastTime = lastTime;
	uint32_t fpsFrameCount = 0;
	float2 cameraSpeed{ 0.f };
	while (running) 
	{
		const auto currentTime = std::chrono::steady_clock::now();
		const std::chrono::duration<float> elapsed = currentTime - lastTime;
		const float elapsedMs = elapsed.count() * 1000.0f;

		// Input
		SDL_Event event;
		while (SDL_PollEvent(&event)) 
		{
			switch (event.type)
			{
			case SDL_EVENT_MOUSE_MOTION:
				uiRS->OnMouseMove(event.motion.x, event.motion.y);
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				bool validButton = true;
				st::gfx::MouseButton button;
				switch (event.button.button)
				{
				case SDL_BUTTON_LEFT: button = st::gfx::MouseButton::LEFT; break;
				case SDL_BUTTON_MIDDLE: button = st::gfx::MouseButton::MIDDLE; break;
				case SDL_BUTTON_RIGHT: button = st::gfx::MouseButton::RIGHT; break;
				default: validButton = false;
				}
				if (validButton)
				{
					uiRS->OnMouseButtonUpdate(button, event.button.down ? st::gfx::KeyAction::PRESS : st::gfx::KeyAction::RELEASE);
				}				
				break;
			}
			case SDL_EVENT_KEY_DOWN:
				switch (event.key.key)
				{
				case SDLK_W: cameraSpeed.y = -1.f; break;
				case SDLK_S: cameraSpeed.y = 1.f; break;
				}
				break;
			case SDL_EVENT_KEY_UP:
				switch (event.key.key)
				{
				case SDLK_W: cameraSpeed.y = 0.f; break;
				case SDLK_S: cameraSpeed.y = 0.f; break;
				}
				break;

			case SDL_EVENT_QUIT:
				running = false;
				break;
			}
		}

		// Scene graph update
		if (scene)
		{
			float3 camFwd = camera->GetForward();
			float3 camPos = camera->GetPosition();
			float3 newPos = camPos + camFwd * cameraSpeed.y * elapsedMs;
			camera->SetPosition(newPos);
			scene->Update();
		}

		// Update FPS counter
		{
			std::chrono::duration<float> fpsElapsed = currentTime - fpsLastTime;
			if (fpsElapsed.count() > 1.f)
			{
				uiRS->m_Data.FPS = fpsFrameCount / fpsElapsed.count();
				fpsFrameCount = 0;
				fpsLastTime = currentTime;
			}
		}

		deviceManager->Render([&renderView]()
		{
			renderView->Render();
		});

		fpsFrameCount++;
		lastTime = currentTime;
		std::this_thread::yield();
	}

	// Clean up
	camera.reset();
	renderView.reset();
	uiRS.reset();
	compositeRS.reset();
	opaqueRS.reset();
	scene.reset();

	deviceManager->Shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0; // Exit successfully
}