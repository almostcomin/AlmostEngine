#pragma once

#include "RHI/Resource.h"

namespace alm::rhi
{

// Indirect command signature: encapsulates ONE draw/dispatch command executed by ExecuteIndirect.
// Only the portable subset: one command per signature and no root argument changes
// (D3D12 inline constants / VB-IB views have no equivalent in Vulkan).
//
//   Draw         -> D3D12_DRAW_ARGUMENTS          / VkDrawIndirectCommand            (16 bytes)
//   DrawIndexed  -> D3D12_DRAW_INDEXED_ARGUMENTS  / VkDrawIndexedIndirectCommand     (20 bytes)
//   Dispatch     -> D3D12_DISPATCH_ARGUMENTS      / VkDispatchIndirectCommand        (12 bytes)
//   DispatchMesh -> D3D12_DISPATCH_MESH_ARGUMENTS / VkDrawMeshTasksIndirectCommand   (16 bytes)
struct CommandSignatureDesc
{
	enum class CommandType : uint8_t
	{
		Draw = 0,
		DrawIndexed,
		Dispatch,
		DispatchMesh
	};

	CommandType commandType = CommandType::Draw;

	static uint32_t GetStride(CommandType type)
	{
		switch (type)
		{
		case CommandSignatureDesc::CommandType::Draw:         return 16; // D3D12_DRAW_ARGUMENTS / VkDrawIndirectCommand
		case CommandSignatureDesc::CommandType::DrawIndexed:  return 20; // D3D12_DRAW_INDEXED_ARGUMENTS
		case CommandSignatureDesc::CommandType::Dispatch:     return 12; // D3D12_DISPATCH_ARGUMENTS
		case CommandSignatureDesc::CommandType::DispatchMesh: return 16; // D3D12_DISPATCH_MESH_ARGUMENTS
		}
		return 0;
	}
};

class ICommandSignature : public IResource
{
public:

	virtual const CommandSignatureDesc& GetDesc() const = 0;
	virtual uint32_t GetStride() const = 0;

    ResourceType GetResourceType() const override { return ResourceType::CommandSignature; }

protected:

    ICommandSignature(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

} // namespace alm::rhi