// Private diagnostic only. Independently authored from repository contracts.
// No production target, codec selection, stream format or default is changed.
#include "core/checked_math.hpp"
#include "core/sha256.hpp"
#include "frame/lzss_position_distance_8m_prepared_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_owned_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_stream_decoder.hpp"
#include "frame/lzss_position_distance_8m_stream_encoder.hpp"
#include "frame/lzss_position_distance_8m_token_frame_encoder.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace marc;
using namespace frame::internal;
using Token = dictionary::internal::LzssTypedToken;
using Op = context::internal::ModeledOperation;
using Old = LzssPositionDistance8mStreamEncoder;
using New = LzssPositionDistance8mOwnedStreamEncoder;
using Prepared = LzssPositionDistance8mPreparedStreamEncoder;
using Decode = LzssPositionDistance8mStreamDecoder;
using Clock = std::chrono::steady_clock;
constexpr std::size_t mib = 1u << 20;
constexpr std::size_t ceiling = 1024u * mib;
constexpr auto end = core::flag_value(core::ProcessFlags::end_input);
void require(bool b, const char *message) {
  if (!b)
    throw std::runtime_error(message);
}
std::size_t add(std::initializer_list<std::size_t> values) {
  std::size_t n{};
  for (auto v : values)
    require(core::checked_add(n, v, n), "addition");
  return n;
}
std::size_t mul(std::size_t a, std::size_t b) {
  std::size_t n{};
  require(core::checked_multiply(a, b, n), "multiplication");
  return n;
}
template <class T> std::size_t bytes(const std::vector<T> &v) {
  return mul(v.capacity(), sizeof(T));
}
void admit(std::size_t n) { require(n <= ceiling, "diagnostic policy"); }
struct Stats {
  std::size_t logical{}, block_peak{}, final_live{}, allocations{}, calls{}, written{};
};
struct Plan { std::size_t tokens{}, events{}, payload{}, prior{}, planner_allocated{}, planner_logical{}; };
using Digest = std::array<std::byte, core::sha256_digest_size>;
constexpr std::byte guard{0xa7};
constexpr std::array<std::array<unsigned,3>,6> orders{{
    {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}}};
