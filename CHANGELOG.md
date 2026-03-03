# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `BehaviorTreeFactory.register_scripting_enum(name, value)` and `register_scripting_enums({...})`.
- `Blackboard.create(parent=None)` support for parent/child blackboard hierarchies.
- Lock-scoped mutable port-content access API for Python nodes (`get_locked_port_content`).
- `BehaviorTreeFactory.register_loop_node(registration_id, value_type)` for BT.CPP `LoopNode<T>` registration.
- New typed port support:
  - `pose2d`
  - `queue[pose2d]`
- Expanded BT.CPP parity coverage via dedicated parity suites:
  - `tests/test_btcpp_examples_parity.py`
  - `tests/test_btcpp_examples_remaining_parity.py`

### Changed

- `provided_ports()` dict specs now support `default` and `description` fields.
- Packaging metadata in `pyproject.toml` expanded for public distribution (`authors`, `maintainers`, `classifiers`, `urls`, `keywords`, license metadata).
- Scikit-build configuration modernized:
  - `cmake.minimum-version` -> `cmake.version`
  - removed explicit `cmake` and `ninja` build-system dependencies (injected by scikit-build-core)

### Removed

- Internal-only markdown notes/checklists that were not intended for public consumers.
