#pragma once

#include <SDL3/SDL_events.h>

namespace alm::gfx
{
	class Camera;
}

namespace alm::fw
{

class CameraController
{
public:

	CameraController() = default;
	~CameraController();

	void SetWindow(SDL_Window* window) { m_Window = window; }
	void SetCamera(std::shared_ptr<alm::gfx::Camera> camera) { m_Camera = camera; }

	void SetSpeed(float v) { m_Speed = v; }
	float GetSpeed() const { return m_Speed; }

	// Disabling stops the camera and ignores input until re-enabled (e.g. while a gizmo is being used)
	void SetEnabled(bool b) { m_Enabled = b; if (!b) Stop(); }
	bool IsEnabled() const { return m_Enabled; }

	void Stop() { m_CurrentSpeed = float2{ 0.f, 0.f }; }

	void Update(float deltaTime);

	// Returns true if the event has been processed
	bool OnSDLEvent(const SDL_Event& event);

private:

	bool m_MouseMiddlePressed = false;
	bool m_Enabled = true;
	float m_Speed = 1.f;
	float2 m_CurrentSpeed{ 0.f };

	SDL_Window* m_Window = nullptr;
	std::shared_ptr<alm::gfx::Camera> m_Camera;
};

} // namespace alm::fw