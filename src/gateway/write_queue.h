#pragma once

#include <cstddef>
#include <deque>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::gateway {

enum class WriteQueueState {
    Normal,
    SoftLimited,
    HardLimited,
};

class WriteQueue {
public:
    WriteQueue(std::size_t soft_limit_bytes, std::size_t hard_limit_bytes);

    core::Result<void> enqueue(core::ByteBuffer bytes);
    core::Result<core::ByteBuffer> pop_front();
    void clear() noexcept;

    std::size_t pending_bytes() const noexcept;
    std::size_t pending_frames() const noexcept;
    WriteQueueState state() const noexcept;
    std::size_t soft_limit_bytes() const noexcept;
    std::size_t hard_limit_bytes() const noexcept;

private:
    std::deque<core::ByteBuffer> queue_;
    std::size_t pending_bytes_{0};
    std::size_t soft_limit_bytes_{0};
    std::size_t hard_limit_bytes_{0};
};

} 
