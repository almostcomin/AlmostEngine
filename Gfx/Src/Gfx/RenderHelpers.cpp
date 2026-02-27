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

void st::gfx::RenderSetInstanced(const std::vector<const st::gfx::MeshInstance*>& instancesSet, st::rhi::BufferUniformView sceneBuffer,
	st::rhi::BufferReadOnlyView instancesIndexBuffer, st::rhi::ICommandList* commandList)
{
	if (instancesSet.empty())
		return;

	interop::MultiInstanceDrawConstants shaderConstants;
	shaderConstants.sceneDI = sceneBuffer;
	shaderConstants.instancesDI = instancesIndexBuffer;

	st::gfx::Mesh* currentMesh = instancesSet[0]->GetMesh().get();
	int prevIdx = 0;
	for (int i = 0; i < instancesSet.size(); ++i)
	{
		const st::gfx::MeshInstance* meshInstance = instancesSet[i];
		if (currentMesh != meshInstance->GetMesh().get())
		{
			shaderConstants.baseInstanceIdx = prevIdx;
			shaderConstants.meshIndex = instancesSet[prevIdx]->GetMeshSceneIndex();
			shaderConstants.materialIndex = instancesSet[prevIdx]->GetMaterialSceneIndex();
			commandList->PushGraphicsConstants(shaderConstants);
			commandList->DrawInstanced(currentMesh->GetIndexCount(), i - prevIdx, 0);

			currentMesh = meshInstance->GetMesh().get();
			prevIdx = i;
		}
	}

	shaderConstants.baseInstanceIdx = prevIdx;
	shaderConstants.meshIndex = instancesSet[prevIdx]->GetMeshSceneIndex();
	shaderConstants.materialIndex = instancesSet[prevIdx]->GetMaterialSceneIndex();
	commandList->PushGraphicsConstants(shaderConstants);
	commandList->DrawInstanced(currentMesh->GetIndexCount(), instancesSet.size() - prevIdx, 0);
}