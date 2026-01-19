# BehaviorTree.PY — Project Checklist

Goal: provide Python bindings for BehaviorTree.CPP (no vendoring/rebuilding BT.CPP), while exposing as much BT.CPP functionality as practical.

Non-goals:
- Maintain a fork of BehaviorTree.CPP in this repo.
- Require patches to BehaviorTree.CPP.

## Repo structure (planned)
- [ ] Provide a Python-first entrypoint:
  - [x] Create `pyproject.toml` using a modern CMake/wheel backend (e.g. `scikit-build-core`).
  - [ ] Ensure `pip install .` is the primary developer/user workflow (validate in a clean environment + document it).
- [x] Maintain a clean CMake core underneath (ROS-agnostic):
  - [x] Add `CMakeLists.txt` that builds only the pybind11 module(s) and links against `behaviortree_cpp::behaviortree_cpp`.
  - [x] Keep CMake install targets usable by CMake/ament consumers (ament Python install hook + overlay import works).
- [x] Treat CMake as canonical:
  - [x] Keep build options, feature toggles, and install layout defined in `CMakeLists.txt`.
  - [x] Keep `pyproject.toml` minimal and only responsible for invoking the CMake build.
- [x] Add Python package `behaviortree_py/` for ergonomic API surface.
- [x] Keep build/test instructions in the workspace-level `README.md`.
## Build artifact layout (decision)
- [x] All bindings live in the native extension `behaviortree_py._core`.
- [x] The pure-Python package `behaviortree_py/` contains only lightweight wrappers/helpers that call into `_core`.

## Versioning & compatibility
- [x] Decide version policy: `behaviortree_py` version equals BT.CPP version (e.g. `4.7.1`).
- [x] Compatibility policy (default):
  - [x] Require same `MAJOR.MINOR` between `behaviortree_py` and installed `behaviortree_cpp`.
  - [x] Allow `PATCH` mismatch with a warning.
- [x] Encode runtime check (at import-time or module init) that reads installed BT.CPP version from the linked library and warns/errors per policy.
- [ ] Define tagging scheme and release process (tags, changelog).

## Build & link against BehaviorTree.CPP
- [x] Build/link against `behaviortree_cpp::behaviortree_cpp` via `find_package(behaviortree_cpp CONFIG REQUIRED)`.
- [x] Development and CI must prefer the workspace copy of BehaviorTree.CPP (see workspace `README.md`).

## Python ↔ BT data interchange
- [x] Implement hybrid value contract:
  - [x] Typed values (for interop with C++ nodes):
    - [x] Primitives: `bool`, `int64`, `double`, `string`.
    - [x] Flat homogeneous lists of primitives (e.g. `list[int]`, `list[float]`, `list[str]`, `list[bool]`).
  - [x] JSON lane (everything else treated as JSON payload):
    - [x] `dict` / nested lists / mixed-type containers, etc.
- [ ] Document the recommended pattern:
  - [ ] Use typed values for ports that connect to C++ nodes.
  - [ ] Use an explicit “payload/config” port convention for JSON objects (Python-first wiring / JSON-aware nodes).
- [x] Define conversion rules precisely:
  - [x] Numeric mapping rules (Python `int`→`int64`, Python `float`→`double`, JSON number handling).
  - [x] Dict key restrictions (JSON requires string keys).
  - [x] Treatment of `None`/null:
    - [x] `None` allowed only in JSON lane and maps to JSON `null`.
    - [x] `None` rejected for typed ports/values (i.e., `None` is never treated as a typed primitive; it is JSON-lane only).
  - [x] JSON array policy: arrays must be homogeneous; mixed-type arrays error.
- [ ] Error UX:
  - [ ] When a value is treated as JSON, error/warn messages explain why (e.g., “nested list → JSON mode”) and suggest typed alternatives.
  - [ ] Define failure modes (type errors, conversion errors) and error messages.

## API coverage (bindings)
- [x] Expose core factory operations (register node types, create trees from XML/text/file).
- [x] Expose tree execution controls (`tickOnce`, `tickWhileRunning`, halting, status).
- [ ] Expose node base classes for user-implemented actions (subclassing-only API):
  - [x] `SyncActionNode` subclassing (override `tick()`).
  - [x] `StatefulActionNode` subclassing (override `on_start/on_running/on_halted`).
  - [ ] Document (and test) async patterns built on stateful nodes (e.g., generator-based helper in Python).
- [x] Ports declaration API (decision):
  - [x] Use `@classmethod def provided_ports(cls): ...` as the canonical mechanism (BT.CPP-aligned).
  - [x] Provide validation and clear errors for malformed port specs.
