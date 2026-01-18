# BehaviorTree.PY Examples — Checklist (`examples/`)

Goal: examples + validation for `behaviortree_py` that run against the workspace overlay build.

This folder serves two purposes:
1) regression testbed for the Python bindings,
2) user-facing cookbook of patterns (including custom C++ struct interop).

Non-goals:
- Do not vendor or compile the core BT.CPP library.

## Project setup
- [ ] Decide build system for examples:
  - [ ] pure Python (recommended for most examples), plus optional CMake for custom-type plugins
  - [ ] or monorepo-style workspace instructions
- [ ] Document prerequisites:
  - [ ] Workspace is built and sourced (see repo root `README.md`).
  - [ ] Python version(s)
  - [ ] `pybind11` / compiler toolchain (only for custom-type plugin examples)
- [ ] Keep examples ROS-agnostic:
  - [ ] no hardcoded ROS paths in code; environment setup is documented only.
- [ ] If `BehaviorTree.PY` ships ROS metadata, add a colcon-based “happy path” for examples:
  - [ ] workspace layout + commands to build/install behaviortree_py via colcon
  - [ ] confirm examples run against the workspace overlay

## Smoke tests (must-have)
- [ ] Stage 0 command set (copy/paste):
  - [ ] `cd ../../.. && cd ros2_ws && source /opt/ros/<distro>/setup.bash && colcon build --symlink-install && source install/setup.bash`
  - [ ] `python3 -c "import behaviortree_py; print(behaviortree_py.btcpp_version_string())"`
- [ ] Import test: `import behaviortree_py` and confirm it can locate `libbehaviortree_cpp.so`.
- [ ] Verify version compatibility checks:
  - [ ] Default requires same `MAJOR.MINOR` and warns on `PATCH` mismatch.
- [ ] Minimal tree tick: create a trivial tree from XML/text and tick to completion.
- [ ] Blackboard read/write: set a value, read it back, confirm expected conversion semantics.
- [ ] Strict input semantics:
  - [ ] Missing inputs raise (no optional/sentinel read API).
  - [ ] Conversion failures raise with actionable messages.
- [ ] GIL policy:
  - [ ] Confirm other Python threads run while `tree.tick_*()` is executing (GIL released in C++ sections).
  - [ ] Confirm Python overrides are called with GIL held.
- [ ] Thread-safety scope:
  - [ ] Initial: only tick from the creating/main Python thread (documented).
  - [ ] (Deferred) multi-thread ticking support as an optional feature, with dedicated stress tests.
- [ ] Lifetime/ownership:
  - [ ] Confirm Python node instances remain alive while the tree exists (strong refs held by wrappers).
  - [ ] Confirm teardown releases Python references deterministically (no leaks in repeated create/destroy cycles).
- [ ] Error propagation:
  - [ ] Python exceptions inside node overrides abort ticking and propagate to caller (BT.CPP-aligned).
  - [ ] Exception messages include node path/name and the phase where it occurred.
- [ ] Run the smoke suite across targets:
  - [ ] ROS: humble, jazzy, kilted, rolling (separate environments/containers)

## Feature coverage examples (incremental)
- [ ] Stage mapping (keep in sync with `PROJECT_CHECKLIST.md`):
  - [ ] Stage 1: create/tick built-in trees (no Python nodes).
  - [ ] Stage 2: Python `SyncActionNode` and `StatefulActionNode` subclass examples using `provided_ports()`.
  - [ ] Stage 3: typed primitives/lists + JSON lane + strict error examples.
  - [ ] Stage 4: plugin-based examples (`registerFromPlugin`) and file-based trees.
  - [ ] Stage 5: introspection and JSON import/export examples.
  - [ ] Stage 6: logger integration examples (feature-gated by installed deps).
- [ ] Factory registration of Python action nodes (sync).
- [ ] Stateful action nodes (start/running/halt).
- [ ] Async/cooperative pattern (if supported) and cancellation semantics.
- [ ] Subtrees, includes, and tree composition patterns.
- [ ] Plugins: loading and using shared-library nodes from system install (if applicable).
- [ ] Logging / monitoring examples (as supported by BT.CPP install).
- [ ] Ports declaration patterns:
  - [ ] Demonstrate `@classmethod provided_ports()` on Python subclasses.
  - [ ] Demonstrate validation failures and error messages for malformed port specs.

## Data interchange examples
- [ ] Primitive ports: bool/int/double/string.
- [ ] List ports: `list[int]`, `list[float]`, `list[str]` (as supported).
- [ ] JSON fallback lane: dict/nested list structures with round-trip checks.
- [ ] Explicit “payload/config port” convention examples (JSON values not intended to feed typed C++ ports).
- [ ] Mixed-type JSON arrays should raise clear errors (documented limitation).
- [ ] Pattern: replacing “mixed-type array” payloads with a custom struct:
  - [ ] Show a schema as a C++ struct with named fields (instead of heterogeneous arrays).
  - [ ] Register a BT.CPP JSON converter for that struct in a small helper library / type plugin.
  - [ ] Python passes a `dict` (JSON object) that maps to the struct via the converter.
  - [ ] (Optional) also expose the struct as a real Python class via a pybind11 type plugin.
- [ ] `None` handling (decision):
  - [ ] `None` allowed only in JSON lane and maps to JSON `null`.
  - [ ] `None` rejected for typed ports/values, with clear error messages.
- [ ] Failure examples: demonstrate and explain conversion errors.

## Custom C++ struct interop examples (key deliverable)

### Pattern A — JSON-only struct interop
- [ ] Define a sample struct in a small C++ helper library (built separately from BT.CPP).
- [ ] Register BT.CPP JSON converters for the struct in that helper library.
- [ ] In Python, use dicts to represent the struct and show it flowing through the tree.
- [ ] Document how a user would replicate this for their own struct.

### Pattern B — Bound-class struct interop (pybind11)
- [ ] Create a “type plugin” Python extension module that:
  - [ ] exposes the struct as a Python class
  - [ ] registers JSON converters (or provides conversion glue) on import
- [ ] Demonstrate passing/receiving this struct as a Python object in user code (with clear limitations).
- [ ] Document build/install steps for the plugin and how it integrates with `behaviortree_py`.

## Docs & UX
- [ ] Each example has:
  - [ ] a short “what it shows”
  - [ ] prerequisites
  - [ ] expected output
  - [ ] troubleshooting section
- [ ] Add a “matrix” mapping example → feature(s) covered.
- [ ] Add a “platform matrix” mapping example → which environments it was validated on (ROS distros).
