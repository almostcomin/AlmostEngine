#include "Gfx/GfxPCH.h"
#include "Gfx/MaterialRef.h"
#include "Gfx/MaterialManager.h"

alm::gfx::MaterialRef::MaterialRef(const MaterialRef& other) : m_Proxy{ other.m_Proxy }
{
	if (m_Proxy)
	{
		m_Proxy->RefCount.fetch_add(1);
	}
}

alm::gfx::MaterialRef::MaterialRef(MaterialRef::Proxy* proxy) : m_Proxy{ proxy }
{
	if (m_Proxy)
	{
		m_Proxy->RefCount.fetch_add(1);
	}
}

alm::gfx::MaterialRef::~MaterialRef()
{
	Reset();
}

alm::gfx::MaterialRef& alm::gfx::MaterialRef::operator=(const MaterialRef& other)
{
	if (this == &other)
		return *this;

	Reset();

	m_Proxy = other.m_Proxy;
	if (m_Proxy)
	{
		m_Proxy->RefCount.fetch_add(1);
	}
	
	return *this;
}

void alm::gfx::MaterialRef::Reset()
{
	if (m_Proxy != nullptr)
	{
		if (m_Proxy->RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			m_Proxy->MaterialManager->DumpMaterial(m_Proxy);
		}
	}
}
