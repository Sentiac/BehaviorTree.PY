# BehaviorTree.PY Examples — Checklist (`examples/`)

Goal: examples + validation for `behaviortree_py` that run against the workspace overlay build.

This folder serves two purposes:
1) regression testbed for the Python bindings,
2) user-facing cookbook of patterns (including custom C++ struct interop).

Non-goals:
- Do not vendor or compile the core BT.CPP library.

Current status:
- Regression coverage currently lives in `../tests/` (Stages 1–5, except introspection + loggers).
- Runnable cookbook scripts live in this folder (`examples/`).

## Project setup
- [x] Decide build system for examples:
  - [x] pure Python scripts (recommended for most examples), plus optional CMake for custom-type plugins
  - [ ] or monorepo-style workspace instructions
- [ ] Document prerequisites:
  - [x] Workspace is built and sourced (see repo root `README.md`).
  - [ ] Python version(s)
  - [x] compiler toolchain (only for custom-type plugin examples)
- [x] Keep examples ROS-agnostic:
  - [x] no hardcoded ROS paths in code; environment setup is documented only.
- [x] If `BehaviorTree.PY` ships ROS metadata, add a colcon-based “happy path” for examples:
  - [x] workspace layout + commands to build/install behaviortree_py via colcon
  - [x] confirm examples run against the workspace overlay

## Smoke tests (must-have)
- [ ] Stage 0 command set (copy/paste):
  - [ ] `cd ../../.. && cd ros2_ws && source /opt/ros/<distro>/setup.bash && colcon build --symlink-install && source install/setup.bash`
  - [ ] `python3 -c "import behaviortree_py; print(behaviortree_py.btcpp_version_string())"`
- [x] Import test: `import behaviortree_py` and confirm it can locate `libbehaviortree_cpp.so`. (covered by `../tests/test_stage1_smoke.py`)
- [ ] Verify version compatibility checks:
  - [ ] Default requires same `MAJOR.MINOR` and warns on `PATCH` mismatch.
- [x] Minimal tree tick: create a trivial tree from XML/text and tick to completion. (covered by `../tests/test_stage1_smoke.py`)
- [x] Blackboard read/write: set a value, read it back, confirm expected conversion semantics. (covered by `../tests/test_stage3_ports_and_blackboard.py`)
- [ ] Strict input semantics:
  - [ ] Missing inputs raise (no optional/sentinel read API).
  - [x] Conversion failures raise. (covered by `../tests/test_stage3_ports_and_blackboard.py`)
  - [ ] Conversion failures include actionable messages (port name, expected type, and why the value was treated as JSON).
- [ ] GIL policy:
  - [x] Confirm other Python threads run while `tree.tick_*()` is executing (GIL released in C++ sections).
  - [ ] Confirm Python overrides are called with GIL held.
- [ ] Thread-safety scope:
  - [x] Initial: only tick from the creating/main Python thread (enforced + tested).
  - [ ] Documented.
  - [ ] (Deferred) multi-thread ticking support as an optional feature, with dedicated stress tests.
- [ ] Lifetime/ownership:
  - [ ] Confirm Python node instances remain alive while the tree exists (strong refs held by wrappers).
  - [ ] Confirm teardown releases Python references deterministically (no leaks in repeated create/destroy cycles).
- [ ] Error propagation:
  - [x] Python exceptions inside node overrides abort ticking and propagate to caller (BT.CPP-aligned).
  - [x] Exception messages include node path/name and the phase where it occurred.
- [ ] Run the smoke suite across targets:
  - [ ] ROS: humble, jazzy, kilted, rolling (separate environments/containers)

