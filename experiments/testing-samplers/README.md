# Testing samplers

Code for the blog post "Unit-testing a sampler".

- `models.py`: small Ising models with exact distributions and exact transition matrices
- `harness.py`: the checks (exact distribution test, naive and batch-means expectation tests, R-hat)
- `thrml_runner.py`: runs THRML's chromatic Gibbs sampler on many chains in parallel
- `run_all.py`: reproduces every number in the post and writes `results.json`
- `test_samplers.py`: the checks as pytest tests (two are expected to fail and marked `xfail`)
- `dashboard.py`: builds `dashboard.html`, an interactive overview of all experiments (open it in a browser)

Requires Python 3.10 or newer:

```sh
python -m venv .venv && . .venv/bin/activate
pip install thrml scipy pytest
python run_all.py
pytest test_samplers.py
python dashboard.py   # needs results.json from run_all.py
```
