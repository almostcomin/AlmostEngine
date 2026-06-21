#pragma once

#include "Core/Common.h"
#include "Gfx/RenderStageType.h"
#include "Gfx/Transform.h"
#include "Gfx/RenderGraphTypes.h"
#include <SDL3/SDL_events.h>

struct SDL_Window;

namespace alm::gfx
{
    class DeviceManager;
    class Scene;
    class RenderView;
    class Camera;
    class ImGuiRenderStage;
    class SceneGraphNode;
}

namespace alm::fw
{
    class FrameworkUI;
}

namespace alm::fw
{

class App : public alm::noncopyable_nonmovable
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

    App(const std::string& name, RenderStageSetMode renderStageSetMode);
    virtual ~App();

    void Run(const AppArgs& args);

    bool GetStartupArgBool(const std::string& key, bool defaultValue = false);
    std::string GetStartupArgString(const std::string& key, const std::string& defaultValue = {});

    void RefreshUIData();

    void ShowNormal(const uint2& screenPos);
    void HideNormal();

protected:

    virtual bool Initialize() { return true; }
    virtual bool Update(float deltaTime) { return true; }
    virtual void Render() {}    
    virtual void Shutdown() {}
    
    virtual void OnSDLEvent(const SDL_Event& event) {};
    virtual std::shared_ptr<alm::gfx::ImGuiRenderStage> UserInitRenderStages() { return nullptr; };

    virtual gfx::RenderStageTypeID GetUIRenderStageType() const; // default FrameworkUI

    void ShowArrow(const gfx::Transform& transform);
    void HideArrow();

protected:

    std::unique_ptr<alm::gfx::DeviceManager> m_DeviceManager;
    alm::unique<alm::gfx::Scene> m_Scene;
    alm::unique<alm::gfx::RenderView> m_MainRenderView;
    std::shared_ptr<alm::gfx::Camera> m_MainCamera;

    std::shared_ptr<alm::gfx::ImGuiRenderStage> m_ImGuiRS;
    alm::fw::FrameworkUI* m_FrameworkUI;

    SDL_Window* m_Window;
    std::string m_Name;

    RenderStageSetMode m_RenderStageSetMode;

    float m_CPUTime;
    float m_GPUTime;
    float m_FPS;

    AppArgs m_StartupArgs;

    alm::weak<alm::gfx::SceneGraphNode> m_ArrowMeshY;
    
    alm::gfx::RGBufferViewTicket m_GBuffer2ViewTicket;
    alm::gfx::RGBufferViewTicket m_LinearDepthViewTicket;

private:

    bool InitInternal();
    void ShutdownInternal();
    void InitRenderStages();

    void MainLoop();
};

} // namespace alm

// To be implemented by final app
std::unique_ptr<alm::fw::App> CreateApp();