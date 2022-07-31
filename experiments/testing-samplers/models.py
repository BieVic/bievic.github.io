"""Small Ising models that are exactly enumerable, plus exact transition matrices.

Spin convention follows THRML: a boolean state b maps to the spin s = 2b - 1, and
    E(s) = -beta * (sum_i h_i s_i + sum_(i,j) J_ij s_i s_j),   pi(s) ∝ exp(-E(s)).
States are indexed by the integer sum_i b_i 2^i.
"""

from dataclasses import dataclass

import numpy as np
from scipy.linalg import expm


@dataclass(frozen=True)
class Ising:
    n: int
    edges: tuple[tuple[int, int], ...]
    J: np.ndarray  # one weight per edge
    h: np.ndarray  # one bias per site
    beta: float

    @staticmethod
    def ring(n, J, h, beta):
        edges = tuple((i, (i + 1) % n) for i in range(n))
        return Ising(n, edges, np.asarray(J, float), np.asarray(h, float), float(beta))

    def spins(self):
        """All 2^n states as a (2^n, n) array of ±1 spins."""
        idx = np.arange(2 ** self.n)
        return 2 * ((idx[:, None] >> np.arange(self.n)) & 1) - 1

    def energies(self):
        s = self.spins()
        pair = sum(Jk * s[:, i] * s[:, j] for Jk, (i, j) in zip(self.J, self.edges))
        return -self.beta * (s @ self.h + pair)

    def exact(self):
        """The exact Boltzmann distribution over all 2^n states."""
        logw = -self.energies()
        w = np.exp(logw - logw.max())
        return w / w.sum()

    def neighbours(self, i):
        return [(j if a == i else a, Jk) for Jk, (a, j) in zip(self.J, self.edges) if i in (a, j)]


def to_index(bits):
    """Map boolean states of shape (..., n) to state indices."""
    bits = np.asarray(bits, dtype=np.int64)
    return (bits << np.arange(bits.shape[-1])).sum(-1)


def block_update(model: Ising, block, J=None, h=None):
    """Exact transition matrix of one Gibbs update of `block` (sites updated in parallel).

    J and h default to the model's own parameters; passing modified ones gives the
    matrix of a *compiled* (approximate) update instead.
    """
    J = model.J if J is None else J
    h = model.h if h is None else h
    s = model.spins()
    N = 2 ** model.n
    # local field of every site in every state: h_i + sum_j J_ij s_j
    field = np.tile(h, (N, 1)).astype(float)
    for Jk, (a, b) in zip(J, model.edges):
        field[:, a] += Jk * s[:, b]
        field[:, b] += Jk * s[:, a]
    p_up = 1.0 / (1.0 + np.exp(-2.0 * model.beta * field))  # P(s_i = +1 | rest)

    P = np.zeros((N, N))
    others = [k for k in range(model.n) if k not in block]
    for x in range(N):
        for y in range(N):
            if any(s[x, k] != s[y, k] for k in others):
                continue
            P[x, y] = np.prod([p_up[x, i] if s[y, i] == 1 else 1 - p_up[x, i] for i in block])
    return P


def chromatic_sweep(model: Ising, J=None, h=None):
    """One chromatic Gibbs sweep on a bipartite model: even sites, then odd sites."""
    even = list(range(0, model.n, 2))
    odd = list(range(1, model.n, 2))
    return block_update(model, even, J, h) @ block_update(model, odd, J, h)


def edgewise_sweep(model: Ising, dt=0.5):
    """The 'natural' edge-by-edge lowering: one two-spin Glauber gate per edge, composed.

    Each gate is exp(dt * Q_ij), where Q_ij is the Glauber generator of the two-spin energy
        E_ij = -beta * (J_ij s_i s_j + h_i s_i / deg_i + h_j s_j / deg_j),
    i.e. biases are split evenly over the edges touching a site. Every gate is a perfectly
    good sampler for its own two-spin energy, and sum_ij E_ij is the full energy, but the
    composition does not sample exp(-E).
    """
    s = model.spins()
    N = 2 ** model.n
    deg = np.zeros(model.n)
    for a, b in model.edges:
        deg[a] += 1
        deg[b] += 1

    P = np.eye(N)
    # edge colouring of the ring (even n): even-indexed edges, then odd-indexed edges
    order = list(range(0, len(model.edges), 2)) + list(range(1, len(model.edges), 2))
    for k in order:
        a, b = model.edges[k]
        local = [(sa, sb) for sa in (-1, 1) for sb in (-1, 1)]
        e = {
            (sa, sb): -model.beta * (model.J[k] * sa * sb + model.h[a] * sa / deg[a] + model.h[b] * sb / deg[b])
            for sa, sb in local
        }
        Q = np.zeros((4, 4))
        for u, (sa, sb) in enumerate(local):
            for v, (ta, tb) in enumerate(local):
                if (sa != ta) + (sb != tb) == 1:  # single-spin flips
                    Q[u, v] = 1.0 / (1.0 + np.exp(e[(ta, tb)] - e[(sa, sb)]))
            Q[u, u] = -Q[u].sum()
        G = expm(dt * Q)
        gate = np.zeros((N, N))
        for x in range(N):
            u = local.index((s[x, a], s[x, b]))
            for v, (ta, tb) in enumerate(local):
                y = x
                y = y | (1 << a) if ta == 1 else y & ~(1 << a)
                y = y | (1 << b) if tb == 1 else y & ~(1 << b)
                gate[x, y] += G[u, v]
        P = P @ gate
    return P


def stationary(P):
    """Stationary distribution of a row-stochastic matrix."""
    w, v = np.linalg.eig(P.T)
    pi = np.real(v[:, np.argmin(np.abs(w - 1))])
    return pi / pi.sum()


def tv(p, q):
    return 0.5 * np.abs(np.asarray(p) - np.asarray(q)).sum()


def dobrushin(P):
    """Dobrushin contraction coefficient: max TV distance between any two rows."""
    rows = P[:, None, :] - P[None, :, :]
    return 0.5 * np.abs(rows).sum(-1).max()


def worst_case_row_tv(P, Q):
    """sup_x TV(P(x, .), Q(x, .)): the worst-case error of Q as an implementation of P."""
    return 0.5 * np.abs(P - Q).sum(-1).max()


def sample_chain(P, n_steps, x0, rng):
    """Run a Markov chain with exact transition matrix P (used for samplers THRML can't express)."""
    cdf = np.cumsum(P, axis=1)
    xs = np.empty(n_steps, dtype=np.int64)
    x = x0
    for t in range(n_steps):
        x = min(np.searchsorted(cdf[x], rng.random()), P.shape[0] - 1)
        xs[t] = x
    return xs
