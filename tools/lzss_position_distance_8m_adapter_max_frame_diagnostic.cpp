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
#include "frame/lzss_position_distance_8m_owning_adapter.hpp"
#include "lzss_position_distance_8m_owning_allocator_seam.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
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
using Adapter = LzssPositionDistance8mOwningAdapter;
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
struct Plan { std::size_t tokens{}, events{}, payload{}, prior{}, planner_allocated{}, planner_logical{}, literals{}, matches{}, match_bytes{}; };
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
  std::array<LzssPositionDistance8mOwningConfig,8> adapter_configs;
  std::array<LzssPositionDistance8mOwningRequirements,8> adapter_queries;
  std::array<Adapter::Ledger,8> adapter_ledgers;
  std::array<test::owning8m::Controller,2> delegate_controls;
  std::array<std::byte,4096> separate_delegate_working;
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
  std::size_t adapter_fixed{}, adapter_initial{}, exact_budget{}, refusal_written{}, refusal_consumed{}, refusal_position{}, refusal_live{}, refusal_calls{}, delegate_calls{}, delegate_deleted{}, delegate_peak{};
  unsigned refusal_code{}, rejected_configs{};
  bool isolated{};
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
    auto config=LzssPositionDistance8mOwningConfig{header,limits,common,raw.size(),wire};
    const auto a=Adapter::query(config);
    require(a.error==core::ErrorCode::none,"prospective adapter initial admission");
    const auto adapter_total=add({a.fixed_bytes,block_plan});
    admit(adapter_total);
    return std::max({peak,d.aggregate_bytes,adapter_total});
  }
  void prepare(std::size_t n, unsigned pattern, unsigned kind) {
    require(n == 8*mib,"maximum frame size");
    require((pattern==0 || pattern==2) && (kind==0 || kind==2),"recipe");
    f=n; frame_count=kind==0?1:kind==1?2:3;
    limits.max_frame_size=limits.max_block_size=8*mib; limits.max_lz_distance=8*mib;
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
      for(const auto &token:b.tokens) {
        if(token.kind==dictionary::internal::LzssTypedTokenKind::literal)++plans[j].literals;
        else if(token.kind==dictionary::internal::LzssTypedTokenKind::match) {
          ++plans[j].matches;plans[j].match_bytes=add({plans[j].match_bytes,token.length});
        } else require(false,"composition kind");
      }
      require(plans[j].literals+plans[j].matches==tj &&
              plans[j].literals+plans[j].match_bytes==length,"composition expansion");
      if(length==f) require(pattern==2 ? plans[j].literals>3*(f/4) : plans[j].match_bytes>3*(f/4),"observed recipe composition");
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

  void adapter_encode() {
    auto c=LzssPositionDistance8mOwningConfig{header,limits,retained(),raw.size(),wire};
    const auto q=Adapter::query(c);require(q.error==core::ErrorCode::none,"adapter initial query");
    adapter_fixed=q.fixed_bytes;adapter_initial=q.initial_bytes;
    exact_budget=add({q.fixed_bytes,block_plan});admit(exact_budget);
    prepared_stats.logical=exact_budget;
    auto run_positive=[&](std::size_t budget) {
      auto config=c;config.limits.max_internal_buffered_bytes=budget;
      test::owning8m::Controller controller;
      {test::owning8m::Scope scope(controller);
        Adapter x(config);
        require(x.ledger().live==add({f,mul(65536+f,4)}),"adapter initial ledger");
        prepared_stats.calls=0;
        prepared_stats.written=drive(x,raw,std::span(prepared).first(wire),prepared_stats);
        const auto ledger=x.ledger();
        require(ledger.peak==block_plan && ledger.live==final_plan && ledger.calls==add({2,mul(5,frame_count)}),"adapter actual full capacities");
        require(ledger.blocks==7,"adapter retained final generation");
        prepared_stats.block_peak=ledger.peak;prepared_stats.final_live=ledger.live;prepared_stats.allocations=ledger.calls;
        if(isolated)require(controller.valid && controller.live==ledger.live && controller.peak==ledger.peak && controller.calls==ledger.calls,"independent delegate ledger");
      }
      require(controller.valid && controller.live==0 && std::ranges::all_of(controller.records,[](const auto&r){return !r.p;}),"real adapter destruction");
      if(isolated)require(controller.deleted==controller.calls,"real delegate releases");
      delegate_calls=controller.calls;delegate_deleted=controller.deleted;delegate_peak=controller.peak;
      check_guard(prepared,wire);
    };
    run_positive(ceiling);
    require(std::ranges::equal(reference,prepared),"adapter broad wire equality");
    std::fill(prepared.begin(),prepared.end(),guard);
    run_positive(exact_budget);
    require(std::ranges::equal(reference,prepared),"adapter exact wire equality");
    // One byte below the complete adjacent-generation peak. All external
    // owners and call views remain charged at their original full capacities.
    std::fill(prepared.begin(),prepared.end(),guard);
    auto limited=c;limited.limits.max_internal_buffered_bytes=exact_budget-1;
    test::owning8m::Controller controller;
    {
      test::owning8m::Scope scope(controller);Adapter x(limited);
      auto r=x.process(raw,std::span(prepared).first(wire),end);
      require(r.status==core::StreamStatus::error,"below generation ceiling refusal");
      const auto target=frame_count==1?0u:1u;
      auto boundary=112u;for(unsigned j=0;j<target;++j)boundary+=80+plans[j].payload;
      require(r.output_produced==boundary && r.input_consumed==mul(target+1,f) &&
          r.error.byte_position==mul(target,f) && r.error.bit_position==0,"below ceiling valid prefix counts");
      require(std::equal(reference.begin(),reference.begin()+boundary,prepared.begin()),"below ceiling prefix bytes");
      require(std::ranges::all_of(std::span(prepared).subspan(boundary),[](auto b){return b==guard;}),"failed frame guard");
      refusal_written=r.output_produced;refusal_consumed=r.input_consumed;refusal_position=r.error.byte_position;refusal_code=static_cast<unsigned>(r.error.code);
      const auto ledger=x.ledger();refusal_live=ledger.live;refusal_calls=ledger.calls;
      const auto initial=add({f,mul(65536+f,4)});
      const auto old=target?add({mul(24,plans[0].tokens),mul(3,plans[0].payload),160}):0;
      require(ledger.live==add({initial,old}),"below ceiling old generation retained");
      if(isolated)require(controller.valid && controller.live==ledger.live,"refusal independent ledger");
      const auto again=x.process({},std::span(prepared).first(wire),0xffffffffu);
      require(again.status==core::StreamStatus::error && again.error.code==r.error.code &&
              again.error.byte_position==r.error.byte_position && !again.input_consumed && !again.output_produced,"below ceiling sticky");
    }
    require(controller.valid && !controller.live && std::ranges::all_of(controller.records,[](const auto&r){return !r.p;}),"refusal real destruction");
    // These are independent stream-limit checks, not late-fault injection.
    for(unsigned mode=0;mode<2;++mode) {
      auto invalid=c;
      if(mode==0)invalid.limits.max_frame_size=f-1;else invalid.limits.max_block_size=f-1;
      test::owning8m::Controller observed;
      {test::owning8m::Scope scope(observed);Adapter x(invalid);
        std::fill(prepared.begin(),prepared.end(),guard);
        auto r=x.process(raw,std::span(prepared).first(wire),end);
        require(r.status==core::StreamStatus::error && r.output_produced<=112,"configured frame/block limit refusal");
        require(std::ranges::all_of(std::span(prepared).subspan(r.output_produced),[](auto b){return b==guard;}),"configured limit no frame publication");
        if(mode==0)require(!x.ledger().calls && !observed.calls,"frame refusal before allocation");
      }
      require(observed.valid && !observed.live && std::ranges::all_of(observed.records,[](const auto&r){return !r.p;}),"configured refusal real destruction");
      ++rejected_configs;
    }
    // Reconstruct complete valid bytes for the unchanged decoder. Failed
    // output is never treated as a complete stream.
    std::fill(prepared.begin(),prepared.end(),guard);
    run_positive(exact_budget);
    require(std::ranges::equal(reference,prepared),"final adapter complete wire");
  }
  void run(unsigned order) {
    require(order==0,"fixed untimed order");
    for(auto path:orders[order]) {
      if(path==0)old_encode();
      else if(path==1)owner_encode<New>(owned,new_stats);
      else adapter_encode();
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

} // namespace
int main(int argc,char** argv) {
  try {
    require(argc==4,"arguments: pattern kind ordinary-or-isolated");
    Driver d;
    const auto pattern=static_cast<unsigned>(std::stoul(argv[1]));
    const auto kind=static_cast<unsigned>(std::stoul(argv[2]));
    const std::string_view role(argv[3]);require(role=="ordinary" || role=="isolated","role");
    d.isolated=role=="isolated";d.prepare(8*mib,pattern,kind);d.run(0);
    std::cout<<"{\"passed\":true,\"untimed\":true,\"isolated\":"<<(d.isolated?"true":"false")
      <<",\"frame_bytes\":"<<d.f<<",\"pattern\":"<<pattern<<",\"kind\":"<<kind
      <<",\"frames\":"<<d.frame_count<<",\"input_bytes\":"<<d.raw.size()<<",\"wire_bytes\":"<<d.wire
      <<",\"driver_bytes\":"<<sizeof(Driver)<<",\"controls_bytes\":"<<sizeof(Controls)
      <<",\"common\":"<<d.retained()<<",\"prospective_common\":"<<d.prospective_common
      <<",\"block_plan\":"<<d.block_plan<<",\"final_plan\":"<<d.final_plan
      <<",\"adapter_fixed\":"<<d.adapter_fixed<<",\"adapter_initial\":"<<d.adapter_initial
      <<",\"exact_budget\":"<<d.exact_budget<<",\"refusal_written\":"<<d.refusal_written
      <<",\"refusal_consumed\":"<<d.refusal_consumed<<",\"refusal_position\":"<<d.refusal_position
      <<",\"refusal_code\":"<<d.refusal_code<<",\"refusal_live\":"<<d.refusal_live
      <<",\"refusal_calls\":"<<d.refusal_calls<<",\"rejected_configs\":"<<d.rejected_configs
      <<",\"delegate_calls\":"<<d.delegate_calls<<",\"delegate_deleted\":"<<d.delegate_deleted
      <<",\"delegate_peak\":"<<d.delegate_peak<<",\"delegate_live_after_destroy\":0,\"raw_sha256\":";
    print_digest(d.raw_digest);std::cout<<",\"wire_sha256\":";print_digest(d.wire_digest);
    std::cout<<",\"plans\":[";
    for(std::size_t j=0;j<d.frame_count;++j){if(j)std::cout<<",";const auto &p=d.plans[j];
      std::cout<<"{\"sequence\":"<<j<<",\"prior\":"<<p.prior<<",\"tokens\":"<<p.tokens
        <<",\"events\":"<<p.events<<",\"payload\":"<<p.payload<<",\"literals\":"<<p.literals
        <<",\"matches\":"<<p.matches<<",\"match_bytes\":"<<p.match_bytes
        <<",\"planner_allocated\":"<<p.planner_allocated<<",\"planner_logical\":"<<p.planner_logical<<"}";}
    std::cout<<"],";print("operation",d.old_stats);std::cout<<",";print("safe",d.new_stats);
    std::cout<<",";print("adapter",d.prepared_stats);
    for(unsigned i=0;i<3;++i){std::cout<<",";print(i==0?"decode_operation":i==1?"decode_safe":"decode_adapter",d.decode_stats[i]);}
    std::cout<<"}\n";return 0;
  }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
