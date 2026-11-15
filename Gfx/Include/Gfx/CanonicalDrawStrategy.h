#pragma once

#include "Gfx/IDrawStrategy.h"

namespace alm::gfx
{

class CanonicalDrawStrategy : public IDrawStrategy
{
public:

	void Prepare(const RenderSet& drawInfos) override;
	DrawBucket GetNextBucket() override;
	void Clear() override;

private:

	const RenderSet* m_RenderSet = nullptr;
	std::vector<RenderableDrawInfo>::const_iterator m_Next = {};
};

} // namespace alm::gfx