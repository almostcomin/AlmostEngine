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

#if _WIN32
#include <Windows.h>
#endif

#include "Core/static_vector.h"
#include "Core/Log.h"
#include "Core/ComPtr.h"
#include "Core/Math.h"