#pragma once

#include "RHI/Shader.h"
#include "RHI/PipelineState.h"
#include "RHI/ResourceState.h"

namespace alm::rhi
{
	class Device;
	struct FramebufferInfo;
};

namespace alm::gfx
{
	class Mesh;
	class ShaderFactory;
	class DataUploader;
	class MaterialManager;
	class MaterialRef;
};

namespace alm::gfx
{

enum class CubeNormalDirection : uint8_t
{
	Outward, // normals point away from the cube center (regular solid cube)
	Inward,  // normals point towards the cube center (environment/skybox cubes)
	None     // no normals at all
};

enum class CubeUVMode : uint8_t
{
	None, // no uvs
	UV2D, // per face 2d uvs
	UV3D  // 3d direction uvs (unnormalized corner direction, for cubemap sampling)
};

class CommonResources
{
public:

	CommonResources(ShaderFactory* shaderFactory, MaterialManager* materialManager, rhi::Device* device);
	~CommonResources();

	rhi::GraphicsPipelineStateOwner CreateBlitGraphicsPSO(const rhi::FramebufferInfo& fbInfo);
	rhi::GraphicsPipelineStateOwner CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo, const rhi::ShaderHandle& PS, const std::string debugName);
	rhi::GraphicsPipelineStateOwner CreateFullscreenPassPSO(const rhi::FramebufferInfo& fbInfo, const rhi::ShaderHandle& VS, const rhi::ShaderHandle& PS,
		const std::string debugName);

	rhi::ComputePipelineStateHandle GetBlitComputePSO() { return m_BlitComputePSO.get_weak(); }

	rhi::ComputePipelineStateHandle GetClearBufferPSO() { return m_ClearBufferPSO.get_weak(); }

	rhi::ShaderHandle GetBlitVS() const { return m_BlitVS.get_weak(); }
	rhi::ShaderHandle GetBlitPS() const { return m_BlitPS.get_weak(); }

	// Generates a UV sphere of radius 1 centered at origin
	// stacks: horizontal subdivisions (latitude bands), >= 2
	// slices: vertical subdivisions (longitude segments), >= 3
	// generateCpuGeometry: keeps a copy of the geometry on cpu (needed for raycast)
	std::shared_ptr<alm::gfx::Mesh> CreateUVSphere(float radius, uint32_t stacks, uint32_t slices, bool generateCpuGeometry,
		alm::gfx::DataUploader* dataUploader, const std::string& name, const alm::gfx::MaterialRef* opt_materialRef = nullptr);

	// Generates a cube of edge length `size` centered at origin (6 faces, 24 vertices, 36 indices)
	// normalDirection: outward for a regular solid cube, inward for environment/skybox cubes
	// uvMode: UV2D generates per face 2d uvs, UV3D generates 3d direction uvs (vertex direction, suitable for
	// sampling a cubemap with textureCube(direction)), None omits uvs
	// generateCpuGeometry: keeps a copy of the geometry on cpu (needed for raycast)
	std::shared_ptr<alm::gfx::Mesh> CreateCube(float size, CubeNormalDirection normalDirection, CubeUVMode uvMode, bool generateCpuGeometry,
		alm::gfx::DataUploader* dataUploader, const std::string& name, const alm::gfx::MaterialRef* opt_materialRef = nullptr);

	// Blue noise texture: 64x64 single channel
	//rhi::TextureHandle GetBlueNoise_HDR_64x64_L() const;

	void ClearTexture2D_RGBA(rhi::ICommandList* commandList, rhi::ITexture* texture, const float4& clearValue,
		rhi::ResourceState currentState, rhi::ResourceState finalState);
	void ClearTexture2D_R(rhi::ICommandList* commandList, rhi::ITexture* texture, float clearValue,
		rhi::ResourceState currentState, rhi::ResourceState finalState);

private:

	ShaderFactory* m_ShaderFactory;
	MaterialManager* m_MaterialManager;
	rhi::Device* m_Device;

	rhi::ShaderOwner m_BlitVS;
	rhi::ShaderOwner m_BlitPS;
	rhi::GraphicsPipelineStateDesc m_BlitGraphicsPSODesc;

	rhi::ShaderOwner m_BlitCS;
	rhi::ComputePipelineStateOwner m_BlitComputePSO;

	rhi::ShaderOwner m_ClearBufferCS;
	rhi::ComputePipelineStateOwner m_ClearBufferPSO;

	rhi::ShaderOwner m_ClearTexture_RGBA_CS;
	rhi::ShaderOwner m_ClearTexture_R_CS;
	rhi::ComputePipelineStateOwner m_ClearTexture_RGBA_PSO;
	rhi::ComputePipelineStateOwner m_ClearTexture_R_PSO;
};

} // namespace st::gfx