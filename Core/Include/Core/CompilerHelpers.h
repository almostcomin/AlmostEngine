#pragma once

// CompilerHelpers.h

// --- Compiler detection (useful going forward) ---
#if defined(_MSC_VER) && !defined(__clang__)
#define ALM_COMPILER_MSVC 1
#elif defined(__clang__)
#define ALM_COMPILER_CLANG 1
#elif defined(__GNUC__)
#define ALM_COMPILER_GCC 1
#endif

// --- Silence warnings from third-party headers ---
#if defined(_MSC_VER) && !defined(__clang__)

#define ALM_DISABLE_THIRDPARTY_WARNINGS \
        __pragma(warning(push, 0))
    // warning(push, 0) lowers the warning level to 0 for the whole header,
    // so you don't have to enumerate warning codes one by one.

#define ALM_RESTORE_WARNINGS \
        __pragma(warning(pop))

#elif defined(__clang__)

#define ALM_DISABLE_THIRDPARTY_WARNINGS \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"") \
        _Pragma("clang diagnostic ignored \"-Weverything\"")

#define ALM_RESTORE_WARNINGS \
        _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)

#define ALM_DISABLE_THIRDPARTY_WARNINGS \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define ALM_RESTORE_WARNINGS \
        _Pragma("GCC diagnostic pop")

#else
#define ALM_DISABLE_THIRDPARTY_WARNINGS
#define ALM_RESTORE_WARNINGS
#endif