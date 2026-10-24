"""CSV contracts shared by producers, runner and summarizer."""
RAW_FIELDS = (
    'round order impl workload workload_fingerprint threads mode metric value unit '
    'better valid invalid_reason stub swap_used_before_kb swap_used_after_kb pinned pinned_cpus'
).split()
PROGRAM_FIELDS = 'metric value unit better valid invalid_reason stub'.split()
SUMMARY_FIELDS = (
    'workload threads mode metric impl rounds_valid median min max ratio_median '
    'ratio_min ratio_max aa_min aa_max verdict flags'
).split()
MODES = {'prequeued', 'end_to_end', 'na'}
