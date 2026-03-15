#include "Gfx/RenderHelpers.h"
#include "Interop/RenderResources.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "RHI/CommandList.h"
#include "Gfx/RenderSet.h"

void st::gfx::DrawElements(const std::vector<const st::gfx::MeshInstance*>& instancesSet, st::rhi::BufferUniformView sceneBuffer,
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

void st::gfx::DrawRenderSetInstanced(const RenderSet& renderSet, st::rhi::BufferReadOnlyView renderSetIndicesBuffer,
	st::rhi::BufferUniformView sceneBuffer, const RenderContext& renderContext, st::rhi::ICommandList* commandList)
{
	if (renderSet.Elements.empty())
		return;

	interop::MultiInstanceDrawConstants shaderConstants = {};
	shaderConstants.sceneDI = sceneBuffer;
	shaderConstants.instancesDI = renderSetIndicesBuffer;

	int visibleInstanceIndex = 0; // Index to the visible buffer
	for(const auto& domainBase : renderSet.Elements)
	{
		if (domainBase.first == MaterialDomain::AlphaBlended)
			continue;

		for (const auto& cullBase : domainBase.second)
		{
			if (cullBase.second.empty())
				continue;

			rhi::IGraphicsPipelineState* PSO = renderContext.Get(domainBase.first, cullBase.first);
			if (!PSO)
			{
				LOG_ERROR("Material domain '{}', Cull mode '{}': No PSO defined in RenderContext '{}'",
					GetMaterialDomainString(domainBase.first),
					cullBase.first == rhi::CullMode::Back ? "Back" : cullBase.first == rhi::CullMode::Front ? "Front" : "None",
					renderContext.GetDebugName());
				continue;
			}

			commandList->SetPipelineState(PSO);
			const auto& instances = cullBase.second;

			st::gfx::Mesh* currentMesh = instances[0]->GetMesh().get();
			int prevIdx = 0;
			for (int i = 1; i < instances.size(); ++i)
			{
				const st::gfx::MeshInstance* meshInstance = instances[i];
				if (currentMesh != meshInstance->GetMesh().get())
				{
					shaderConstants.baseInstanceIdx = visibleInstanceIndex + prevIdx;
					shaderConstants.meshIndex = instances[prevIdx]->GetMeshSceneIndex();
					shaderConstants.materialIndex = instances[prevIdx]->GetMaterialSceneIndex();
					commandList->PushGraphicsConstants(shaderConstants);
					commandList->DrawInstanced(currentMesh->GetIndexCount(), i - prevIdx, 0);

					currentMesh = meshInstance->GetMesh().get();
					prevIdx = i;
				}
			}

			shaderConstants.baseInstanceIdx = visibleInstanceIndex + prevIdx;
			shaderConstants.meshIndex = instances[prevIdx]->GetMeshSceneIndex();
			shaderConstants.materialIndex = instances[prevIdx]->GetMaterialSceneIndex();
			commandList->PushGraphicsConstants(shaderConstants);
			commandList->DrawInstanced(currentMesh->GetIndexCount(), instances.size() - prevIdx, 0);

			visibleInstanceIndex += instances.size();
		}
	}
}