- [x] Expose blackboard access (read/write) in a way compatible with system BT.CPP (no storing `pybind11::object` inside BT).
- [x] Expose plugin loading mechanisms supported by BT.CPP installs (if applicable).
- [x] Input/Output access semantics (decision):
  - [x] Strict-only: missing inputs or conversion failures raise exceptions (no optional/sentinel API in the core surface).
- [ ] Threading/GIL policy (decision):
  - [x] Release the GIL while BT.CPP runs/ticks.
  - [x] Acquire the GIL only when invoking Python overrides (`tick`, `on_start`, `on_running`, `on_halted`) and other Python callbacks.
  - [x] Thread-safety scope (initial): ticking supported only from the creating/main Python thread.
  - [x] Fail fast if called from unexpected threads (where feasible).
  - [ ] Document the contract clearly.
- [x] Lifetime/ownership (decision):
  - [x] C++ node wrappers hold strong references to Python instances for the lifetime of the tree/node.
  - [x] Destruction releases Python refs deterministically when the tree is destroyed/reset (with GIL held).
- [ ] Error propagation (decision):
  - [x] Python exceptions raised inside node overrides propagate and abort the current tick (fail-fast, BT.CPP-aligned).
  - [x] Provide clear exception context (node name/path, phase: `tick`/`on_start`/`on_running`/`on_halted`).

## Custom type strategy (user-defined C++ structs)
- [ ] Define “type plugin” concept:
  - [ ] Users can build an optional extension module that registers JSON converters and (optionally) Python bindings for their structs.
  - [ ] Importing the plugin should register converters/bindings for the current process.
- [ ] Document two supported patterns:
  - [ ] JSON-only interop (struct represented as dict in Python).
  - [ ] Bound-class interop (struct exposed as a real Python class via plugin module).

## Staged binding plan (develop → test → advance)

Each stage should be completed with:
- a minimal, stable API surface
- examples added under `examples/` and regression tests under `tests/`
- validation across targets (humble/jazzy/kilted/rolling) as applicable

### Stage 0 — Build + discovery baseline (no BT features yet)
- [x] Canonical CMake project builds `behaviortree_py._core` and installs `behaviortree_py`.
- [ ] `pyproject.toml` drives the CMake build (`pip install .`) (validate this path end-to-end).
- [x] `find_package(behaviortree_cpp CONFIG REQUIRED)` works in the workspace build.
- [x] Version check implemented: require same `MAJOR.MINOR`, warn on `PATCH`.
- [ ] ROS packaging metadata included from the start (convenience):
  - [x] `package.xml` + minimal `CMakeLists.txt` hooks for colcon/ament workflows.
  - [x] Keep ROS packaging thin; do not hardcode ROS paths.
- [x] `source install/setup.bash` makes `behaviortree_py` importable (PYTHONPATH hook installed).

### Stage 1 — Core BT runtime (C++ nodes only)
Bind only the pieces needed to create and tick trees using **built-in** BT.CPP nodes (no Python-defined nodes yet).
- [x] `NodeStatus` surfaced cleanly.
- [x] `BehaviorTreeFactory`: `createTreeFromText`, `createTreeFromFile`, `registerFromPlugin`.
- [x] `Tree`: `tickOnce`, `tickExactlyOnce`, `tickWhileRunning`, `haltTree`, `rootBlackboard`.
- [x] `Tree.rootNode` binding (introduce a read-only `TreeNode` wrapper for introspection).
- [ ] Additional factory APIs needed for “real-world trees”:
  - [x] `BehaviorTreeFactory.registerBehaviorTreeFromFile/Text` + list/clear registered trees.
  - [ ] Substitution rules APIs (where exposed as non-template methods).

### Stage 2 — Python nodes (subclassing-only)
Enable Python-defined action nodes without patching BT.CPP.
- [x] Bind base classes for authoring:
  - [x] `SyncActionNode` (`tick`)
  - [x] `StatefulActionNode` (`on_start/on_running/on_halted`)
- [x] Factory registration for Python subclasses:
  - [x] Uses `@classmethod provided_ports()` as the canonical port spec.
  - [x] Builds a correct `TreeNodeManifest` and `NodeBuilder`.
- [x] Strict error semantics:
  - [x] Python exceptions propagate and abort ticking (BT.CPP-aligned).
  - [x] Missing input / conversion failures raise (no optional API).
- [x] Lifetime correctness:
  - [x] C++ wrappers hold strong refs to Python instances; deterministic teardown.
