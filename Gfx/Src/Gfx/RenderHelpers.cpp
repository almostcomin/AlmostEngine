#include "Gfx/RenderHelpers.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "RHI/CommandList.h"

void st::gfx::RenderSet(const std::vector<const st::gfx::MeshInstance*>& instancesSet, st::rhi::BufferUniformView sceneBuffer,
	st::rhi::ICommandList* commandList)
{
	if (instancesSet.empty())
		return;

	interop::SingleInstanceDrawData shaderConstants;
	shaderConstants.sceneDI = sceneBuffer;

	for (const st::gfx::MeshInstance* meshInstance : instancesSet)
	{
		shaderConstants.instanceIdx = meshInstance->GetLeafSceneIndex();
		commandList->PushGraphicsConstants(shaderConstants);

		commandList->Draw(meshInstance->GetMesh()->GetIndexCount());
	}
}

void st::gfx::RenderSetInstanced(const std::vector<std::pair<rhi::CullMode, std::vector<const st::gfx::MeshInstance*>>>& renderSet,
	st::rhi::BufferUniformView sceneBuffer, st::rhi::BufferReadOnlyView renderSetIndicesBuffer, const RenderContext& renderContext,
	st::rhi::ICommandList* commandList)
{
	if (renderSet.empty())
		return;

	interop::MultiInstanceDrawConstants shaderConstants = {};
	shaderConstants.sceneDI = sceneBuffer;
	shaderConstants.instancesDI = renderSetIndicesBuffer;

	for (auto& cullBase : renderSet)
	{
		if (cullBase.second.empty())
			continue;

		rhi::IGraphicsPipelineState* PSO = nullptr;
		switch (cullBase.first)
		{
		case rhi::CullMode::Back:
			PSO = renderContext.PSO_BackCull.get();
			break;
		case rhi::CullMode::Front:
			PSO = renderContext.PSO_FrontCull.get();
			break;
		case rhi::CullMode::None:
			PSO = renderContext.PSO_NoCull.get();
			break;
		}

		if (!PSO)
		{
			LOG_ERROR("Cull mode '{}' not set in RenderContext",
				cullBase.first == rhi::CullMode::Back ? "Back" : cullBase.first == rhi::CullMode::Front ? "Front" : "None");
		}

		if (PSO)
		{
			commandList->SetPipelineState(PSO);
			const auto& instances = cullBase.second;

			st::gfx::Mesh* currentMesh = instances[0]->GetMesh().get();
			int prevIdx = 0;
			for (int i = 0; i < instances.size(); ++i)
			{
				const st::gfx::MeshInstance* meshInstance = instances[i];
				if (currentMesh != meshInstance->GetMesh().get())
				{
					shaderConstants.baseInstanceIdx = prevIdx;
					shaderConstants.meshIndex = instances[prevIdx]->GetMeshSceneIndex();
					shaderConstants.materialIndex = instances[prevIdx]->GetMaterialSceneIndex();
					commandList->PushGraphicsConstants(shaderConstants);
					commandList->DrawInstanced(currentMesh->GetIndexCount(), i - prevIdx, 0);

					currentMesh = meshInstance->GetMesh().get();
					prevIdx = i;
				}
			}

			shaderConstants.baseInstanceIdx = prevIdx;
			shaderConstants.meshIndex = instances[prevIdx]->GetMeshSceneIndex();
			shaderConstants.materialIndex = instances[prevIdx]->GetMaterialSceneIndex();
			commandList->PushGraphicsConstants(shaderConstants);
			commandList->DrawInstanced(currentMesh->GetIndexCount(), instances.size() - prevIdx, 0);
		}
	}
}