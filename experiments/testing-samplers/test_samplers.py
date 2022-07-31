"""The checks from the post as pytest tests. The edge-by-edge test is expected to fail."""

import jax
import numpy as np
import pytest

import thrml_runner
from harness import exact_distribution_test, mean_test, rhat
from models import Ising, edgewise_sweep, sample_chain, to_index

_rng = np.random.default_rng(0)
MODEL_A = Ising.ring(8, _rng.normal(0, 1, 8), _rng.normal(0, 0.5, 8), 1.5)


def test_thrml_matches_exact_distribution():
    pi = MODEL_A.exact()  # enumerate all 2^8 states
    samples = thrml_runner.run(MODEL_A, jax.random.key(1), n_chains=4000, n_warmup=200, n_samples=1)
    result = exact_distribution_test(to_index(samples[:, -1, :]), pi, np.random.default_rng(0))
    assert result["p_value"] > 0.01, f"TV {result['tv']:.3f} to the target"


@pytest.mark.xfail(reason="the edge-by-edge lowering samples the wrong distribution", strict=True)
def test_edgewise_matches_exact_distribution():
    rng = np.random.default_rng(1)
    P = edgewise_sweep(MODEL_A)
    final = np.array([sample_chain(P, 200, rng.integers(256), rng)[-1] for _ in range(4000)])
    result = exact_distribution_test(final, MODEL_A.exact(), rng)
    assert result["p_value"] > 0.01, f"TV {result['tv']:.3f} to the target"


def test_thrml_mean_energy():
    pi, energies = MODEL_A.exact(), MODEL_A.energies()
    samples = thrml_runner.run(MODEL_A, jax.random.key(2), n_chains=1, n_warmup=200, n_samples=2000)
    assert mean_test(energies[to_index(samples[0])], pi @ energies, "batch") > 0.01


@pytest.mark.xfail(reason="a cold ferromagnet does not mix within 2,000 sweeps", strict=True)
def test_cold_ferromagnet_mixes():
    model = Ising.ring(8, np.ones(8), np.zeros(8), 2.5)
    init = np.array([1, 1, 1, 1, -1, -1, -1, -1])
    samples = thrml_runner.run(model, jax.random.key(4), n_chains=8, n_warmup=200, n_samples=2000, init_spin=init)
    magnetisation = model.spins()[to_index(samples)].mean(-1)
    assert rhat(magnetisation) < 1.1
