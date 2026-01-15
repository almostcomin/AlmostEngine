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
#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderStages/DepthPrepassRenderStage.h"
#include "Gfx/RenderStages/DeferredBaseRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
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
            const auto& bbox = walker->GetWorldBounds();
            ss << " [" << bbox.min.x << ", " << bbox.min.y << ", " << bbox.min.z << " .. "
                << bbox.max.x << ", " << bbox.max.y << ", " << bbox.max.z << "]";
        }

        if (walker->GetLeaf())
        {
			switch (walker->GetLeaf()->GetType())
			{
			case st::gfx::SceneGraphLeaf::Type::MeshInstance:
				ss << " : MESH_INSTANCE ";
				break;
			case st::gfx::SceneGraphLeaf::Type::Camera:
				ss << " : CAMERA ";
				break;
			default:
				ss << " : UNKNOWN_LEAF ";
			}
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
	SDL_Window* window = SDL_CreateWindow("Structure", 1920, 1080, SDL_WINDOW_RESIZABLE);
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

	// Init ImGui
	{
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(1.25f);			// Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
		style.FontScaleDpi = 1.25f;			// Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
		io.ConfigDpiScaleFonts = true;      // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
		io.ConfigDpiScaleViewports = true;  // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
	}

	// Our scene
	st::unique<st::gfx::Scene> scene;

	// Create RenderView
	auto renderView = st::make_unique_with_weak<st::gfx::RenderView>(deviceManager.get(), "Main view");

	// Create shadowmap render stage
	std::shared_ptr<st::gfx::ShadowmapRenderStage> shadowmapRS{ new st::gfx::ShadowmapRenderStage{ 1024, 4, st::rhi::Format::D32 }};

	// Create depth prepass render stage
	std::shared_ptr<st::gfx::DepthPrepassRenderStage> depthPrepassRS{ new st::gfx::DepthPrepassRenderStage };

	// Create deferred render stage
	std::shared_ptr<st::gfx::DeferredBaseRenderStage> deferredRS{ new st::gfx::DeferredBaseRenderStage };

	// Create lighting render stage
	std::shared_ptr<st::gfx::DeferredLightingRenderStage> lightingRS{ new st::gfx::DeferredLightingRenderStage };
	
	// Create opaque render stage
	std::shared_ptr<st::gfx::OpaqueRenderStage> opaqueRS{ new st::gfx::OpaqueRenderStage };

	// Create debug render stage
	std::shared_ptr<st::gfx::DebugRenderStage> debugRS{ new st::gfx::DebugRenderStage };

	// Create composite render stage
	std::shared_ptr<st::gfx::CompositeRenderStage> compositeRS{ new st::gfx::CompositeRenderStage };

	// Create UI render stage
	std::string requestLoadFile;
	bool requestClose = false;
	bool requestQuit = false;
	std::shared_ptr<StructureUI> uiRS{ new StructureUI{ renderView.get_weak(), window, deviceManager.get() }};
	uiRS->m_RequestLoadFile = [&requestLoadFile](const char* filename) { requestLoadFile = filename; };
	uiRS->m_RequestClose = [&requestClose] { requestClose = true; };
	uiRS->m_RequestQuit = [&requestQuit] { requestQuit = true; };

	// Create camera
	auto camera = std::make_shared<st::gfx::Camera>();
	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, (int*)&windowWidth, (int*)&windowHeight);
	camera->SetAspect((float)windowWidth / windowHeight);
	camera->SetPosition({ 0.f, 0.f, 5.f });

	// Add stages to render view
	renderView->SetRenderStages({ shadowmapRS, depthPrepassRS, deferredRS, lightingRS/*opaqueRS*/, debugRS, compositeRS, uiRS});
	renderView->SetCamera(camera);

	// Main loop

	auto lastTime = std::chrono::steady_clock::now();
	auto fpsLastTime = lastTime;
	uint32_t fpsFrameCount = 0;
	float2 cameraSpeed{ 0.f };
	bool mouseMiddlePressed = false;
	while (!requestQuit)
	{
		const auto currentTime = std::chrono::steady_clock::now();
		const std::chrono::duration<float> elapsed = currentTime - lastTime;
		const float elapsedMs = elapsed.count() * 1000.0f;
		uiRS->m_Data.CPUTime = elapsedMs;
		uiRS->m_Data.GPUTime = (std::max)(deviceManager->GetGPUFrameTime(), 0.f);

		if (!requestLoadFile.empty())
		{
			auto importResult = st::gfx::ImportGlTF(requestLoadFile.c_str(), deviceManager.get());
			if (importResult)
			{
				scene.reset(new st::gfx::Scene{ deviceManager.get() });
				scene->SetSceneGraph(std::move(*importResult));

				renderView->SetScene(scene.get_weak());
				PrintSceneGraph(scene->GetSceneGraph()->GetRoot());
			}
			else
			{
				LOG_ERROR("Error importing file '{}': {}", requestLoadFile, importResult.error());
			}
			requestLoadFile.clear();
		}
		if (requestClose)
		{
			if (scene)
			{
				scene.reset();
			}
			requestClose = false;
		}

		// Input
		SDL_Event event;
		while (SDL_PollEvent(&event)) 
		{
			switch (event.type)
			{
			case SDL_EVENT_MOUSE_MOTION:
				uiRS->OnMouseMove(event.motion.x, event.motion.y);
				if (mouseMiddlePressed)
				{
					int windowWidth, windowHeight;
					SDL_GetWindowSize(window, (int*)&windowWidth, (int*)&windowHeight);
					{
						float angleRad = event.motion.yrel * PI / windowHeight;
						glm::quat q = glm::angleAxis(-angleRad, camera->GetRight());
						float3 newFwd = q * camera->GetForward();
						camera->SetForward(newFwd);
					}
					{
						float angleRad = event.motion.xrel * PI / windowWidth;
						glm::quat q = glm::angleAxis(-angleRad, camera->GetUp());
						float3 newFwd = q * camera->GetForward();
						camera->SetForward(newFwd);
					}
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				bool validButton = true;
				st::gfx::MouseButton button;
				switch (event.button.button)
				{
				case SDL_BUTTON_LEFT: 
					button = st::gfx::MouseButton::LEFT; 
					break;
				case SDL_BUTTON_MIDDLE: 
					button = st::gfx::MouseButton::MIDDLE;
					mouseMiddlePressed = event.button.down;
					break;
				case SDL_BUTTON_RIGHT: 
					button = st::gfx::MouseButton::RIGHT; 
					break;
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
				case SDLK_W: cameraSpeed.y = 1.f; break;
				case SDLK_S: cameraSpeed.y = -1.f; break;
				case SDLK_A: cameraSpeed.x = 1.f; break;
				case SDLK_D: cameraSpeed.x = -1.f; break;
				}
				break;

			case SDL_EVENT_KEY_UP:
				switch (event.key.key)
				{
				case SDLK_W: cameraSpeed.y = 0.f; break;
				case SDLK_S: cameraSpeed.y = 0.f; break;
				case SDLK_A: cameraSpeed.x = 0.f; break;
				case SDLK_D: cameraSpeed.x = 0.f; break;
				}
				break;

			case SDL_EVENT_QUIT:
				requestQuit = true;
				break;
			}
		}

		// Scene graph update
		if (scene)
		{
			const float3& camFwd = camera->GetForward();
			const float3& camRight = camera->GetRight();

			float3 newPos = camera->GetPosition();
			newPos += camFwd * cameraSpeed.y * elapsedMs * 0.001f;
			newPos += -camRight * cameraSpeed.x * elapsedMs * 0.001f;

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

		if (deviceManager->UpdateWindowSize())
		{
			SDL_GetWindowSize(window, (int*)&windowWidth, (int*)&windowHeight);
			camera->SetAspect((float)windowWidth / windowHeight);

			renderView->OnWindowSizeChanged();
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
	lightingRS.reset();
	deferredRS.reset();
	depthPrepassRS.reset();
	shadowmapRS.reset();
	debugRS.reset();
	scene.reset();

	deviceManager->Shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0; // Exit successfully
}