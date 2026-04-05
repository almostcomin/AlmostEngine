#include "Framework/FrameworkPCH.h"
#include "Framework/CameraController.h"
#include "Gfx/Camera.h"

alm::CameraController::~CameraController() = default;

void alm::CameraController::Update(float deltaTime)
{
	const float3& camFwd = m_Camera->GetForward();
	const float3& camRight = m_Camera->GetRight();

	float3 newPos = m_Camera->GetPosition();
	float2 cameraTurboSpeed = m_CurrentSpeed;
	SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_SHIFT)
		cameraTurboSpeed *= 2.f;
	newPos += camFwd * cameraTurboSpeed.y * deltaTime;
	newPos += -camRight * cameraTurboSpeed.x * deltaTime;

	m_Camera->SetPosition(newPos);
}

bool alm::CameraController::OnSDLEvent(const SDL_Event& event)
{
	if (!m_Camera)
		return false;

	bool eventProcessed = false;
	switch (event.type)
	{
	case SDL_EVENT_MOUSE_MOTION:
	{
		if (m_MouseMiddlePressed)
		{
			int windowWidth, windowHeight;
			SDL_GetWindowSize(m_Window, (int*)&windowWidth, (int*)&windowHeight);
			{
				float angleRad = event.motion.yrel * PI / windowHeight;
				glm::quat q = glm::angleAxis(-angleRad, m_Camera->GetRight());
				float3 newFwd = q * m_Camera->GetForward();
				m_Camera->SetForward(newFwd);
			}
			{
				float angleRad = event.motion.xrel * PI / windowWidth;
				glm::quat q = glm::angleAxis(-angleRad, m_Camera->GetUp());
				float3 newFwd = q * m_Camera->GetForward();
				m_Camera->SetForward(newFwd);
			}
			eventProcessed = true;
		}
	} break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	{
		switch (event.button.button)
		{
		case SDL_BUTTON_LEFT:
			break;
		case SDL_BUTTON_MIDDLE:
			m_MouseMiddlePressed = event.button.down;
			eventProcessed = true;
			break;
		case SDL_BUTTON_RIGHT:
			break;
		}
	} break;

	case SDL_EVENT_KEY_DOWN:
	{
		switch (event.key.key)
		{
		case SDLK_W: m_CurrentSpeed.y = m_Speed; eventProcessed = true; break;
		case SDLK_S: m_CurrentSpeed.y = -m_Speed; eventProcessed = true; break;
		case SDLK_A: m_CurrentSpeed.x = m_Speed; eventProcessed = true; break;
		case SDLK_D: m_CurrentSpeed.x = -m_Speed; eventProcessed = true; break;
		}
	} break;

	case SDL_EVENT_KEY_UP:
	{
		switch (event.key.key)
		{
		case SDLK_W: m_CurrentSpeed.y = 0.f; eventProcessed = true; break;
		case SDLK_S: m_CurrentSpeed.y = 0.f; eventProcessed = true; break;
		case SDLK_A: m_CurrentSpeed.x = 0.f; eventProcessed = true; break;
		case SDLK_D: m_CurrentSpeed.x = 0.f; eventProcessed = true; break;
		}
	} break;
	}

	return eventProcessed;
}