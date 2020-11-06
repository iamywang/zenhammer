#include "PatternBuilder.h"

#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "DramAnalyzer.h"
#include "GlobalDefines.h"
#include "utils.h"

PatternBuilder::PatternBuilder(int num_activations, volatile char* target_address)
    : num_activations(num_activations), target_addr(target_address) {
  randomize_parameters();
}

void PatternBuilder::randomize_parameters() {
  // STATIC FUZZING PARAMETERS
  // those parameters are only randomly selected when calling randomize_parameters
  printf(FCYAN "[+] Generating new set of random parameters: ");
  num_hammering_pairs = Range(3, 26).get_random_number();
  agg_inter_distance = Range(1, 16).get_random_number();
  agg_intra_distance = Range(2, 2).get_random_number();

  // must always be >=2 because we use two NOPs for hammering synchronization
  num_nops = Range(2, 2).get_random_number();

  agg_rounds = num_activations / (num_hammering_pairs * 2);
  auto hammer_rounds = Range(850000, 1150000).get_random_number();
  // num_refresh_intervals = hammer_rounds / agg_rounds;
  num_refresh_intervals = Range(75, 80).get_random_number();

  random_start_address = target_addr + MB(100) + (((rand() % (MEM_SIZE - MB(200)))) / PAGE_SIZE) * PAGE_SIZE;

  printf("num_hammering_pairs: %d, ", num_hammering_pairs);
  printf("agg_inter_distance: %d, ", agg_inter_distance);
  printf("agg_intra_distance: %d, ", agg_intra_distance);
  printf("num_nops: %d, ", num_nops);
  printf("num_refresh_intervals: %d", num_refresh_intervals);
  printf(NONE "\n");

  // DYNAMIC FUZZING PARAMETERS
  // these parameters specify ranges of valid values that are then randomly determined while generating the pattern
  multiplicator_hammering_pairs = Range(4, 12);
  multiplicator_nops = Range(1, 4);
}

int PatternBuilder::get_total_duration_pi(int num_ref_intervals) {
  return num_ref_intervals * duration_full_refresh;
}

/// This method hammers using a randomly generated pattern and then counts the number of NOP accesses that were required
/// until the next REFRESH. This number is then minimized by iteratively removing accesses, or phrased differently,
/// by reducing the length of the pattern such that it perfectly fits into a REFRESH interval.
void PatternBuilder::hammer_and_improve_params() {
  printf("[+] Hammering using jitted code...\n");

  // the maximum number of optimization iterations
  const int max_optimization_rounds = 25;
  int optimization_rounds = 0;

  // the number of activations after hammering until the next REFRESH; note that this is the number over all
  // num_refresh_intervals repetitions, thus must be divided to get the average of activations
  int num_total_acts_trailing_sync;

  int num_activations_after_last_refresh;

  int num_activations_to_next_refresh;

  do {
    optimization_rounds++;
    num_total_acts_trailing_sync = fn();
    num_activations_after_last_refresh = (num_total_acts_trailing_sync / num_refresh_intervals) % num_activations;
    num_activations_to_next_refresh = num_activations - num_activations_after_last_refresh;
    printf("avg #acts to prev. REFRESH: %d\n", num_activations_to_next_refresh);
    for (int i = 0; i < (num_activations_to_next_refresh / (2 * optimization_rounds)); ++i) aggressor_pairs.pop_back();
    printf("[+] Removed one aggressor (now: %zu) and rebuilding pattern now.\n", aggressor_pairs.size());
    rt.release(fn);
    jit_hammering_code(agg_rounds, num_refresh_intervals);
  } while (num_activations_to_next_refresh > 10 && optimization_rounds < max_optimization_rounds);
  printf("[+] Used %d iterations to improve hammering pattern (avg #acts: %d).\n",
         optimization_rounds, num_total_acts_trailing_sync / num_refresh_intervals);
}

void PatternBuilder::cleanup_and_rerandomize() {
  rt.release(fn);
  aggressor_pairs.clear();
  nops.clear();
  randomize_parameters();
}

void PatternBuilder::get_random_indices(size_t max, size_t num_indices, std::vector<size_t>& indices) {
  // use all numbers in range (0, ..., num_indices-1 = max) if there is only this one possibility
  indices.resize(num_indices);
  if (max == (num_indices - 1)) {
    std::iota(indices.begin(), indices.end(), 0);
    return;
  }
  // use random numbers between [0, num_indices-1] where num_indices-1 < max
  // use a set to avoid adding the same number multiple times
  std::set<size_t> nums;
  while (nums.size() < num_indices) {
    int candidate = rand() % max;
    if (nums.count(candidate) > 0) continue;
    nums.insert(candidate);
  }
  indices.insert(indices.end(), nums.begin(), nums.end());
}

