#pragma once

#include <memory>
#include "Core/Common.h"
#include "Core/Memory.h"

namespace st::rhi
{
	class Device;

	enum class ResourceType
	{
		Unknown = 0,

		Buffer,
		Texture,
		Framebuffer,
		Fence,
		CommandList,
		Shader,
		GraphicsPipelineState
	};

	inline const char* ResourceTypeToString(ResourceType type)
	{
		switch (type)
		{
		case ResourceType::Buffer: return "Buffer";
		case ResourceType::Texture: return "Texture";
		case ResourceType::Framebuffer: return "Framebuffer";
		case ResourceType::Fence: return "Fence";
		case ResourceType::CommandList: return "CommandList";
		case ResourceType::Shader: return "Shader";
		case ResourceType::GraphicsPipelineState: return "GraphicsPipelineState";
		case ResourceType::Unknown:
		default:
			return "Unknown";
		};
	};

	struct NativeResource
	{
		union {
			uint64_t integer;
			void* pointer;
		};

		NativeResource(uint64_t i) : integer(i) {}
		NativeResource(void* p) : pointer(p) {}

		template<typename T> 
		operator T* () const { return static_cast<T*>(pointer); }
	};

	class IResource : public st::enable_weak_from_this<IResource>, st::noncopyable_nonmovable
	{
		friend class st::rhi::Device;

	public:

		virtual ResourceType GetResourceType() const = 0;

		// Returns a native object or interface, for example ID3D11Device*, or nullptr if the requested interface is unavailable.
		// Does *not* AddRef the returned interface.
		virtual NativeResource GetNativeResource() { assert(0); return nullptr; }

		virtual void Release(Device* device) = 0;

		const std::string& GetDebugName() const { return m_DebugName; }
		const Device* GetDevice() const { return m_Device; }
		Device* GetDevice() { return m_Device; }

	protected:

		IResource(Device*, const std::string& debugName);
		virtual ~IResource();

	private:

		std::string m_DebugName;
		Device* m_Device;
	};

	struct ResourcePtrHash 
	{
		using is_transparent = void;

		size_t operator()(const st::unique<IResource>& ptr) const 
		{
			return std::hash<IResource*>()(ptr.get());
		}

		size_t operator()(IResource* ptr) const 
		{
			return std::hash<IResource*>()(ptr);
		}
	};

	struct ResourcePtrEqual 
	{
		using is_transparent = void;

		bool operator()(const st::unique<IResource>& a, const st::unique<IResource>& b) const 
		{
			return a.get() == b.get();
		}

		bool operator()(const st::unique<IResource>& a, IResource* b) const 
		{
			return a.get() == b;
		}

		bool operator()(IResource* a, const st::unique<IResource>& b) const 
		{
			return a == b.get();
		}
	};
}