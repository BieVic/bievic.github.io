"""Run every experiment of the post with fixed seeds and write results.json."""

import json

import jax
import numpy as np

import thrml_runner
from harness import exact_distribution_test, ess, mean_test, rhat
from models import (
    Ising,
    chromatic_sweep,
    dobrushin,
    edgewise_sweep,
    sample_chain,
    stationary,
    to_index,
    tv,
    worst_case_row_tv,
)

rng = np.random.default_rng(0)
J = rng.normal(0, 1, 8)
h = rng.normal(0, 0.5, 8)
A = Ising.ring(8, J, h, 1.5)
B = Ising.ring(8, np.ones(8), np.full(8, 0.1), 1.0)
results = {"model_A": {"J": J.round(3).tolist(), "h": h.round(3).tolist(), "beta": 1.5}}

# --- Experiment 1: exact test catches the edge-by-edge lowering -------------------------
piA = A.exact()
E = edgewise_sweep(A)
correct = thrml_runner.run(A, jax.random.key(1), n_chains=4000, n_warmup=200, n_samples=1)
wrong = np.array([sample_chain(E, 200, rng.integers(256), rng)[-1] for _ in range(4000)])
sA = A.spins()
pE = stationary(E)
results["exp1"] = {
    "thrml": exact_distribution_test(to_index(correct[:, -1, :]), piA, rng),
    "edgewise": exact_distribution_test(wrong, piA, rng),
    "edgewise_stationary_tv_by_dt": {dt: tv(stationary(edgewise_sweep(A, dt)), piA) for dt in (1.0, 0.5, 0.1, 0.02)},
    "magnetisation_exact": (piA @ sA).round(3).tolist(),
    "magnetisation_edgewise": (pE @ sA).round(3).tolist(),
}

# --- Experiment 2: naive vs batch-means tests on a *correct* sampler --------------------
results["exp2"] = {}
for name, m, key in (("A", A, 3), ("B", B, 5)):
    pi = m.exact()
    en = m.energies()
    x = thrml_runner.run(m, jax.random.key(key), n_chains=500, n_warmup=200, n_samples=2000)
    series = en[to_index(x)]
    naive = np.array([mean_test(s, pi @ en, "naive") for s in series])
    batch = np.array([mean_test(s, pi @ en, "batch") for s in series])
    results["exp2"][name] = {
        "reject_naive": float((naive < 0.05).mean()),
        "reject_batch": float((batch < 0.05).mean()),
        "median_ess": float(np.median([ess(s) for s in series])),
        "n": 2000,
        "runs": 500,
    }
# power of the batch test against the edge-by-edge sampler on model A
enA = A.energies()
p_wrong = [mean_test(enA[sample_chain(E, 2000, rng.integers(256), rng)], piA @ enA, "batch") for _ in range(100)]
results["exp2"]["batch_power_vs_edgewise"] = float((np.array(p_wrong) < 0.05).mean())

# --- Experiment 3: a stuck ferromagnet, and which diagnostic notices -------------------
results["exp3"] = {}
for beta in (2.0, 2.5):
    F = Ising.ring(8, np.ones(8), np.zeros(8), beta)
    sF = F.spins()
    init = np.array([1, 1, 1, 1, -1, -1, -1, -1])
    x = thrml_runner.run(F, jax.random.key(4), n_chains=8, n_warmup=200, n_samples=2000, init_spin=init)
    idx = to_index(x)
    mag = sF[idx].mean(-1)
    results["exp3"][str(beta)] = {
        "chain_mean_magnetisation": mag.mean(1).round(3).tolist(),
        "exact_mean_magnetisation": float(F.exact() @ sF.mean(1)),
        "rhat_energy": rhat(F.energies()[idx]),
        "rhat_magnetisation": rhat(mag),
    }

# --- Experiment 4: certificates for a compiled (coupling-capped) sampler ---------------
C = Ising.ring(8, J, h, 0.5)
piC = C.exact()
P = chromatic_sweep(C)
rho = dobrushin(P)
rows = []
for jmax in (0.4, 0.6, 0.8, 1.0, 1.2):
    Jc = np.clip(J, -jmax, jmax)
    Pc = chromatic_sweep(C, J=Jc)
    # local certificate: sum over sites of the worst-case error of one site's update
    local = 0.0
    for i in range(C.n):
        nb = C.neighbours(i)
        worst = 0.0
        for signs in np.array(np.meshgrid(*[[-1, 1]] * len(nb))).T.reshape(-1, len(nb)):
            l = h[i] + sum(Jk * s for (_, Jk), s in zip(nb, signs))
            lc = h[i] + sum(np.clip(Jk, -jmax, jmax) * s for (_, Jk), s in zip(nb, signs))
            worst = max(worst, abs(1 / (1 + np.exp(-2 * C.beta * l)) - 1 / (1 + np.exp(-2 * C.beta * lc))))
        local += worst
    eta = worst_case_row_tv(P, Pc)
    P2, Pc2 = P @ P, Pc @ Pc
    rows.append(
        {
            "jmax": jmax,
            "measured": tv(stationary(Pc), piC),
            "bound_local": min(1.0, local / (1 - rho)),
            "bound_exact_1": min(1.0, eta / (1 - rho)),
            "bound_exact_2": min(1.0, worst_case_row_tv(P2, Pc2) / (1 - dobrushin(P2))),
        }
    )
results["exp4"] = {"beta": 0.5, "rho_sweep": rho, "max_abs_J": float(np.abs(J).max()), "rows": rows}

with open("results.json", "w") as f:
    json.dump(results, f, indent=2, default=float)
print(json.dumps(results, indent=2, default=float))
