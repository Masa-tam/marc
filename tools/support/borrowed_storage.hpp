#ifndef MARC_TOOLS_BORROWED_STORAGE_HPP
#define MARC_TOOLS_BORROWED_STORAGE_HPP

#include <marc/marc.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

namespace marc::cli {

class Storage {
public:
  Storage() = default;
  Storage(const Storage &) = delete;
  Storage &operator=(const Storage &) = delete;
  ~Storage() {
    if (aligned_)
      ::operator delete(data_, std::align_val_t{alignment_});
    else
      ::operator delete(data_);
  }
  marc_status allocate(std::uint64_t bytes, std::uint64_t alignment) noexcept {
    if (data_ || !bytes || !alignment || (alignment & (alignment - 1)))
      return MARC_STATUS_INVALID_ARGUMENT;
    if (bytes > std::numeric_limits<std::size_t>::max() ||
        alignment > std::numeric_limits<std::size_t>::max())
      return MARC_STATUS_LIMIT_EXCEEDED;
    const auto a = static_cast<std::size_t>(alignment);
    const bool extended = a > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
    auto p = extended ? ::operator new(static_cast<std::size_t>(bytes),
                                       std::align_val_t{a}, std::nothrow)
                      : ::operator new(static_cast<std::size_t>(bytes),
                                       std::nothrow);
    if (!p)
      return MARC_STATUS_OUT_OF_MEMORY;
    data_ = p;
    bytes_ = static_cast<std::size_t>(bytes);
    alignment_ = a;
    aligned_ = extended;
    return MARC_STATUS_OK;
  }
  marc_buffer view() const noexcept {
    return {static_cast<std::uint8_t *>(data_), bytes_};
  }

private:
  void *data_{};
  std::size_t bytes_{}, alignment_{};
  bool aligned_{};
};

} // namespace marc::cli

#endif
