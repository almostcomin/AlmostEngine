#include "Framework/FrameworkPCH.h"
#include "Framework/App.h"

class OutdoorsApp : public alm::App
{
public:

	OutdoorsApp() : alm::App{ "OutdoorsApp" } {}
	~OutdoorsApp() override = default;

	bool Initialize(const AppArgs& args) override
	{
		return true;
	}

	bool Update(float deltaTime) override
	{
		return true;
	}

	void Shutdown() override
	{
	}

	void OnSDLEvent(const SDL_Event& event) override
	{
	}

	//alm::gfx::RenderStageTypeID GetUIRenderStageType() const override { return StructureUI::StaticType(); }

private:

};

std::unique_ptr<alm::App> CreateApp()
{
	return std::unique_ptr<alm::App>{ new OutdoorsApp };
}
