#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
namespace receipt {
struct Event {
  void *p;
  std::size_t bytes, alignment, freed;
};
std::array<Event, 128> events{};
std::size_t count{}, calls{}, refuse{}, deletion{};
bool active{};
void *allocate(std::size_t n, std::size_t a, bool fallible) {
  if (active && fallible && ++calls == refuse)
    return nullptr;
  void *base{}, *p{};
  if (a) {
    if (n > std::numeric_limits<std::size_t>::max() - a - sizeof(void *))
      return nullptr;
    base = std::malloc(n + a + sizeof(void *));
    if (!base)
      return nullptr;
    auto address =
        (reinterpret_cast<std::uintptr_t>(base) + sizeof(void *) + a - 1) &
        ~(a - 1);
    p = reinterpret_cast<void *>(address);
    std::memcpy(static_cast<char *>(p) - sizeof(void *), &base, sizeof(base));
  } else
    p = std::malloc(n ? n : 1);
  if (p && active && fallible) {
    if (count == events.size())
      std::abort();
    events[count++] = {p, n, a, 0};
  }
  return p;
}
void release(void *p, bool aligned = false) noexcept {
  if (!p)
    return;
  for (std::size_t i = 0; i < count; ++i)
    if (events[i].p == p && !events[i].freed)
      events[i].freed = ++deletion;
  if (aligned) {
    void *base{};
    std::memcpy(&base, static_cast<char *>(p) - sizeof(void *), sizeof(base));
    std::free(base);
  } else
    std::free(p);
}
void start(std::size_t fail) {
  events.fill({});
  count = calls = deletion = 0;
  refuse = fail;
  active = true;
}
void stop() {
  active = false;
  for (std::size_t i = 0; i < count; ++i)
    if (!events[i].freed)
      std::abort();
}
} // namespace receipt
void *operator new(std::size_t n) {
  if (auto p = receipt::allocate(n, 0, false))
    return p;
  throw std::bad_alloc{};
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
  return receipt::allocate(n, 0, true);
}
void *operator new[](std::size_t n, const std::nothrow_t &) noexcept {
  return receipt::allocate(n, 0, true);
}
void operator delete(void *p) noexcept { receipt::release(p); }
void operator delete[](void *p) noexcept { receipt::release(p); }
void operator delete(void *p, std::size_t) noexcept { receipt::release(p); }
void operator delete[](void *p, std::size_t) noexcept { receipt::release(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept {
  receipt::release(p);
}
void operator delete[](void *p, const std::nothrow_t &) noexcept {
  receipt::release(p);
}
void *operator new(std::size_t n, std::align_val_t a) {
  if (auto p = receipt::allocate(n, static_cast<std::size_t>(a), false))
    return p;
  throw std::bad_alloc{};
}
void *operator new[](std::size_t n, std::align_val_t a) {
  return ::operator new(n, a);
}
void *operator new(std::size_t n, std::align_val_t a,
                   const std::nothrow_t &) noexcept {
  return receipt::allocate(n, static_cast<std::size_t>(a), true);
}
void *operator new[](std::size_t n, std::align_val_t a,
                     const std::nothrow_t &) noexcept {
  return receipt::allocate(n, static_cast<std::size_t>(a), true);
}
void operator delete(void *p, std::align_val_t) noexcept {
  receipt::release(p, true);
}
void operator delete[](void *p, std::align_val_t) noexcept {
  receipt::release(p, true);
}
void operator delete(void *p, std::size_t, std::align_val_t) noexcept {
  receipt::release(p, true);
}
void operator delete[](void *p, std::size_t, std::align_val_t) noexcept {
  receipt::release(p, true);
}
void operator delete(void *p, std::align_val_t,
                     const std::nothrow_t &) noexcept {
  receipt::release(p, true);
}
void operator delete[](void *p, std::align_val_t,
                       const std::nothrow_t &) noexcept {
  receipt::release(p, true);
}
#define main qualified_cli_main
#include "../tools/marc_cli.cpp"
#undef main
#include <chrono>
#include <stdexcept>
void require(bool b) {
  if (!b)
    throw std::runtime_error("allocation/file transaction invariant");
}
int main(int argc, char **argv) {
  try {
    require(argc == 2 || argc == 3);
    const auto parent = std::filesystem::path(argv[argc - 1]);
    std::filesystem::create_directories(parent);
    const auto root =
        parent /
        ("run-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(root));
    std::filesystem::path source;
    if (argc == 3)
      source = argv[1];
    else {
      auto raw = root / "seed-raw.bin";
      source = root / "seed.marc";
      {
        std::ofstream f(raw, std::ios::binary);
        std::array<char, 64> b{};
        b.fill('A');
        f.write(b.data(), b.size());
        require(bool(f));
      }
      auto in = raw.string(), out = source.string();
      const char *args[]{"marc",     "encode",
                         "--codec",  "lzss-position-distance-dynamic-range-16m",
                         in.c_str(), out.c_str()};
      require(qualified_cli_main(6, args) == 0);
    }
    receipt::start(0);
    {
      marc_cli_16m::Storage storage;
      require(storage.allocate(129, 64) == MARC_STATUS_OK);
      require(reinterpret_cast<std::uintptr_t>(storage.view().data) % 64 == 0);
      require(storage.allocate(1, 1) == MARC_STATUS_INVALID_ARGUMENT);
    }
    receipt::stop();
    require(receipt::count == 1 && receipt::events[0].alignment == 64);
    std::size_t allocations{};
    for (std::size_t k = 0; k <= allocations || k == 0; ++k) {
      auto output =
          root / ((k ? "refusal-" : "positive-") + std::to_string(k) + ".bin");
      auto in = source.string(), out = output.string();
      const char *args[]{"marc",     "decode",
                         "--codec",  "lzss-position-distance-dynamic-range-16m",
                         in.c_str(), out.c_str()};
      receipt::start(k);
      const auto code = qualified_cli_main(6, args);
      if (!k)
        allocations = receipt::calls;
      receipt::stop();
      require(code == (k ? 1 : 0));
      require(!std::filesystem::exists(output.string() + ".tmp"));
      require(std::filesystem::exists(output) == (k == 0));
      if (!k) {
        require(allocations == 9 && receipt::count == 9);
        auto &e = receipt::events;
        // First five borrowed owners must be released after both factory
        // objects.
        for (std::size_t i = 0; i < 5; ++i)
          require(e[i].freed > e[5].freed && e[i].freed > e[6].freed);
        std::cout << "{\"external_charge\":";
        std::uint64_t external{};
        require(marc_cli_16m::external_charge(external));
        std::cout << external << ",\"workspace_bytes\":[";
        for (std::size_t i = 0; i < 5; ++i)
          std::cout << (i ? "," : "") << e[i].bytes;
        std::cout << "],\"fallible_allocations\":" << allocations << "}\n";
      }
    }
    std::cout << "PASS nine allocation refusals, aligned storage and real CLI "
                 "cleanup/lifetime order\n";
    auto raw = root / "raw.bin";
    {
      std::ofstream file(raw, std::ios::binary);
      std::array<char, 64> bytes{};
      bytes.fill('A');
      file.write(bytes.data(), bytes.size());
      require(bool(file));
    }
    allocations = 0;
    for (std::size_t k = 0; k <= allocations || k == 0; ++k) {
      auto output = root / ("encode-" + std::to_string(k) + ".marc");
      auto in = raw.string(), out = output.string();
      const char *args[]{"marc",     "encode",
                         "--codec",  "lzss-position-distance-dynamic-range-16m",
                         in.c_str(), out.c_str()};
      receipt::start(k);
      const auto code = qualified_cli_main(6, args);
      if (!k)
        allocations = receipt::calls;
      receipt::stop();
      require(allocations > 9 && code == (k ? 1 : 0));
      require(!std::filesystem::exists(output.string() + ".tmp") &&
              std::filesystem::exists(output) == (k == 0));
    }
    std::cout << "PASS " << allocations
              << " owning encoder allocation refusals including deferred "
                 "generation\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
