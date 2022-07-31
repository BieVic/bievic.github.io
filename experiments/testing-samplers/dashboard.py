"""Build dashboard.html: an interactive overview of the testing-samplers experiments.

Recomputes the raw data behind the post (with the same seeds as run_all.py where applicable)
and embeds it into a single HTML file that only needs a browser and internet access for the
plotting library.
"""

import json

import jax
import numpy as np

import thrml_runner
from harness import mean_test, rhat
from models import Ising, chromatic_sweep, edgewise_sweep, stationary, to_index

rng = np.random.default_rng(0)
J = rng.normal(0, 1, 8)
h = rng.normal(0, 0.5, 8)
A = Ising.ring(8, J, h, 1.5)
B = Ising.ring(8, np.ones(8), np.full(8, 0.1), 1.0)
data = {}

# 1. The three distributions over all 256 states of model A
pi = A.exact()
thrml_final = thrml_runner.run(A, jax.random.key(1), n_chains=4000, n_warmup=200, n_samples=1)
empirical = np.bincount(to_index(thrml_final[:, -1, :]), minlength=256) / 4000
edgewise = stationary(edgewise_sweep(A))
order = np.argsort(-pi)
data["dist"] = [
    {"rank": r, "state": int(s), "exact": float(pi[s]), "thrml": float(empirical[s]), "edgewise": float(edgewise[s])}
    for r, s in enumerate(order)
]

# 2. p-values of naive vs batch-means tests on a correct sampler, and autocorrelation
data["pvalues"] = {}
data["acf"] = {}
for name, m, key in (("A", A, 3), ("B", B, 5)):
    pim, en = m.exact(), m.energies()
    x = thrml_runner.run(m, jax.random.key(key), n_chains=500, n_warmup=200, n_samples=2000)
    series = en[to_index(x)]
    data["pvalues"][name] = {
        "naive": [mean_test(s, pim @ en, "naive") for s in series],
        "batch": [mean_test(s, pim @ en, "batch") for s in series],
    }
    centred = series - series.mean(1, keepdims=True)
    var = (centred**2).mean(1)
    data["acf"][name] = [
        float(np.mean((centred[:, : -k or None] * centred[:, k:]).mean(1) / var)) for k in range(0, 31)
    ]

# 3. Magnetisation traces of the cold ferromagnet
data["traces"] = {}
for beta in (2.0, 2.5):
    F = Ising.ring(8, np.ones(8), np.zeros(8), beta)
    init = np.array([1, 1, 1, 1, -1, -1, -1, -1])
    x = thrml_runner.run(F, jax.random.key(4), n_chains=8, n_warmup=200, n_samples=2000, init_spin=init)
    idx = to_index(x)
    mag = F.spins()[idx].mean(-1)
    data["traces"][str(beta)] = {
        "chains": mag[:, ::4].round(3).tolist(),  # thinned for the plot
        "rhat_magnetisation": rhat(mag),
        "rhat_energy": rhat(F.energies()[idx]),
    }

# 4. Certificates and summary numbers from run_all.py
with open("results.json") as f:
    results = json.load(f)
data["cert"] = results["exp4"]
data["summary"] = {
    "exp1": results["exp1"]["thrml"] | {"edgewise": results["exp1"]["edgewise"]},
    "exp2": results["exp2"],
}

with open("dashboard_template.html") as f:
    html = f.read().replace("/*DATA*/null", json.dumps(data))
with open("dashboard.html", "w") as f:
    f.write(html)
print("wrote dashboard.html")