- [x] GIL policy implemented:
  - [x] release GIL for C++ ticking; acquire only for Python overrides.
  - [x] tick only from creating/main Python thread (initial).
- [ ] Expand Python node coverage toward BT.CPP parity:
  - [x] `ConditionNode` subclassing support.
  - [ ] `DecoratorNode` subclassing support (single-child semantics).
  - [ ] `ControlNode` subclassing support (multi-child semantics + halting rules).
  - [x] Add clear exception context (node path/name + phase) when rethrowing Python exceptions.

### Stage 3 — Data interchange contract + blackboard surface
Implement the agreed type/JSON rules without extending BT.CPP.
- [x] Typed values supported: primitives + flat homogeneous lists.
- [x] JSON lane supported for structured payloads:
  - [x] dict keys must be strings
  - [x] JSON arrays must be homogeneous
  - [x] `None` allowed only in JSON lane (`null`)
- [ ] Provide strict conversions and actionable errors (“why treated as JSON”, etc.).
- [x] Bind minimal blackboard APIs needed for examples (set/get, introspection as needed).
- [ ] Extend the port spec beyond “AnyTypeAllowed”:
  - [x] Support INOUT ports.
  - [ ] Support typed ports (PortInfo/TypeInfo) and enforce the typed/JSON contract against port types.
  - [x] Expose `PortDirection`, `PortInfo`, and `TypeInfo` in Python for introspection/debugging.

### Stage 4 — Plugins + real-world trees
Prove interoperability with externally-built nodes and file-based trees.
- [x] Plugin workflow tested (`registerFromPlugin`).
- [ ] Example trees using plugins and subtrees/includes (where supported by BT.CPP parser).
- [x] Avoid assumptions about build artifact paths (plugin-load test compiles a plugin into a temp dir).
- [ ] Document plugin workflow (how to build a BT.CPP plugin + load it from Python/ROS overlays).

### Stage 5 — Introspection + JSON import/export utilities
Expose read-only and utility APIs for debugging and tooling.
- [x] `Tree.applyVisitor`, `Tree.getNodesByPath` (or equivalent) where feasible.
- [x] `ExportTreeToJSON` / `ImportTreeFromJSON` bindings.
- [x] JSON export/import helpers using BehaviorTree.PY conversion rules.
- [ ] Serialized status snapshot utilities if needed by downstream tooling.
- [x] Introduce read-only `TreeNode` wrappers for introspection:
  - [x] name/fullPath/status/type/registration_name
  - [x] children traversal where applicable
  - [ ] parent traversal where feasible
  - [x] access to manifest/ports for each node

### Stage 6 — Logging integrations (optional, feature-gated)
Bind logger classes in a way that gracefully degrades when deps are missing.
- [ ] `StdCoutLogger`, `FileLogger2`, `MinitraceLogger`.
- [ ] `SqliteLogger` (requires sqlite).
- [ ] `Groot2Publisher` (requires ZeroMQ).

### Stage 7 — Packaging convenience (ROS + CI matrix)
- [x] Provide ROS package metadata/build for convenience (ROS-agnostic core).
- [ ] CI covers: humble/jazzy/kilted/rolling (at least smoke + Stage 2/3).

## Testing
- [x] Add a minimal test suite that runs against the workspace BT.CPP build.
- [ ] Add CI recipe (container or local instructions).
- [ ] Add a pip-based smoke test job (`pip install .` against a system/workspace BT.CPP).
- [ ] Add threading/GIL regression tests:
  - [ ] `tree.tick_*()` releases the GIL (other Python threads continue making progress).
  - [ ] Ticking from a non-creating/non-main Python thread fails fast with a clear error.
- [ ] Add stress tests for teardown/leaks (repeat create/tick/destroy loops).

## Documentation
- [ ] Quickstart: install + run a minimal tree.
- [ ] API reference mapping: BT.CPP concepts → Python equivalents.
- [ ] “Interoperability” guide: mixing Python nodes and C++ nodes, data types, and troubleshooting.
- [ ] Release notes template.

---

## Optional improvements (deferred)

- [ ] Callback-based registration API (in addition to subclassing):
  - [ ] `register_action("Name", fn, inputs=[...], outputs=[...])`
  - [ ] `register_stateful("Name", on_start, on_running, on_halted, ...)`
  - [ ] Keep this as sugar that wraps internal subclasses; do not block core work on it.

- [ ] Multi-thread entrypoints:
  - [ ] Allow calling `tree.tick_*()` from threads other than the creating/main Python thread, with a clear and tested contract.
  - [ ] Add stress tests for shutdown/destruction and callback re-entrancy.
