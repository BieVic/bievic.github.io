"""Run THRML's chromatic block-Gibbs sampler on a models.Ising instance, many chains in parallel."""

import jax
import jax.numpy as jnp
import numpy as np
from thrml import Block, SamplingSchedule, SpinNode, sample_states
from thrml.models import IsingEBM, IsingSamplingProgram, hinton_init


def build(model):
    nodes = [SpinNode() for _ in range(model.n)]
    edges = [(nodes[a], nodes[b]) for a, b in model.edges]
    ebm = IsingEBM(nodes, edges, jnp.asarray(model.h), jnp.asarray(model.J), jnp.asarray(model.beta))
    # bipartite ring: even sites form one colour class, odd sites the other
    free_blocks = [Block(nodes[0::2]), Block(nodes[1::2])]
    program = IsingSamplingProgram(ebm, free_blocks, clamped_blocks=[])
    return nodes, ebm, free_blocks, program


def run(model, key, n_chains, n_warmup, n_samples, steps_per_sample=1, init_spin=None):
    """Return boolean samples of shape (n_chains, n_samples, n).

    init_spin: None for THRML's default (hinton_init), or an array of shape (n_chains,) with
    +1/-1 to start each chain with all spins up or all spins down.
    """
    nodes, ebm, free_blocks, program = build(model)
    k_init, k_samp = jax.random.split(key)
    if init_spin is None:
        init = hinton_init(k_init, ebm, free_blocks, (n_chains,))
    else:
        up = jnp.asarray(init_spin) > 0
        init = [jnp.broadcast_to(up[:, None], (n_chains, len(b))) for b in free_blocks]
    schedule = SamplingSchedule(n_warmup=n_warmup, n_samples=n_samples, steps_per_sample=steps_per_sample)
    keys = jax.random.split(k_samp, n_chains)

    def one_chain(k, state):
        return sample_states(k, program, schedule, state, [], [Block(nodes)])[0]

    out = jax.vmap(one_chain)(keys, init)
    return np.asarray(out, dtype=bool)