Digest digest(std::span<const std::byte> input) {
  core::Sha256 hash;
  Digest result{};
  require(hash.update(input) && hash.finalize(result), "digest");
  return result;
}
void print_digest(const Digest &d) {
  constexpr char hex[]="0123456789abcdef";
  std::cout << "\"";
  for (auto b:d) { const auto n=std::to_integer<unsigned>(b); std::cout<<hex[n>>4]<<hex[n&15]; }
  std::cout << "\"";
}
struct Buffers {
  std::vector<std::byte> raw, publication, frame, payload;
  std::vector<Token> tokens, scratch;
  std::vector<Op> operations, operation_scratch;
  std::vector<std::uint32_t> index;
  auto workspace() {
    return LzssPositionDistance8mFrameEncodeWorkspace{
        tokens, scratch, index, operations, operation_scratch, frame, payload};
  }
  auto capacities(std::size_t in, std::size_t out) const {
    return LzssPositionDistance8mStreamEncodeCapacities{
        raw.size(),
        publication.size(),
        tokens.size(),
        scratch.size(),
        index.size(),
        operations.size(),
        operation_scratch.size(),
        frame.size(),
        payload.size(),
        in,
        out};
  }
  std::size_t full() const {
    return add({bytes(raw), bytes(publication), bytes(frame), bytes(payload),
                bytes(tokens), bytes(scratch), bytes(operations),
                bytes(operation_scratch), bytes(index)});
  }
  std::size_t sizes() const {
    return add({raw.size(), publication.size(), frame.size(), payload.size(),
                mul(tokens.size() + scratch.size(), sizeof(Token)),
                mul(operations.size() + operation_scratch.size(), sizeof(Op)),
                mul(index.size(), 4)});
  }
};
class Allocator final : public LzssPositionDistance8mStreamAllocator {
public:
  struct Receipt {
    const void *data{};
    std::size_t capacity{};
  };
  std::array<Receipt, 16> receipts{};
  LzssPositionDistance8mExactStreamAllocator exact;
  std::size_t live{}, peak{}, calls{}, fail_at{};
  bool reject() noexcept { return fail_at && calls + 1 == fail_at; }
  LzssPositionDistance8mAllocatorControls controls() const noexcept override {
    return {this, sizeof(*this), 256};
  }
  template <class T> auto keep(LzssPositionDistance8mOwnedBlock<T> b) noexcept {
    ++calls;
    if (b.data) {
      for (auto &r : receipts)
        if (!r.data) {
          r = {b.data, mul(b.capacity, sizeof(T))};
          live = add({live, r.capacity});
          peak = std::max(peak, live);
          return b;
        }
      std::terminate();
    }
    return b;
  }
  auto tokens(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedTokens override {
    return keep(reject() ? LzssPositionDistance8mOwnedTokens{} : exact.tokens(n));
  }
  auto bytes(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedBytes override {
    return keep(reject() ? LzssPositionDistance8mOwnedBytes{} : exact.bytes(n));
  }
  auto indices(std::size_t n) noexcept
      -> LzssPositionDistance8mOwnedIndex override {
    return keep(reject() ? LzssPositionDistance8mOwnedIndex{} : exact.indices(n));
  }
  template <class T>
  void destroy(LzssPositionDistance8mOwnedBlock<T> &b) noexcept {
    auto p = b.data;
    exact.release(b); // Real deletion precedes receipt removal.
    if (p) {
      for (auto &r : receipts)
        if (r.data == p) {
          live -= r.capacity;
          r = {};
          return;
        }
      std::terminate();
    }
  }
  void release(LzssPositionDistance8mOwnedTokens &b) noexcept override {
    destroy(b);
  }
  void release(LzssPositionDistance8mOwnedBytes &b) noexcept override {
    destroy(b);
  }
  void release(LzssPositionDistance8mOwnedIndex &b) noexcept override {
    destroy(b);
  }
};
// Named conservative reservations for all phase-local controls. These may
// exceed their physical union. Array storage, owners and call views are extra.
struct Controls {
  Buffers buffers;
  Allocator allocator;
  TypedContextFrameValidationContext context;
  LzssPositionDistance8mStreamEncodeCapacities capacities;
  LzssPositionDistance8mStreamEncodeRequirements encode_query;
  LzssPositionDistance8mStreamRequirements decode_query;
  LzssPositionDistance8mTokenFrameWorkspace token_workspace;
  LzssPositionDistance8mTokenFramePlan token_plan;
  dictionary::internal::LzssPositionDistance8mParsePlan parse_plan;
  core::ProcessResult result;
  std::array<std::size_t, 128> scalars;
  std::array<Clock::time_point, 8> times; // Reserved for the later timing gate; never read.
  core::Sha256 hash;
  std::array<Digest, 8> digests;
  // Concrete conservative reservation for SHA-256 transform/observer locals.
  std::array<std::uint32_t,128> hash_controls;
  std::array<std::byte,128> hash_bytes;
  std::array<std::span<const std::byte>, 4> inputs;
  std::array<std::span<std::byte>, 4> outputs;
};
struct Driver {
  core::DecoderLimits limits{};
  TypedContextStreamHeader header{};
  std::vector<std::byte> raw, reference, owned, prepared, decoded;
  Stats old_stats{}, new_stats{}, prepared_stats{};
  std::array<Stats,3> decode_stats{};
  std::array<Plan,3> plans{};
  Digest raw_digest{}, wire_digest{};
  std::size_t f{}, t{}, e{}, p{}, frame_count{}, wire{}, block_plan{}, final_plan{}, prospective_common{};
  std::size_t retained() const {
    return add({sizeof(*this), sizeof(Controls), bytes(raw), bytes(reference),
                bytes(owned), bytes(prepared), bytes(decoded)});
  }
  auto op_caps() const {
    return LzssPositionDistance8mStreamEncodeCapacities{
      f,add({80,p}),t,t,add({65536,f}),e,e,add({80,p}),p,raw.size(),wire};
  }
  std::size_t phase_peak(std::size_t common) const {
    auto q=query_lzss_position_distance_8m_stream_encode_workspace(limits,op_caps(),common);
    require(q.error==core::ErrorCode::none,"prospective operation admission");
    auto peak=q.aggregate_bytes;
    for(auto working:{New::working_bytes(),Prepared::working_bytes()})
      peak=std::max(peak,add({common,block_plan,working,sizeof(Allocator),256,raw.size(),wire}));
    auto d=query_lzss_position_distance_8m_stream_workspace(limits,add({80,p}),t,t,f,f,
        add({common,wire,raw.size()}));
    require(d.error==core::ErrorCode::none,"prospective decoder admission");
    return std::max(peak,d.aggregate_bytes);
  }
  void prepare(std::size_t n, unsigned pattern, unsigned kind) {
    require(n == mib || n == 8*mib,"frame size");
    require(pattern<4 && kind<3 && (!kind || !pattern),"recipe");
    f=n; frame_count=kind==0?1:kind==1?2:3;
    limits.max_block_size=8*mib; limits.max_lz_distance=8*mib;
    limits.max_internal_buffered_bytes=ceiling;
    header.frame_size=static_cast<std::uint32_t>(f);
    header.original_size=add({mul(f,kind?2:1),kind==2?32u:0u});
    header.dictionary={8*mib,3,258,0}; header.dictionary_variant=11;
    header.context_algorithm=1; header.context_variant=12; header.context_count=47;
    header.range_model_total=32768;
    admit(add({retained(),static_cast<std::size_t>(header.original_size)}));
    raw.resize(static_cast<std::size_t>(header.original_size));
    admit(retained());
    std::uint32_t random=1439;
    for(std::size_t i=0;i<f;++i) {
      random^=random<<13; random^=random>>17; random^=random<<5;
      if(pattern==0)raw[i]=std::byte(i%256);
      else if(pattern==1)raw[i]=i<mib+17?std::byte(random&255):raw[i%(mib+17)];
      else if(pattern==2)raw[i]=std::byte(random&255);
      else raw[i]=std::byte((i/65536)%3==0?(random&255):i%7);
    }
    if(kind)std::copy_n(raw.begin(),f,raw.begin()+f);
    if(kind==2)std::copy_n(raw.begin(),32,raw.begin()+2*f);
    wire=112;
    std::size_t prior{},previous{},generation_peak{};
    for(std::size_t j=0;j<frame_count;++j) {
      // Each actual frame is independently planned with its real sequence/prior.
      const auto length=std::min(f,raw.size()-prior);
      const auto input=std::span<const std::byte>(raw).subspan(prior,length);
      Buffers b;
      auto planner_extra=add({retained(),raw.size()-length});
      admit(add({planner_extra,mul(add({65536,length}),4),
          lzss_position_distance_8m_token_frame_working_bytes(),length}));
      b.index.resize(add({65536,length}));
      admit(add({planner_extra,b.full(),
          lzss_position_distance_8m_token_frame_working_bytes(),length}));
      auto count=dictionary::internal::query_lzss_position_distance_8m_indexed(
          input,header.dictionary,limits,0,0,b.index,planner_extra);
      require(count.error==dictionary::internal::LzssPositionDistance8mParseError::output_too_small,"token count");
      const auto tj=count.details.token_count;
      require(tj<=length,"token bound");
      admit(add({planner_extra,b.full(),mul(24,tj),
          lzss_position_distance_8m_token_frame_working_bytes(),length}));
      b.tokens.resize(tj); b.scratch.resize(tj);
      admit(add({planner_extra,b.full(),lzss_position_distance_8m_token_frame_working_bytes(),length}));
      auto context=TypedContextFrameValidationContext{header,limits,j,prior};
      auto w=LzssPositionDistance8mTokenFrameWorkspace{b.tokens,b.scratch,b.index,{},{}};
      auto plan=query_lzss_position_distance_8m_token_frame_encode(input,context,w,0,
          add({planner_extra,b.full()-b.sizes()}));
      require(plan.error==LzssPositionDistance8mTokenFrameError::storage_too_small && plan.bytes_required>=80,"payload count");
      plans[j]={tj,plan.counts.declared_event_count,plan.bytes_required-80,prior,
          b.full(),add({planner_extra,b.full(),
              lzss_position_distance_8m_token_frame_working_bytes(),length})};
      require(plans[j].payload<=limits.max_compressed_payload_size && plans[j].events<=mul(2,length),"plan bounds");
      t=std::max(t,tj); e=std::max(e,plans[j].events); p=std::max(p,plans[j].payload);
      wire=add({wire,plan.bytes_required});
      const auto generation=add({mul(24,tj),mul(3,plans[j].payload),160});
      generation_peak=std::max(generation_peak,add({previous,generation}));
      previous=generation; prior=add({prior,length});
    } // All planning allocations have really been destroyed.
    const auto initial=add({f,mul(add({65536,f}),4)});
    block_plan=add({initial,generation_peak}); final_plan=add({initial,previous});
    prospective_common=add({retained(),mul(3,add({wire,16})),raw.size(),16});
    admit(phase_peak(prospective_common)); // Joint admission BEFORE destinations.
    reference.assign(add({wire,16}),guard); owned.assign(add({wire,16}),guard);
    prepared.assign(add({wire,16}),guard); decoded.assign(add({raw.size(),16}),guard);
    admit(phase_peak(retained())); // Actual full capacities, no capacity discount.
    raw_digest=digest(raw);
  }
  void check_guard(const std::vector<std::byte>& v,std::size_t n) const {
    require(v.size()==add({n,16}) && std::ranges::all_of(std::span(v).subspan(n),
        [](std::byte b){return b==guard;}),"guard");
  }
  template <class Transform>
  std::size_t drive(Transform &x, std::span<const std::byte> input,
                    std::span<std::byte> output, Stats &s) {
    std::size_t i{}, o{};
    for (std::size_t k = 0; k < 10000; ++k) {
      auto r = x.process(input.subspan(i), output.subspan(o), end);
      require(r.input_consumed <= input.size() - i &&
                  r.output_produced <= output.size() - o,
              "counts");
      i += r.input_consumed;
      o += r.output_produced;
      ++s.calls;
      require(r.status != core::StreamStatus::progress || r.input_consumed ||
                  r.output_produced,
              "zero progress");
      require(r.status != core::StreamStatus::error, "process failed");
      if (r.status == core::StreamStatus::end_of_stream) {
        require(i == input.size() && o == output.size(), "complete sizes");
        auto again = x.process({}, {}, 0xffffffffu);
        require(again.status == core::StreamStatus::end_of_stream &&
                    !again.input_consumed && !again.output_produced,
                "terminal");
        return o;
      }
      require(r.status != core::StreamStatus::need_input || i < input.size(),
              "unexpected starvation");
      require(r.status != core::StreamStatus::need_output || o < output.size(),
              "unexpected output exhaustion");
    }
    throw std::runtime_error("call guard");
  }
  void old_encode() {
    const auto caps = op_caps();
    auto q = query_lzss_position_distance_8m_stream_encode_workspace(
        limits, caps, retained());
    require(q.error == core::ErrorCode::none, "reference admission");
    admit(q.aggregate_bytes);
    {
      Buffers b;
      b.raw.resize(f);
      b.publication.resize(80 + p);
      b.frame.resize(80 + p);
      b.payload.resize(p);
      b.tokens.resize(t);
      b.scratch.resize(t);
      b.operations.resize(e);
      b.operation_scratch.resize(e);
      b.index.resize(65536 + f);
      require(b.full()>=b.sizes(),"workspace spare subtraction");
      auto external = add({retained(), b.full() - b.sizes()});
      q = query_lzss_position_distance_8m_stream_encode_workspace(
          limits, b.capacities(raw.size(), wire), external);
      require(q.error == core::ErrorCode::none, "reference actual capacities");
      admit(q.aggregate_bytes);
      old_stats.logical = q.aggregate_bytes;
      old_stats.block_peak = b.full();
      old_stats.final_live = b.full();
      Old x(header, limits, b.raw, b.publication, b.workspace(), external);
      old_stats.written = drive(x, raw, std::span(reference).first(wire), old_stats);

    }

  }
  template<class Path> void owner_encode(std::vector<std::byte>& destination,Stats& stats) {
    stats.logical=add({retained(),block_plan,Path::working_bytes(),sizeof(Allocator),256,raw.size(),wire});
    admit(stats.logical);
    Allocator a;
    {
      Path x(header,limits,a,retained());
      stats.written=drive(x,raw,std::span(destination).first(wire),stats);
      stats.block_peak=a.peak; stats.final_live=a.live; stats.allocations=a.calls;
      require(a.peak==block_plan && a.live==final_plan,"owner actual ledger");
    }
    require(a.live==0 && std::ranges::all_of(a.receipts,[](auto&r){return !r.data;}),"real destruction receipts");
  }
  void decode(const std::vector<std::byte>& source,Stats& stats) {
    // ALL encoders are already gone. Every consumer has a fresh workspace.
    auto extra=add({retained(),wire,raw.size()});
    auto q=query_lzss_position_distance_8m_stream_workspace(limits,add({80,p}),t,t,f,f,extra);
    require(q.error==core::ErrorCode::none,"decoder admission"); admit(q.aggregate_bytes);
    {
      Buffers b;
      b.publication.resize(add({80,p})); b.tokens.resize(t); b.scratch.resize(t);
      b.raw.resize(f); b.frame.resize(f);
      extra=add({extra,b.full()-b.sizes()});
      q=query_lzss_position_distance_8m_stream_workspace(limits,b.publication.size(),t,t,f,f,extra);
      require(q.error==core::ErrorCode::none,"decoder actual capacities"); admit(q.aggregate_bytes);
      stats.logical=q.aggregate_bytes; stats.block_peak=b.full(); stats.final_live=b.full();
      Decode x(limits,b.publication,b.tokens,b.scratch,b.raw,b.frame,extra);
      stats.written=drive(x,std::span(source).first(wire),std::span(decoded).first(raw.size()),stats);
    }
    require(std::ranges::equal(raw,std::span(decoded).first(raw.size())),"raw bytes");
    require(digest(std::span(decoded).first(raw.size()))==raw_digest,"raw digest");
    check_guard(source,wire); check_guard(decoded,raw.size());
  }
  void run(unsigned order) {
    require(order<orders.size(),"order");
    for(auto path:orders[order]) {
      if(path==0)old_encode();
      else if(path==1)owner_encode<New>(owned,new_stats);
      else owner_encode<Prepared>(prepared,prepared_stats);
    }
    require(std::ranges::equal(reference,owned) && std::ranges::equal(reference,prepared),"wire bytes");
    wire_digest=digest(std::span(reference).first(wire));
    require(digest(std::span(owned).first(wire))==wire_digest &&
        digest(std::span(prepared).first(wire))==wire_digest,"wire digest");
    decode(reference,decode_stats[0]); decode(owned,decode_stats[1]); decode(prepared,decode_stats[2]);
  }

};
void print(const char* name,const Stats& s) {
  std::cout<<"\""<<name<<"\":{\"logical_reservation\":"<<s.logical
    <<",\"allocation_peak\":"<<s.block_peak<<",\"final_live\":"<<s.final_live
    <<",\"allocations\":"<<s.allocations<<",\"calls\":"<<s.calls<<",\"written\":"<<s.written<<"}";
}
template<class Path> void faults() {
  // Small cyclic fixture isolates initial and each candidate allocation failure.
  Driver d;
  d.f=64; d.header.frame_size=64; d.header.original_size=64;
  d.header.dictionary={8*mib,3,258,0}; d.header.dictionary_variant=11;
  d.header.context_algorithm=1; d.header.context_variant=12;
  d.header.context_count=47; d.header.range_model_total=32768;
  d.limits.max_block_size=8*mib; d.limits.max_lz_distance=8*mib;
  d.limits.max_internal_buffered_bytes=ceiling;
  d.raw.resize(64); for(std::size_t i=0;i<64;++i)d.raw[i]=std::byte(i);
  d.prepared.assign(2048,guard);
  for(std::size_t stage=1;stage<=7;++stage) {
    Allocator a; a.fail_at=stage;
    {
      Path x(d.header,d.limits,a,d.retained());
      auto r=x.process(d.raw,std::span(d.prepared).first(2032),end);
      require(r.status==core::StreamStatus::error,"injected allocation failure");
      require(r.output_produced<=112,"failed frame not published");
      require(std::ranges::all_of(std::span(d.prepared).subspan(r.output_produced),
          [](std::byte b){return b==guard;}),"failure guard");
      auto again=x.process({}, {},end);
      require(again.status==core::StreamStatus::error && !again.input_consumed && !again.output_produced,"sticky failure");
    }
    require(a.live==0 && std::ranges::all_of(a.receipts,[](auto&r){return !r.data;}),"fault real release");
    std::fill(d.prepared.begin(),d.prepared.end(),guard);
  }
  const auto initial=add({64,mul(65536+64,4),Path::working_bytes(),sizeof(Allocator),256,d.retained()});
  for(bool below:{false,true}) {
    Allocator a; auto limits=d.limits;
    limits.max_internal_buffered_bytes=initial-(below?1:0);
    limits.max_block_size=64;
    {
      Path x(d.header,limits,a,d.retained());
      require(a.calls==(below?0u:2u),"initial threshold admission before allocation");
      if(below) {
        auto r=x.process({}, {},end);
        require(r.status==core::StreamStatus::error && !r.input_consumed && !r.output_produced,"one below rejected");
      }
    }
    require(!a.live && std::ranges::all_of(a.receipts,[](auto&r){return !r.data;}),"threshold release");
  }
}
} // namespace
int main(int argc,char** argv) {
  try {
    if(argc==2 && std::string_view(argv[1])=="--audit-probe") {
      std::cout<<"untimed audit probe ready"<<std::endl; std::cin.get(); return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--faults") {
      faults<New>(); faults<Prepared>();
      std::cout<<"{\"untimed\":true,\"allocation_faults\":14,\"threshold_checks\":4,\"passed\":true}\n"; return 0;
    }
    require(argc==6 && std::string_view(argv[1])=="--untimed","arguments: --untimed frame_bytes pattern kind order");
    Driver d;
    const auto pattern=static_cast<unsigned>(std::stoul(argv[3]));
    const auto kind=static_cast<unsigned>(std::stoul(argv[4]));
    const auto order=static_cast<unsigned>(std::stoul(argv[5]));
    d.prepare(std::stoull(argv[2]),pattern,kind); d.run(order);
    std::cout<<"{\"untimed\":true,\"frame_bytes\":"<<d.f<<",\"pattern\":"<<pattern
      <<",\"kind\":"<<kind<<",\"order\":"<<order<<",\"raw_bytes\":"<<d.raw.size()
      <<",\"wire_bytes\":"<<d.wire<<",\"ceiling\":"<<ceiling
      <<",\"common\":"<<d.retained()<<",\"prospective_common\":"<<d.prospective_common
      <<",\"driver_size\":"<<sizeof(Driver)<<",\"controls_size\":"<<sizeof(Controls)
      <<",\"allocator_size\":"<<sizeof(Allocator)<<",\"safe_working\":"<<New::working_bytes()
      <<",\"prepared_working\":"<<Prepared::working_bytes()<<",\"wire_equal\":true,\"raw_equal\":true,\"raw_sha256\":";
    print_digest(d.raw_digest); std::cout<<",\"wire_sha256\":"; print_digest(d.wire_digest);
    std::cout<<",\"plans\":[";
    for(std::size_t j=0;j<d.frame_count;++j) { if(j)std::cout<<",";
      const auto &p=d.plans[j];
      std::cout<<"{\"sequence\":"<<j<<",\"prior\":"<<p.prior<<",\"tokens\":"<<p.tokens
        <<",\"events\":"<<p.events<<",\"payload\":"<<p.payload
        <<",\"planner_allocated\":"<<p.planner_allocated
        <<",\"planner_logical\":"<<p.planner_logical<<"}"; }
    std::cout<<"],"; print("operation",d.old_stats); std::cout<<","; print("safe",d.new_stats);
    std::cout<<","; print("prepared",d.prepared_stats);
    for(unsigned i=0;i<3;++i) { std::cout<<",";print(i==0?"decode_operation":i==1?"decode_safe":"decode_prepared",d.decode_stats[i]); }
    std::cout<<"}\n"; return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
