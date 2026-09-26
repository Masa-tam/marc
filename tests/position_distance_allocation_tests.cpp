#include "marc/marc.h"
#include "test_assert.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

// Isolated executable only: no allocation controls enter the production ABI.
namespace {
bool armed{};
unsigned fail_at{}, attempts{}, released{};
void* live[2]{};
void release(void* p) noexcept {
    if (p != nullptr) for (auto& slot : live) {
        if (slot == p) { slot = nullptr; ++released; break; }
    }
    std::free(p);
}
}
void* operator new(std::size_t n) {
    assert(!armed); // An unexpected throwing allocation must not evade tracking.
    if (void* p = std::malloc(n ? n : 1)) return p;
    std::abort(); // Unexpected real exhaustion, not the injected failure path.
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    if (armed && ++attempts == fail_at) return nullptr;
    void* p = std::malloc(n ? n : 1);
    assert(p != nullptr);
    if (armed) {
        assert(attempts >= 1 && attempts <= 2);
        live[attempts-1] = p;
    }
    return p;
}
void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept {
    return ::operator new(n, tag);
}
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }

int main() {
    uint8_t input[66]{}, archive[2048]{}, decoded[66]{};
    for (std::size_t i = 0; i < sizeof(input); ++i) input[i] = static_cast<uint8_t>(i % 7);
    std::size_t archive_size{};
    for (const auto direction : {MARC_DIRECTION_ENCODE, MARC_DIRECTION_DECODE}) {
        marc_lzss_position_distance_dynamic_range_config c{};
        assert(marc_lzss_position_distance_dynamic_range_config_init(direction, &c) == MARC_STATUS_OK);
        c.original_size = sizeof(input);
        c.frame_size = 33; c.max_frame_size = 33; c.max_block_size = 33;
        c.max_compressed_payload_size = 18*33+5;
        marc_workspace_requirements r{};
        assert(marc_lzss_position_distance_dynamic_range_workspace_requirements(&c,&r) == MARC_STATUS_OK);
        marc_buffer p{static_cast<uint8_t*>(std::malloc(r.primary_bytes)), r.primary_bytes};
        marc_buffer s{static_cast<uint8_t*>(std::malloc(r.secondary_bytes)), r.secondary_bytes};
        marc_buffer v{static_cast<uint8_t*>(std::malloc(r.views_bytes)), r.views_bytes};
        assert(p.data && s.data && v.data);
        // Success first captures a two-frame archive for the decoder tests.
        for (unsigned failure : {0u, 1u, 2u, 0u}) {
            assert(!live[0] && !live[1]);
            attempts = 0; released = 0; fail_at = failure; armed = true;
            assert(marc_lzss_position_distance_dynamic_range_workspace_requirements(&c,&r) == MARC_STATUS_OK);
            assert(attempts == 0);
            marc_transform* t = nullptr;
            auto status = marc_lzss_position_distance_dynamic_range_create(&c,p,s,v,&t);
            if (failure) {
                assert(status == MARC_STATUS_OUT_OF_MEMORY && t == nullptr);
                assert(attempts == failure && released == failure-1);
            } else {
                assert(status == MARC_STATUS_OK && t != nullptr && attempts == 2);
                const auto source_data = direction == MARC_DIRECTION_ENCODE ? input : archive;
                const auto source_size = direction == MARC_DIRECTION_ENCODE ? sizeof(input) : archive_size;
                auto sink_data = direction == MARC_DIRECTION_ENCODE ? archive : decoded;
                const auto sink_size = direction == MARC_DIRECTION_ENCODE ? sizeof(archive) : sizeof(decoded);
                std::size_t consumed{}, produced{};
                marc_status process_status{};
                for (unsigned calls = 0; calls < 4096; ++calls) {
                    const marc_const_buffer source{source_data + consumed, source_size - consumed};
                    const marc_buffer sink{sink_data + produced, produced == sink_size ? std::size_t{0} : std::size_t{1}};
                    const auto result = marc_transform_process(t,source,sink,MARC_PROCESS_END_INPUT);
                    assert(result.input_consumed <= source.size && result.output_produced <= sink.size);
                    assert(result.status >= MARC_STATUS_PROGRESS && result.status <= MARC_STATUS_END_OF_STREAM);
                    consumed += result.input_consumed; produced += result.output_produced;
                    process_status = result.status;
                    assert(attempts == 2);
                    if (process_status == MARC_STATUS_END_OF_STREAM) break;
                }
                assert(process_status == MARC_STATUS_END_OF_STREAM && consumed == source_size);
                if (direction == MARC_DIRECTION_ENCODE) archive_size = produced;
                else {
                    assert(produced == sizeof(input));
                    assert(std::memcmp(input,decoded,sizeof(input)) == 0);
                }
                assert(attempts == 2); // No process-time allocation.
                marc_transform_destroy(t);
                assert(released == 2);
            }
            assert(!live[0] && !live[1]);
            armed = false;
        }
        c.reserved = 1; attempts = 0; armed = true;
        marc_transform* t = nullptr;
        assert(marc_lzss_position_distance_dynamic_range_create(&c,p,s,v,&t) == MARC_STATUS_INVALID_ARGUMENT);
        assert(t == nullptr && attempts == 0 && !live[0] && !live[1]);
        armed = false;
        std::free(v.data); std::free(s.data); std::free(p.data);
    }
    return 0;
}
