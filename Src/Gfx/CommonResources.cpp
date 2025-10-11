#include "Gfx/CommonResources.h"
#include "Gfx/ShaderFactory.h"

st::gfx::CommonResources::CommonResources(st::gfx::ShaderFactory* shaderFactory) :
	m_ShaderFactory(shaderFactory)
{
	m_FullscreenVS = m_ShaderFactory->CreateShader("Shaders/fullscreen_vs.vso", nvrhi::ShaderType::Vertex);
}

st::gfx::CommonResources::~CommonResources()
{}

