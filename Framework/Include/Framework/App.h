#pragma once

#include "Core/Common.h"
#include "Gfx/RenderStageType.h"
#include <SDL3/SDL_events.h>

struct SDL_Window;

namespace alm::gfx
{
    class DeviceManager;
    class Scene;
    class RenderView;
    class Camera;
    class ImGuiRenderStage;
}

namespace alm
{

class App //: public alm::noncopyable_nonmovable
{
public:

    using AppArgs = std::unordered_map<std::string, std::string>;

    enum class RenderStageSetMode
    {
        None,
        Default,
        User
    };
    
public:

    App(const std::string& windowTitle, RenderStageSetMode renderStageSetMode);
    virtual ~App();

    void Run(const AppArgs& args);

    bool GetStartupArgBool(const std::string& key, bool defaultValue = false);
    std::string GetStartupArgString(const std::string& key, const std::string& defaultValue = {});

protected:

    virtual bool Initialize() { return true; }
    virtual bool Update(float deltaTime) { return true; }
    virtual void Render() {}    
    virtual void Shutdown() {}
    
    virtual void OnSDLEvent(const SDL_Event& event) {};
    virtual std::shared_ptr<alm::gfx::ImGuiRenderStage> UserInitRenderStages() { return nullptr; };

    virtual gfx::RenderStageTypeID GetUIRenderStageType() const { return gfx::RenderStageType_None; }

protected:

    std::unique_ptr<alm::gfx::DeviceManager> m_DeviceManager;
    alm::unique<alm::gfx::Scene> m_Scene;
    alm::unique<alm::gfx::RenderView> m_MainRenderView;
    std::shared_ptr<alm::gfx::Camera> m_MainCamera;

    std::shared_ptr<alm::gfx::ImGuiRenderStage> m_ImGuiRS;

    SDL_Window* m_Window = nullptr;
    std::string m_WindowTitle;

    RenderStageSetMode m_RenderStageSetMode;

    float m_CPUTime;
    float m_GPUTime;
    float m_FPS;

    AppArgs m_StartupArgs;

private:

    bool InitInternal();
    void ShutdownInternal();
    void InitRenderStages();

    void MainLoop();
};

} // namespace alm

// To be implemented by final app
std::unique_ptr<alm::App> CreateApp();