## Feature coverage examples (incremental)
- [ ] Stage mapping (keep in sync with `PROJECT_CHECKLIST.md`):
  - [x] Stage 1: create/tick built-in trees (no Python nodes). (covered by `../tests/test_stage1_smoke.py`)
  - [x] Stage 2: Python `SyncActionNode` and `StatefulActionNode` subclass examples using `provided_ports()`. (covered by `../tests/test_stage2_python_nodes.py`)
  - [x] Stage 3: typed primitives/lists + JSON lane + strict error examples. (covered by `../tests/test_stage3_ports_and_blackboard.py`)
  - [x] Stage 4: plugin-based examples (`registerFromPlugin`) and file-based trees. (covered by `../tests/test_stage4_create_from_file.py` + `../tests/test_stage4_plugin_loading.py`)
  - [x] Stage 5a: JSON import/export helpers. (covered by `../tests/test_stage5_json_export_import.py`)
  - [x] Stage 5b: introspection examples (requires `TreeNode` wrappers / `Tree.rootNode` bindings).
  - [ ] Stage 6: logger integration examples (feature-gated by installed deps).
- [x] Factory registration of Python action nodes (sync). (covered by `../tests/test_stage2_python_nodes.py`)
- [x] Stateful action nodes (start/running/halt). (covered by `../tests/test_stage2_python_nodes.py`)
- [ ] Async/cooperative pattern (if supported) and cancellation semantics.
- [ ] Subtrees, includes, and tree composition patterns.
- [ ] Plugins: loading and using shared-library nodes from system install (if applicable).
- [ ] Logging / monitoring examples (as supported by BT.CPP install).
- [ ] Ports declaration patterns:
  - [x] Demonstrate `@classmethod provided_ports()` on Python subclasses. (covered by `../tests/test_stage2_python_nodes.py`)
  - [ ] Demonstrate validation failures and error messages for malformed port specs.

- [x] Add runnable cookbook scripts under `examples/` (not just pytest):
  - [x] `stage1_minimal_tick.py` (built-in nodes only).
  - [x] `stage2_sync_action.py` (Python `SyncActionNode` subclass + ports).
  - [x] `stage2_stateful_action.py` (Python `StatefulActionNode` lifecycle + halting).
  - [x] `stage2_condition_node.py` (Python `ConditionNode` subclass).
  - [x] `stage3_value_contract.py` (typed primitives/lists + JSON lane + failure cases).
  - [x] `stage4_create_from_file.py` (+ sample XML under `examples/trees/`).
  - [x] `stage4_plugin_loading.py` (builds a small plugin at runtime via CMake).
  - [x] `stage5_json_roundtrip.py` (tree + blackboard export/import).
  - [x] `stage5_introspection.py` (TreeNode wrappers).

## Data interchange examples
- [x] Primitive ports: bool/int/double/string. (covered by `../tests/test_stage3_ports_and_blackboard.py`)
- [x] List ports: `list[int]`, `list[float]`, `list[str]` (as supported). (covered by `../tests/test_stage3_ports_and_blackboard.py`)
- [x] JSON fallback lane: dict/nested list structures with round-trip checks. (covered by `../tests/test_stage3_ports_and_blackboard.py`)
- [ ] Explicit “payload/config port” convention examples (JSON values not intended to feed typed C++ ports).
- [x] Mixed-type JSON arrays should raise clear errors (documented limitation). (covered by `../tests/test_stage3_ports_and_blackboard.py`)
- [ ] Pattern: replacing “mixed-type array” payloads with a custom struct:
  - [ ] Show a schema as a C++ struct with named fields (instead of heterogeneous arrays).
  - [ ] Register a BT.CPP JSON converter for that struct in a small helper library / type plugin.
  - [ ] Python passes a `dict` (JSON object) that maps to the struct via the converter.
  - [ ] (Optional) also expose the struct as a real Python class via a pybind11 type plugin.
- [ ] `None` handling (decision):
  - [x] `None` allowed only in JSON lane and maps to JSON `null`. (covered by `../tests/test_stage3_ports_and_blackboard.py`)
  - [ ] `None` rejected for typed ports/values, with clear error messages. (blocked until typed ports exist)
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
- [ ] Add `examples/README.md` that:
  - [x] Links to `../tests/` for regression coverage.
  - [x] Provides a copy/paste “run examples” flow against the workspace overlay.
- [ ] Add a “matrix” mapping example → feature(s) covered.
- [ ] Add a “platform matrix” mapping example → which environments it was validated on (ROS distros).
