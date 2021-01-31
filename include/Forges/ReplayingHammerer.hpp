#ifndef BLACKSMITH_SRC_FORGES_REPLAYINGHAMMERER_HPP_
#define BLACKSMITH_SRC_FORGES_REPLAYINGHAMMERER_HPP_

#include "Fuzzer/HammeringPattern.hpp"
#include "Memory/Memory.hpp"

#include <unordered_set>

class ReplayingHammerer {
 private:
  // the Memory instance for hammering
  Memory &mem;

  // a random number generator, required for std::shuffle
  std::mt19937 gen;

  // the FuzzingParameterSet instance belonging to the
  FuzzingParameterSet params;

  // maps: (mapping ID) -> (HammeringPattern), because there's no back-reference from mapping to HammeringPattern
  std::unordered_map<std::string, HammeringPattern> map_mapping_id_to_pattern;

  // the reproducibility score computed during the last invocation of hammer_pattern
  static double last_reproducibility_score;

  // the number of times in which hammering a pattern (at the same location) is repeated; this is only the initial
  // parameter as later we optimize this value
  const int initial_hammering_num_reps = 150;

  // the number of repetitions where we hammer the same pattern at the same location:
  // this is a dynamically learned parameter that is derived from the result of the reproducibility runs; optimizing
  // this allows to save time (and hammer more patterns) as for some DIMMs hammering longer doesn't increase the chance
  // to trigger bit flips
  int hammering_num_reps = initial_hammering_num_reps;

  size_t hammer_pattern(FuzzingParameterSet &fuzz_params, CodeJitter &code_jitter, HammeringPattern &pattern,
                        PatternAddressMapper &mapper, FLUSHING_STRATEGY flushing_strategy,
                        FENCING_STRATEGY fencing_strategy, unsigned long num_reps, int aggressors_for_sync,
                        int num_activations, bool early_stopping, bool sync_each_ref, bool verbose_sync,
                        bool verbose_memcheck, bool verbose_params, bool wait_before_hammering,
                        bool check_flips_after_each_rep);


  std::vector<HammeringPattern> load_patterns_from_json(const char *json_filename,
                                                        const std::unordered_set<std::string> &pattern_ids);

  PatternAddressMapper &get_most_effective_mapping(HammeringPattern &patt);

  void run_refresh_alignment_experiment(PatternAddressMapper &mapper);

  void run_code_jitting_probing(PatternAddressMapper &mapper);

  void find_direct_effective_aggs(PatternAddressMapper &mapper,
                                  std::unordered_set<AggressorAccessPattern> &direct_effective_aggs);

  void find_indirect_effective_aggs(PatternAddressMapper &mapper,
                                    const std::unordered_set<AggressorAccessPattern> &direct_effective_aaps,
                                    std::unordered_set<AggressorAccessPattern> &indirect_effective_aggs);

  void run_pattern_params_probing(PatternAddressMapper &mapper,
                                  const std::unordered_set<AggressorAccessPattern> &direct_effective_aggs,
                                  std::unordered_set<AggressorAccessPattern> &indirect_effective_aggs);

  void load_parameters_from_pattern(HammeringPattern &pattern, PatternAddressMapper &mapper);

  void sweep_pattern_internal(HammeringPattern &pattern, PatternAddressMapper &mapper, size_t num_reps);

 public:

  explicit ReplayingHammerer(Memory &mem);

  void replay_patterns(const char *json_filename, const std::unordered_set<std::string> &pattern_ids);

  void sweep_pattern(HammeringPattern &pattern, PatternAddressMapper &mapper,
                     FuzzingParameterSet &fuzz_params, size_t num_reps);
};

#endif //BLACKSMITH_SRC_FORGES_REPLAYINGHAMMERER_HPP_
