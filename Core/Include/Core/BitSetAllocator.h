#pragma once

#include <vector>
#include <mutex>

namespace st
{

class BitSetAllocator
{
public:
    explicit BitSetAllocator(size_t capacity, bool multithreaded);

    int Allocate();
    void Release(int index);

    size_t GetCapacity() const { return m_Allocated.size(); }

private:
    int m_NextAvailable = 0;
    std::vector<bool> m_Allocated;
    bool m_MultiThreaded;
    std::mutex m_Mutex;
};

} // namespace st