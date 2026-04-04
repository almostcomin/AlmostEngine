#include "Gfx/GfxPCH.h"
#include "Gfx/RenderStageFactory.h"
#include "Gfx/RenderStage.h"

void alm::gfx::RenderStageFactory::Register(alm::gfx::RenderStageTypeID id, CreateFn fn)
{
	Creators()[id] = fn;
}

std::unique_ptr<alm::gfx::RenderStage> alm::gfx::RenderStageFactory::Create(alm::gfx::RenderStageTypeID id)
{
	if (id == RenderStageType_None)
		return nullptr;

	auto it = Creators().find(id);

	if (it != Creators().end())
		return it->second();

	return nullptr;
}

std::shared_ptr<alm::gfx::RenderStage> alm::gfx::RenderStageFactory::CreateShared(alm::gfx::RenderStageTypeID id)
{
	return std::move(Create(id));
}