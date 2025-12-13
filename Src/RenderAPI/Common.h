#pragma once

namespace st::rapi
{

struct StorageRequirements
{
	size_t size;
	size_t alignment;
};

enum class CopyMethod
{
	Buffer2Buffer,
	Buffer2Texture
};

};