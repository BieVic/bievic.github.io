"""A minimal statistical test harness for samplers.

Three kinds of checks:
  * exact_distribution_test: compare samples against an exactly enumerated distribution
  * mean_test: test an expectation, either naively (iid assumption) or with batch-means errors
  * rhat: compare independent chains started from different states (mixing diagnostic)
"""

import numpy as np
from scipy import stats


def exact_distribution_test(state_idx, pi, rng, n_null=2000):
    """Monte Carlo test of H0: the samples are iid draws from pi.

    Statistic: TV distance between the empirical histogram and pi. The null distribution is
    obtained by drawing the same number of iid samples from pi itself, so the test is exact up to
    Monte Carlo error and needs no asymptotic approximation. Feed it *independent* samples
    (e.g. the final state of many independent chains), otherwise the null is wrong.
    """
    state_idx = np.asarray(state_idx)
    k = len(state_idx)
    emp = np.bincount(state_idx, minlength=len(pi)) / k
    observed = 0.5 * np.abs(emp - pi).sum()
    null = 0.5 * np.abs(rng.multinomial(k, pi, size=n_null) / k - pi).sum(-1)
    p_value = (1 + (null >= observed).sum()) / (1 + n_null)
    return {"tv": float(observed), "p_value": float(p_value), "null_tv_95": float(np.quantile(null, 0.95))}


def batch_means_se(series, n_batches=20):
    """Standard error of the mean of an autocorrelated series via non-overlapping batch means."""
    series = np.asarray(series, float)
    m = len(series) // n_batches
    means = series[: m * n_batches].reshape(n_batches, m).mean(1)
    return means.std(ddof=1) / np.sqrt(n_batches)


def ess(series, n_batches=20):
    """Effective sample size implied by the batch-means standard error."""
    series = np.asarray(series, float)
    se = batch_means_se(series, n_batches)
    return float(series.var(ddof=1) / se**2) if se > 0 else float("inf")


def mean_test(series, mu0, method):
    """Two-sided test of H0: E[series] = mu0.

    method="naive" assumes iid samples (standard error sigma / sqrt(n));
    method="batch" uses batch means, which accounts for autocorrelation.
    """
    series = np.asarray(series, float)
    if method == "naive":
        se = series.std(ddof=1) / np.sqrt(len(series))
        z = (series.mean() - mu0) / se
        return float(2 * stats.norm.sf(abs(z)))
    if method == "batch":
        n_batches = 20
        se = batch_means_se(series, n_batches)
        if se == 0:  # a constant series: the chain never moved
            return 1.0 if series.mean() == mu0 else 0.0
        t = (series.mean() - mu0) / se
        return float(2 * stats.t.sf(abs(t), df=n_batches - 1))
    raise ValueError(method)


def rhat(chains):
    """Gelman-Rubin potential scale reduction factor for an array of shape (n_chains, n_samples).

    Values near 1 mean the chains agree; values well above 1 (rule of thumb: > 1.01-1.1) mean
    they explore different regions, i.e. the sampler has not mixed.
    """
    chains = np.asarray(chains, float)
    m, n = chains.shape
    W = chains.var(axis=1, ddof=1).mean()
    B = n * chains.mean(axis=1).var(ddof=1)
    var_plus = (n - 1) / n * W + B / n
    return float(np.sqrt(var_plus / W)) if W > 0 else float("inf")
