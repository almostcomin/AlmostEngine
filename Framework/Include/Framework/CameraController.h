#pragma once

#include <SDL3/SDL_events.h>

namespace alm::gfx
{
	class Camera;
}

namespace alm
{

class CameraController
{
public:

	CameraController() = default;
	~CameraController();

	void SetWindow(SDL_Window* window) { m_Window = window; }
	void SetCamera(std::shared_ptr<alm::gfx::Camera> camera) { m_Camera = camera; }

	void SetSpeed(float v) { m_Speed = v; }

	void Update(float deltaTime);

	// Returns true if the event has been processed
	bool OnSDLEvent(const SDL_Event& event);

private:

	bool m_MouseMiddlePressed = false;
	float m_Speed = 1.f;
	float2 m_CurrentSpeed{ 0.f };

	SDL_Window* m_Window = nullptr;
	std::shared_ptr<alm::gfx::Camera> m_Camera;
};

} // namespace alm