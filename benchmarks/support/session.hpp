#ifndef MARC_BENCHMARK_SESSION_HPP
#define MARC_BENCHMARK_SESSION_HPP

#include "stream_pump.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace marc::benchmark {

struct ResourceReport {
    // The historical three-span report is meaningful only for that API shape.
    marc_workspace_requirements workspace{};
    bool three_spans{};
    const char* scope{};
    std::uint64_t bytes{};
    std::uint64_t caller_bytes{};
    std::uint64_t limit_bytes{};
};

class Session {
public:
    virtual ~Session() = default;
    virtual marc_status create(marc_transform** transform) = 0;
    ResourceReport resources{};
};

struct SessionOptions {
    marc_direction direction{};
    std::uint64_t original_size{};
};

[[nodiscard]] inline std::uint64_t add_bytes(const std::uint64_t a,
                                            const std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a)
        throw std::overflow_error("benchmark resource size overflow");
    return a + b;
}

[[nodiscard]] inline std::uint64_t workspace_bytes(
    const marc_workspace_requirements& q) {
    return add_bytes(add_bytes(q.primary_bytes, q.secondary_bytes), q.views_bytes);
}

class Storage {
public:
    Storage() = default;
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    ~Storage() {
        if (extended_)
            ::operator delete(data_, std::align_val_t{alignment_});
        else
            ::operator delete(data_);
    }

    static void validate(const std::uint64_t bytes, const std::uint64_t alignment) {
        if (bytes == 0) return;
        if (bytes > std::numeric_limits<std::size_t>::max() || alignment == 0
            || (alignment & (alignment - 1)) != 0
            || alignment > std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("invalid benchmark workspace extent");
    }

    void allocate(const std::uint64_t bytes, const std::uint64_t alignment) {
        validate(bytes, alignment);
        if (data_ != nullptr || bytes == 0) return;
        bytes_ = static_cast<std::size_t>(bytes);
        alignment_ = static_cast<std::size_t>(alignment);
        extended_ = alignment_ > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
        data_ = extended_ ? ::operator new(bytes_, std::align_val_t{alignment_})
                          : ::operator new(bytes_);
    }

    [[nodiscard]] marc_buffer view() const noexcept {
        return {static_cast<std::uint8_t*>(data_), bytes_};
    }

private:
    void* data_{};
    std::size_t bytes_{}, alignment_{};
    bool extended_{};
};

template<class> struct QueryTypes;
template<class C, class R>
struct QueryTypes<marc_status (*)(const C*, R*) noexcept> {
    using Config = C;
    using Result = R;
};

template<class> struct DecoderBuffers;
template<class C, class B>
struct DecoderBuffers<marc_status (*)(const C*, const B*, marc_transform**) noexcept> {
    using Type = B;
};

// Four explicit policies used by the existing public CLI. They supply grants
// required by direction-specific Range initializers; no defaults are inferred.
struct RangeLimits {
    std::uint64_t window{}, payload{}, policy{}, tables{};
};

template<class C>
void configure_range(C& config, const RangeLimits& limits) {
    config.max_total_output_size = UINT64_C(1) << 40;
    config.max_frame_size = config.max_block_size = config.max_lz_distance = limits.window;
    config.max_lz_match_length = 258;
    config.max_compressed_payload_size = limits.payload;
    config.max_internal_buffered_bytes = limits.policy;
    config.max_entropy_table_entries = limits.tables;
    config.max_range_model_total = 32768;
    config.max_expansion_ratio = 1024;
    config.expansion_slack = UINT64_C(1) << 20;
}

template<class C>
void configure_retention(C& config) {
    if constexpr (requires { config.external_retained_bytes; }) {
        // Control objects use the same conservative reserve as owning CLI
        // helpers. The full two retained I/O arrays are charged separately,
        // including when --io-size supplies only a prefix to process().
        config.external_retained_bytes = 65536;
        config.input_capacity_bytes = config.output_capacity_bytes = io_capacity;
    }
}

template<auto Init, auto Query, auto Create, auto Apply = nullptr>
class PublicSession final : public Session {
    using C = typename QueryTypes<decltype(Query)>::Config;
    using R = typename QueryTypes<decltype(Query)>::Result;
public:
    marc_status prepare(const SessionOptions& options, const std::uint32_t profile,
                        const RangeLimits* limits) {
        static_assert(sizeof(PublicSession) < 16384);
        marc_status status{};
        if constexpr (std::is_invocable_v<decltype(Init), marc_direction, C*>)
            status = Init(options.direction, &config_);
        else
            status = Init(&config_);
        if (status != MARC_STATUS_OK) return status;
        if constexpr (!std::is_same_v<decltype(Apply), std::nullptr_t>) {
            status = Apply(&config_, profile);
            if (status != MARC_STATUS_OK) return status;
        }
        config_.original_size = options.original_size;
        configure_retention(config_);
        // Only the explicit direction-specific Range session receives limits.
        if constexpr (!std::is_invocable_v<decltype(Init), marc_direction, C*>) {
            if (limits == nullptr) return MARC_STATUS_INVALID_ARGUMENT;
            configure_range(config_, *limits);
        }
        if constexpr (requires { query_.struct_size; }) {
            query_.struct_size = sizeof(query_);
            query_.abi_version = MARC_ABI_VERSION;
        }
        status = Query(&config_, &query_);
        if (status != MARC_STATUS_OK) return status;
        resources.limit_bytes = config_.max_internal_buffered_bytes;
        if constexpr (std::is_same_v<R, marc_workspace_requirements>) {
            resources.workspace = query_;
            resources.three_spans = true;
            resources.scope = "caller-workspace";
            resources.bytes = resources.caller_bytes = workspace_bytes(query_);
            Storage::validate(query_.primary_bytes, alignof(std::max_align_t));
            Storage::validate(query_.secondary_bytes, alignof(std::max_align_t));
            Storage::validate(query_.views_bytes, query_.views_alignment);
        } else if constexpr (requires { query_.initial_bytes; }) {
            if (query_.admission_scope != 1) return MARC_STATUS_INTERNAL_ERROR;
            resources.scope = "initial-only";
            resources.bytes = query_.initial_bytes;
        } else {
            resources.scope = "aggregate-admission";
            resources.bytes = query_.minimum_aggregate_bytes;
        }
        return MARC_STATUS_OK;
    }

