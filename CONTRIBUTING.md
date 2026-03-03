# Contributing

Thanks for your interest in contributing to BehaviorTree.PY.

## Development Setup

1. Install system prerequisites (compiler toolchain, CMake, Python).
2. Ensure `behaviortree_cpp` is available in `CMAKE_PREFIX_PATH`.
3. Install in editable mode:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip
CMAKE_PREFIX_PATH=/path/to/btcpp/install python -m pip install -v --no-build-isolation -e .
```

## Running Tests

```bash
./scripts/run_tests.sh
```

Parity suites:

```bash
python -m pytest -q tests/test_btcpp_examples_parity.py
python -m pytest -q tests/test_btcpp_examples_remaining_parity.py
```

## Pull Request Guidelines

- Keep changes focused and scoped.
- Add or update tests for behavior changes.
- Update docs when API or setup behavior changes.
- Ensure CI passes.

## Commit Style

- Use clear, imperative commit messages.
- Prefer small commits that are easy to review.

## Questions

For contribution-related questions: contact@sentiac.com