void PatternBuilder::jit_hammering_code(size_t agg_rounds, uint64_t hammering_intervals) {
  logger = new asmjit::StringLogger;
  asmjit::CodeHolder code;
  code.init(rt.environment());
  code.setLogger(logger);
  asmjit::x86::Assembler a(&code);

  asmjit::Label while1_begin = a.newLabel();
  asmjit::Label while1_end = a.newLabel();
  asmjit::Label for1_begin = a.newLabel();
  asmjit::Label for1_end = a.newLabel();
  asmjit::Label while2_begin = a.newLabel();
  asmjit::Label while2_end = a.newLabel();

  // ==== here start's the actual program ====================================================
  // The following JIT instructions are based on hammer_sync in blacksmith.cpp, git commit 624a6492.

  // ------- part 1: synchronize with the beginning of an interval ---------------------------

  // access first two NOPs as part of synchronization
  if (nops.size() < 2) fprintf(stderr, "[-] Hammering requires at least 2 NOPs for synchronization.\n");
  std::vector<size_t> random_indices;
  get_random_indices(1, 2, random_indices);
  for (const auto& idx : random_indices) {
    a.mov(asmjit::x86::rax, (uint64_t)nops[idx]);
    asmjit::x86::Mem m = asmjit::x86::ptr(asmjit::x86::rax);
    a.mov(asmjit::x86::rbx, m);
  }

  // while (true) { ...
  a.bind(while1_begin);
  // clflushopt both NOPs
  for (const auto& idx : random_indices) {
    a.mov(asmjit::x86::rax, (uint64_t)nops[idx]);
    a.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
  }
  a.mfence();

  a.rdtscp();  // result of rdtscp is in [edx:eax]
  a.lfence();
  // discard upper 32 bits and store lower 32 bits in ebx to compare later
  a.mov(asmjit::x86::ebx, asmjit::x86::eax);

  // access both NOPs once
  for (const auto& idx : random_indices) {
    a.mov(asmjit::x86::rax, (uint64_t)nops[idx]);
    a.mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rax));
  }

  a.rdtscp();  // result: edx:eax
  // if ((after - before) > 1000) break;
  a.sub(asmjit::x86::eax, asmjit::x86::ebx);
  a.cmp(asmjit::x86::eax, (uint64_t)1000);
  // depending on the cmp's outcome, jump out of loop or to the loop's beginning
  a.jg(while1_end);
  a.jmp(while1_begin);

  a.bind(while1_end);

  // ------- part 2: perform hammering and then check for next ACTIVATE ---------------------------

  a.mov(asmjit::x86::rsi, hammering_intervals);  // loop counter
  a.mov(asmjit::x86::edx, 0);

  // instead of "HAMMER_ROUNDS / ref_rounds" we use "hammering_intervals" which does the same but randomizes the
  // HAMMER_ROUNDS parameter
  a.bind(for1_begin);
  a.cmp(asmjit::x86::rsi, 0);
  a.jz(for1_end);
  a.dec(asmjit::x86::rsi);

  // as agg_rounds is typically a relatively low number, we do not encode the loop in ASM but instead
  // unroll the instructions to avoid the additional jump the loop would cause

  // hammering loop: for (int j = 0; j < agg_rounds; j++) { ... }
  for (size_t i = 0; i < agg_rounds; i++) {
    for (size_t i = 0; i < aggressor_pairs.size(); i++) {
      a.mov(asmjit::x86::rax, (uint64_t)aggressor_pairs[i]);
      a.mov(asmjit::x86::rbx, asmjit::x86::ptr(asmjit::x86::rax));
    }
    for (size_t i = 0; i < aggressor_pairs.size(); i++) {
      a.mov(asmjit::x86::rax, aggressor_pairs[i]);
      a.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
    }
    a.mfence();
  }

  // loop for synchronization after hammering: while (true) { ... }
  a.bind(while2_begin);
  // clflushopt both NOPs
  for (const auto& idx : random_indices) {
    a.mov(asmjit::x86::rax, (uint64_t)nops[idx]);
    a.clflushopt(asmjit::x86::ptr(asmjit::x86::rax));
  }
  a.mfence();
  a.lfence();

  a.push(asmjit::x86::edx);
  a.rdtscp();  // result of rdtscp is in [edx:eax]
  // discard upper 32 bits and store lower 32 bits in ebx to compare later
  a.mov(asmjit::x86::ebx, asmjit::x86::eax);
  a.lfence();
  a.pop(asmjit::x86::edx);

  // access both NOPs once
  for (const auto& idx : random_indices) {
    a.mov(asmjit::x86::rax, (uint64_t)nops[idx]);
    a.mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::rax));
    a.inc(asmjit::x86::edx);
  }

  a.push(asmjit::x86::edx);
  a.rdtscp();  // result: edx:eax
  a.lfence();
  a.pop(asmjit::x86::edx);
  // if ((after - before) > 1000) break;
  a.sub(asmjit::x86::eax, asmjit::x86::ebx);
  a.cmp(asmjit::x86::eax, (uint64_t)1000);

  // depending on the cmp's outcome, jump out of loop or to the loop's beginning
  a.jg(while2_end);
  a.jmp(while2_begin);

  a.bind(while2_end);
  a.jmp(for1_begin);

  a.bind(for1_end);

  a.mov(asmjit::x86::eax, asmjit::x86::edx);
  a.ret();  // this is ESSENTIAL otherwise execution of jitted code creates segfault

  // add the generated code to the runtime.
  asmjit::Error err = rt.add(&fn, &code);
  if (err) throw std::runtime_error("[-] Error occurred when trying to jit code. Aborting execution!");

  // uncomment the following line to see the jitted ASM code
  // printf("[DEBUG] asmjit logger content:\n%s\n", logger->data());
}

