#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <chrono>
#include <random>
#include <array>
#include <cassert>
#include <filesystem>
#include <span>
#include <map>
#include <set>
#include <ranges>
#include <numeric>
#include <sstream>
#include <format>
#include <thread>

#if _WIN32

#define COM_NO_WINDOWS_H
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <objbase.h>
#include <combaseapi.h>
#include <Unknwn.h>
#include <dxgi.h>
#include <dxgidebug.h>

#endif // _WIN32


#include "Core/static_vector.h"
#include "Core/unique_vector.h"
#include "Core/Log.h"
#include "Core/Math/glm_config.h"
#include "Core/Math.h"
#include "Core/Math/aabox.h"
#include "Core/Math/plane.h"
#include "Core/Math/frustum.h"
#include "Core/Memory.h"


