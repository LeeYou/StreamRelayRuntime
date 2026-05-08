#include "gateway/write_queue.h"

#include <utility>

namespace streamrelay::gateway {

WriteQueue::WriteQueue(std::size_t soft_limit_bytes, std::size_t hard_limit_bytes)
    : soft_limit_bytes_(soft_limit_bytes), hard_limit_bytes_(hard_limit_bytes) {}

core::Result<void> WriteQueue::enqueue(core::ByteBuffer bytes) {
    if (hard_limit_bytes_ < soft_limit_bytes_) {
        return core::make_error(core::ErrorCode::InvalidState, "hard limit is lower than soft limit");
    }

    if (pending_bytes_ + bytes.size() > hard_limit_bytes_) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "write queue hard limit exceeded");
    }

    pending_bytes_ += bytes.size();
    queue_.push_back(std::move(bytes));
    return core::success();
}

core::Result<core::ByteBuffer> WriteQueue::pop_front() {
    if (queue_.empty()) {
        return core::make_error(core::ErrorCode::NotFound, "write queue is empty");
    }

    auto bytes = std::move(queue_.front());
    queue_.pop_front();
    pending_bytes_ -= bytes.size();
    return bytes;
}

void WriteQueue::clear() noexcept {
    queue_.clear();
    pending_bytes_ = 0;
}

std::size_t WriteQueue::pending_bytes() const noexcept {
    return pending_bytes_;
}

std::size_t WriteQueue::pending_frames() const noexcept {
    return queue_.size();
}

WriteQueueState WriteQueue::state() const noexcept {
    if (pending_bytes_ >= hard_limit_bytes_) {
        return WriteQueueState::HardLimited;
    }

    if (pending_bytes_ >= soft_limit_bytes_) {
        return WriteQueueState::SoftLimited;
    }

    return WriteQueueState::Normal;
}

std::size_t WriteQueue::soft_limit_bytes() const noexcept {
    return soft_limit_bytes_;
}

std::size_t WriteQueue::hard_limit_bytes() const noexcept {
    return hard_limit_bytes_;
}

} 