std::pair<volatile char*, volatile char*> PatternBuilder::generate_random_pattern(
    std::vector<uint64_t> bank_rank_masks[], std::vector<uint64_t>& bank_rank_functions,
    u_int64_t row_function, u_int64_t row_increment, int num_activations, int bank_no) {
  // === utility functions ===========
  // a wrapper around normalize_addr_to_bank that eliminates the need to pass the two last parameters
  auto normalize_address = [&](volatile char* address) {
    return normalize_addr_to_bank(address, bank_rank_masks[bank_no], bank_rank_functions);
  };
  // a wrapper for the logic required to get an address to hammer (or dummy)
  auto get_address = [&](volatile char* cur_next_addr, std::vector<int> offsets,
                         std::vector<volatile char*>& addresses) -> volatile char* {
    for (const auto& val : offsets) {
      cur_next_addr = normalize_address(cur_next_addr + (val * row_increment));
      printf("%" PRIu64 " ", get_row_index(cur_next_addr, row_function));
      addresses.push_back(cur_next_addr);
    }
    return cur_next_addr;
  };
  auto register_hammering_addresses = [&](size_t num_repetitions, const std::vector<volatile char*>& addresses) {
    aggressor_pairs.reserve(num_repetitions * addresses.size());
    for (size_t i = 0; i < num_repetitions; i++) {
      aggressor_pairs.insert(aggressor_pairs.end(), addresses.begin(), addresses.end());
    }
  };
  // ==================================

  // sanity check
  if (aggressor_pairs.size() > 0 || nops.size() > 0) {
    fprintf(stderr,
            "[-] Cannot generate new pattern without prior cleanup. "
            "Call cleanup_and_rerandomize before requesting a new pattern.\n");
    exit(1);
  }

  printf("[+] Generating a random hammering pattern.\n");

  std::vector<volatile char*> aggressor_candidates;
  std::vector<volatile char*> nop_candidates;

  // build sets of aggressors
  volatile char* cur_next_addr = normalize_address(random_start_address);
  printf("[+] Agg rows: ");
  for (int i = 0; i < num_hammering_pairs; i++) {
    cur_next_addr = get_address(cur_next_addr, {agg_inter_distance, agg_intra_distance}, aggressor_candidates);
  }
  printf("\n");
  // build sets of NOPs
  std::vector<int> nop_offsets = {100, agg_intra_distance};
  printf("[+] NOP rows: ");
  for (int i = 0; i < num_nops; i++) {
    cur_next_addr = get_address(cur_next_addr, {nop_offsets.at(i % nop_offsets.size())}, nop_candidates);
  }
  printf("\n");

  const int total_allowed_accesses = num_activations * (0.95 * num_refresh_intervals);  // TODO: print this number
  const int num_options = 2;

  // TODO fill arrays aggressor_pairs and nops with in total total_allowed_accesses accesses
  // consider that we need to insert clflush before accessing an address again
  int accesses_counter = 0;
  auto get_remaining_accesses = [&]() -> int { return total_allowed_accesses - accesses_counter; };
  while (accesses_counter < total_allowed_accesses) {
    auto selection = rand() % num_options;
    if (selection % num_options == 0) {  // randomly pick a hammering pair
      // get a random even number in the range [0, #elements-1] where #elements is supposed to be even (as we use pairs)
      int num_aggr_candidates = aggressor_candidates.size();
      if (num_aggr_candidates % 2 != 0) {
        fprintf(stderr, "[-] The number of aggressor candidate pairs is not even. Cannot continue - aborting.\n");
        exit(1);
      }
      num_aggr_candidates -= 1;
      int rand_even_idx = (rand() % ((num_aggr_candidates / 2) + 1)) * 2;
      volatile char* agg1 = aggressor_candidates.at(rand_even_idx);
      volatile char* agg2 = aggressor_candidates.at(rand_even_idx + 1);
      int multiplicator = multiplicator_hammering_pairs.get_random_number(get_remaining_accesses() / 2);
      if (multiplicator == -1) continue;
      register_hammering_addresses(multiplicator, {agg1, agg2});
      accesses_counter += 2 * multiplicator;
    } else if (selection % num_options == 1) {  // randomly pick a nop
      volatile char* nop = nop_candidates.at(rand() % nop_candidates.size());
      int multiplicator = multiplicator_nops.get_random_number(get_remaining_accesses());
      if (multiplicator == -1) continue;
      register_hammering_addresses(multiplicator, {nop});
      nops.push_back(nop);
      accesses_counter += multiplicator;
    }
  }

  // TODO: add printing of added pattern elements

  // generate jitted hammering code that hammers these selected addresses
  jit_hammering_code(agg_rounds, num_refresh_intervals);  // TODO This is probably wrong...

  return std::make_pair(aggressor_candidates.front(), aggressor_candidates.back());
}
