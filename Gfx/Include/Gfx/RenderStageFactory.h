#pragma once

#include "Gfx/RenderStageType.h"

namespace alm::gfx
{

class RenderStage;

inline uint32_t MakeRenderStageID(std::string_view name)
{
    return Hash(name);
}

class RenderStageFactory
{
public:

	using CreateFn = std::function<std::unique_ptr<RenderStage>()>;

	static void Register(RenderStageTypeID id, CreateFn fn);

	static std::unique_ptr<RenderStage> Create(RenderStageTypeID id);
    static std::shared_ptr<RenderStage> CreateShared(RenderStageTypeID id);

    template<class T>
    static std::unique_ptr<T> Create()
    {
        auto base = Create(T::StaticType());
        if (!base)
            return nullptr;
        return std::unique_ptr<T>(static_cast<T*>(base.release()));
    }

    template<class T>
    static std::shared_ptr<T> CreateShared()
    {
        return std::move(Create<T>());
    }

private:

    static std::unordered_map<RenderStageTypeID, CreateFn>& Creators()
    {
        static std::unordered_map<RenderStageTypeID, CreateFn> creators;
        return creators;
    }
};

template<typename T>
struct RenderStageRegister
{
    RenderStageRegister(RenderStageTypeID id)
    {
        RenderStageFactory::Register(id, []()
        {
            return std::make_unique<T>();
        });
    }
};

#define REGISTER_RENDER_STAGE(ClassName) \
    public: \
        static alm::gfx::RenderStageTypeID StaticType() { return alm::gfx::MakeRenderStageID(#ClassName); } \
        virtual alm::gfx::RenderStageTypeID GetType() const override { return ClassName::StaticType(); } \
        virtual const char* GetDebugName() const override { return #ClassName; } \
    private: \
        inline static alm::gfx::RenderStageRegister<ClassName> s_Register{ ClassName::StaticType() };

} // namespace alm::gfx