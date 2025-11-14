#pragma once

#include "RenderAPI/Format.h"
#include <string>
#include <vector>

namespace st::rapi
{

enum class InputClassification : uint8_t
{
	PerVertexData,
	PerInstanceData
};

struct InputLayout
{
	static constexpr uint32_t APPEND_ALIGNED_ATTRIBUTE = ~0u; // automatically figure out AlignedByteOffset depending on Format

	struct AttributeDesc
	{
		std::string semanticName;
		uint32_t semanticIndex = 0;
		Format format = Format::UNKNOWN;
		uint32_t inputSlot = 0;
		uint32_t alignedByteOffset = APPEND_ALIGNED_ATTRIBUTE;
		InputClassification inputSlotClassfication = InputClassification::PerVertexData;
	};

	std::vector<AttributeDesc> Attributes;
};

}