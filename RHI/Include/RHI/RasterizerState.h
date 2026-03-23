#pragma once

#include <cstdint>

namespace alm::rhi
{

enum class FillMode : uint8_t
{
	Solid,
	Wireframe
};

enum class CullMode : uint8_t
{
	Back,
	Front,
	None,

	_Size
};

struct RasterizerState
{
	FillMode fillMode = FillMode::Solid;
	CullMode cullMode = CullMode::None;
	bool frontCounterClockwise = true;
	bool depthClipEnable = false;
	bool scissorEnable = false;
	bool multisampleEnable = false;
	bool antialiasedLineEnable = false;
	int32_t depthBias = 0;
	float depthBiasClamp = 0.f;
	float slopeScaledDepthBias = 0.f;

	bool conservativeRasterEnable = false;
	uint32_t forcedSampleCount = 0;
};

} // namespace st::rhi