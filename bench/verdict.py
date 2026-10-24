"""The single definition of comparison thresholds and verdict policy."""
import math
import statistics

MIN_ROUNDS = 5
RECOMMENDED_ROUNDS = 10
MIN_P99_SAMPLES = 100
RELATIVE_THRESHOLD = 0.10


def decide(values, ratios, aa_ratios, better, *, samples_ok=True,
           oversubscribed=False, hybrid_cores=False, stub=False):
    """Ratios always mean implementation / baseline, including lower-is-better.

    Significance requires at least five valid paired rounds for both comparison
    and noise. Descriptive statistics may include additional unpaired values.
    """
    flags = []
    if MIN_ROUNDS <= len(values) < RECOMMENDED_ROUNDS:
        flags.append('below_10_rounds')
    if oversubscribed:
        flags.append('oversubscribed')
    if hybrid_cores:
        flags.append('hybrid_cores')
    if stub:
        flags.append('stub')
    if not aa_ratios:
        flags.append('no_aa_group')
    if not samples_ok:
        return 'insufficient_samples', flags
    if len(values) < MIN_ROUNDS or len(ratios) < MIN_ROUNDS:
        return 'insufficient_rounds', flags
    if aa_ratios and len(aa_ratios) < MIN_ROUNDS:
        flags.append('insufficient_aa_rounds')
    if len(aa_ratios) < MIN_ROUNDS:
        return 'no_significant_difference', flags
    median = statistics.median(ratios)
    delta = abs(median - 1.0)
    within_threshold = delta <= RELATIVE_THRESHOLD or math.isclose(
        delta, RELATIVE_THRESHOLD, rel_tol=0, abs_tol=1e-12)
    overlap = max(min(ratios), min(aa_ratios)) <= min(max(ratios), max(aa_ratios))
    if within_threshold or overlap:
        return 'no_significant_difference', flags
    improvement = median > 1 if better == 'higher' else median < 1
    return ('significant_improvement' if improvement else 'significant_regression'), flags
