#ifndef MARC_C_API_OWNED_BOUNDARY_HPP
#define MARC_C_API_OWNED_BOUNDARY_HPP

#include "c_api/buffer_validation.hpp"
#include "c_api/transform_handle.hpp"
#include "core/status.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace marc::c_api {

// Shared only by factories with this exact capacity/overlap/terminal policy.
class OwnedBoundary final : public core::Transform {
public:
    OwnedBoundary(std::unique_ptr<core::Transform> transform, std::size_t in,
                  std::size_t out) noexcept
        : transform_(std::move(transform)), input_capacity_(in), output_capacity_(out) {}
    void handle(marc_transform* h) noexcept { handle_ = h; }
    core::ProcessResult process(std::span<const std::byte> input, std::span<std::byte> output,
                                std::uint32_t flags) noexcept override {
        if (terminal_)
            return {0, 0, last_.status, last_.error};
        const bool limited = input.size() > input_capacity_ || output.size() > output_capacity_;
        if (limited || !disjoint_buffers(input.data(), input.size(), handle_, sizeof(*handle_)) ||
            !disjoint_buffers(output.data(), output.size(), handle_, sizeof(*handle_)) ||
            !disjoint_buffers(input.data(), input.size(), this, sizeof(*this)) ||
            !disjoint_buffers(output.data(), output.size(), this, sizeof(*this))) {
            last_ = {0,
                     0,
                     core::StreamStatus::error,
                     {limited ? core::ErrorCode::limit_exceeded : core::ErrorCode::invalid_argument,
                      accepted_, 0}};
            terminal_ = true;
            return last_;
        }
        last_ = transform_->process(input, output, flags);
        if (!core::checked_add(accepted_, static_cast<std::uint64_t>(last_.input_consumed),
                               accepted_))
            last_.status = core::StreamStatus::error,
            last_.error = {core::ErrorCode::internal_error, accepted_, 0};
        terminal_ = last_.status == core::StreamStatus::error ||
                    last_.status == core::StreamStatus::end_of_stream;
        return last_;
    }

private:
    std::unique_ptr<core::Transform> transform_;
    marc_transform* handle_{};
    std::size_t input_capacity_{}, output_capacity_{};
    std::uint64_t accepted_{};
    core::ProcessResult last_{};
    bool terminal_{};
};

} // namespace marc::c_api

#endif
