#pragma once

#include "RHI/Resource.h"
#include <string>
#include "Core/Common.h"
#include "Core/Blob.h"

namespace st::rhi
{
    // Shader type mask. The values match ones used in Vulkan.
    enum class ShaderType : uint16_t
    {
        None = 0x0000,

        Compute = 0x0020,

        Vertex = 0x0001,
        Hull = 0x0002,
        Domain = 0x0004,
        Geometry = 0x0008,
        Pixel = 0x0010,
        Amplification = 0x0040,
        Mesh = 0x0080,
        AllGraphics = 0x00DF,

        RayGeneration = 0x0100,
        AnyHit = 0x0200,
        ClosestHit = 0x0400,
        Miss = 0x0800,
        Intersection = 0x1000,
        Callable = 0x2000,
        AllRayTracing = 0x3F00,

        All = 0x3FFF
    };
    ENUM_CLASS_FLAG_OPERATORS(ShaderType)

    struct ShaderDesc
    {
        ShaderType Type = ShaderType::None;
        std::string DebugName = "{null}";
        std::string EntryPoint = "main";

        constexpr ShaderDesc& SetShaderType(st::rhi::ShaderType value) { Type = value; return *this; }
        ShaderDesc& SetDebugName(const std::string& value) { DebugName = value; return *this; }
        ShaderDesc& SetEntryPoint(const std::string& value) { EntryPoint = value; return *this; }
    };

	class IShader : public IResource
	{
    public:

		virtual const ShaderDesc& GetDesc() const = 0;
		virtual const WeakBlob& GetBytecode() const = 0;

        ResourceType GetResourceType() const override { return ResourceType::Shader; }
	};

	using ShaderHandle = st::weak<IShader>;
}