    marc_status create(marc_transform** transform) override {
        if constexpr (std::is_same_v<R, marc_workspace_requirements>) {
            storage_[0].allocate(query_.primary_bytes, alignof(std::max_align_t));
            storage_[1].allocate(query_.secondary_bytes, alignof(std::max_align_t));
            storage_[2].allocate(query_.views_bytes, query_.views_alignment);
            return Create(&config_, storage_[0].view(), storage_[1].view(),
                          storage_[2].view(), transform);
        } else {
            return Create(&config_, transform);
        }
    }

private:
    C config_{};
    R query_{};
    std::array<Storage, 3> storage_;
};

// The five borrowed decoder spans intentionally have a separate adapter. They
// are not reported as three-span storage or as internally owned resources.
template<auto Init, auto Query, auto Create>
class RangeDecoderSession final : public Session {
    using C = typename QueryTypes<decltype(Query)>::Config;
    using R = typename QueryTypes<decltype(Query)>::Result;
    using B = typename DecoderBuffers<decltype(Create)>::Type;
public:
    marc_status prepare(const RangeLimits& limits) {
        static_assert(sizeof(RangeDecoderSession) < 16384);
        auto status = Init(&config_);
        if (status != MARC_STATUS_OK) return status;
        configure_range(config_, limits);
        configure_retention(config_);
        query_.struct_size = sizeof(query_);
        query_.abi_version = MARC_ABI_VERSION;
        status = Query(&config_, &query_);
        if (status != MARC_STATUS_OK) return status;
        if (query_.admission_scope != 1) return MARC_STATUS_INTERNAL_ERROR;
        resources.scope = "capacity-only";
        resources.bytes = query_.minimum_aggregate_bytes;
        resources.limit_bytes = config_.max_internal_buffered_bytes;
        extents_ = {query_.serialized_bytes, query_.token_bytes,
                    query_.token_scratch_bytes, query_.raw_bytes, query_.raw_scratch_bytes};
        alignments_ = {alignof(std::max_align_t), query_.token_alignment,
                       query_.token_alignment, alignof(std::max_align_t), alignof(std::max_align_t)};
        for (std::size_t i = 0; i < storage_.size(); ++i) {
            Storage::validate(extents_[i], alignments_[i]);
            resources.caller_bytes = add_bytes(resources.caller_bytes, extents_[i]);
        }
        return MARC_STATUS_OK;
    }

    marc_status create(marc_transform** transform) override {
        for (std::size_t i = 0; i < storage_.size(); ++i)
            storage_[i].allocate(extents_[i], alignments_[i]);
        const B buffers{sizeof(B), MARC_ABI_VERSION, 0, 0,
                        storage_[0].view(), storage_[1].view(), storage_[2].view(),
                        storage_[3].view(), storage_[4].view()};
        return Create(&config_, &buffers, transform);
    }

private:
    C config_{};
    R query_{};
    std::array<Storage, 5> storage_;
    std::array<std::uint64_t, 5> extents_{}, alignments_{};
};

} // namespace marc::benchmark

#endif
