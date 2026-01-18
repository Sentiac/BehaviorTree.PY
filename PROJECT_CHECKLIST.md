# BehaviorTree.PY — Project Checklist

Goal: provide Python bindings for BehaviorTree.CPP (no vendoring/rebuilding BT.CPP), while exposing as much BT.CPP functionality as practical.

Non-goals:
- Maintain a fork of BehaviorTree.CPP in this repo.
- Require patches to BehaviorTree.CPP.

## Repo structure (planned)
- [ ] Provide a Python-first entrypoint:
  - [ ] Create `pyproject.toml` using a modern CMake/wheel backend (e.g. `scikit-build-core`).
  - [ ] Ensure `pip install .` is the primary developer/user workflow.
- [ ] Maintain a clean CMake core underneath (ROS-agnostic):
  - [ ] Add `CMakeLists.txt` that builds only the pybind11 module(s) and links against `behaviortree_cpp::behaviortree_cpp`.
  - [ ] Keep CMake install targets usable by CMake/ament consumers.
- [ ] Treat CMake as canonical:
  - [ ] Keep build options, feature toggles, and install layout defined in `CMakeLists.txt`.
  - [ ] Keep `pyproject.toml` minimal and only responsible for invoking the CMake build.
- [ ] Add Python package `behaviortree_py/` for ergonomic API surface.
- [ ] Keep build/test instructions in the workspace-level `README.md`.
## Build artifact layout (decision)
- [ ] All bindings live in the native extension `behaviortree_py._core`.
- [ ] The pure-Python package `behaviortree_py/` contains only lightweight wrappers/helpers that call into `_core`.

## Versioning & compatibility
- [ ] Decide version policy: `behaviortree_py` version equals BT.CPP version (e.g. `4.7.1`).
- [ ] Compatibility policy (default):
  - [ ] Require same `MAJOR.MINOR` between `behaviortree_py` and installed `behaviortree_cpp`.
  - [ ] Allow `PATCH` mismatch with a warning.
- [ ] Encode runtime check (at import-time or module init) that reads installed BT.CPP version from the linked library and warns/errors per policy.
- [ ] Define tagging scheme and release process (tags, changelog).

## Build & link against BehaviorTree.CPP
- [ ] Build/link against `behaviortree_cpp::behaviortree_cpp` via `find_package(behaviortree_cpp CONFIG REQUIRED)`.
- [ ] Development and CI must prefer the workspace copy of BehaviorTree.CPP (see workspace `README.md`).

## Python ↔ BT data interchange
- [ ] Implement hybrid value contract:
  - [ ] Typed values (for interop with C++ nodes):
    - [ ] Primitives: `bool`, `int64`, `double`, `string`.
    - [ ] Flat homogeneous lists of primitives (e.g. `list[int]`, `list[float]`, `list[str]`, `list[bool]`).
  - [ ] JSON lane (everything else treated as JSON payload):
    - [ ] `dict` / nested lists / mixed-type containers, etc.
- [ ] Document the recommended pattern:
  - [ ] Use typed values for ports that connect to C++ nodes.
  - [ ] Use an explicit “payload/config” port convention for JSON objects (Python-first wiring / JSON-aware nodes).
- [ ] Define conversion rules precisely:
  - [ ] Numeric mapping rules (Python `int`→`int64`, Python `float`→`double`, JSON number handling).
  - [ ] Dict key restrictions (JSON requires string keys).
  - [ ] Treatment of `None`/null:
    - [ ] `None` allowed only in JSON lane and maps to JSON `null`.
    - [ ] `None` rejected for typed ports/values.
  - [ ] JSON array policy: arrays must be homogeneous; mixed-type arrays error.
- [ ] Error UX:
  - [ ] When a value is treated as JSON, error/warn messages explain why (e.g., “nested list → JSON mode”) and suggest typed alternatives.
  - [ ] Define failure modes (type errors, conversion errors) and error messages.

## API coverage (bindings)
- [ ] Expose core factory operations (register node types, create trees from XML/text/file).
- [ ] Expose tree execution controls (`tickOnce`, `tickWhileRunning`, halting, status).
- [ ] Expose node base classes for user-implemented actions (subclassing-only API):
  - [ ] `SyncActionNode` subclassing (override `tick()`).
  - [ ] `StatefulActionNode` subclassing (override `on_start/on_running/on_halted`).
  - [ ] Document (and test) async patterns built on stateful nodes (e.g., generator-based helper in Python).
- [ ] Ports declaration API (decision):
  - [ ] Use `@classmethod def provided_ports(cls): ...` as the canonical mechanism (BT.CPP-aligned).
  - [ ] Provide validation and clear errors for malformed port specs.
- [ ] Expose blackboard access (read/write) in a way compatible with system BT.CPP (no storing `pybind11::object` inside BT).
- [ ] Expose plugin loading mechanisms supported by BT.CPP installs (if applicable).
- [ ] Input/Output access semantics (decision):
  - [ ] Strict-only: missing inputs or conversion failures raise exceptions (no optional/sentinel API in the core surface).
- [ ] Threading/GIL policy (decision):
  - [ ] Release the GIL while BT.CPP runs/ticks.
  - [ ] Acquire the GIL only when invoking Python overrides (`tick`, `on_start`, `on_running`, `on_halted`) and other Python callbacks.
  - [ ] Thread-safety scope (initial): ticking supported only from the creating/main Python thread.
  - [ ] Document the contract clearly and fail fast if called from unexpected threads (where feasible).
- [ ] Lifetime/ownership (decision):
  - [ ] C++ node wrappers hold strong references to Python instances for the lifetime of the tree/node.
  - [ ] Destruction releases Python refs deterministically when the tree is destroyed/reset (with GIL held).
- [ ] Error propagation (decision):
  - [ ] Python exceptions raised inside node overrides propagate and abort the current tick (fail-fast, BT.CPP-aligned).
  - [ ] Provide clear exception context (node name/path, phase: `tick`/`on_start`/`on_running`/`on_halted`).

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
- examples/tests added under `examples/`
- validation across targets (humble/jazzy/kilted/rolling) as applicable

### Stage 0 — Build + discovery baseline (no BT features yet)
- [ ] Canonical CMake project builds `behaviortree_py._core` and installs `behaviortree_py`.
- [ ] `pyproject.toml` drives the CMake build (`pip install .`).
- [ ] `find_package(behaviortree_cpp CONFIG REQUIRED)` works in the workspace build.
- [ ] Version check implemented: require same `MAJOR.MINOR`, warn on `PATCH`.
- [ ] ROS packaging metadata included from the start (convenience):
  - [ ] `package.xml` + minimal `CMakeLists.txt` hooks for colcon/ament workflows.
  - [ ] Keep ROS packaging thin; do not hardcode ROS paths.

### Stage 1 — Core BT runtime (C++ nodes only)
Bind only the pieces needed to create and tick trees using **built-in** BT.CPP nodes (no Python-defined nodes yet).
- [ ] `NodeStatus` and basic exceptions surfaced cleanly.
- [ ] `BehaviorTreeFactory`: `createTreeFromText`, `createTreeFromFile`, `registerFromPlugin`.
- [ ] `Tree`: `tickOnce`, `tickExactlyOnce`, `tickWhileRunning`, `haltTree`, `rootNode`, `rootBlackboard`.

### Stage 2 — Python nodes (subclassing-only)
Enable Python-defined action nodes without patching BT.CPP.
- [ ] Bind base classes for authoring:
  - [ ] `SyncActionNode` (`tick`)
  - [ ] `StatefulActionNode` (`on_start/on_running/on_halted`)
- [ ] Factory registration for Python subclasses:
  - [ ] Uses `@classmethod provided_ports()` as the canonical port spec.
  - [ ] Builds a correct `TreeNodeManifest` and `NodeBuilder`.
- [ ] Strict error semantics:
  - [ ] Python exceptions propagate and abort ticking (BT.CPP-aligned).
  - [ ] Missing input / conversion failures raise (no optional API).
- [ ] Lifetime correctness:
  - [ ] C++ wrappers hold strong refs to Python instances; deterministic teardown.
- [ ] GIL policy implemented:
  - [ ] release GIL for C++ ticking; acquire only for Python overrides.
  - [ ] tick only from creating/main Python thread (initial).

### Stage 3 — Data interchange contract + blackboard surface
Implement the agreed type/JSON rules without extending BT.CPP.
- [ ] Typed values supported: primitives + flat homogeneous lists.
- [ ] JSON lane supported for structured payloads:
  - [ ] dict keys must be strings
  - [ ] JSON arrays must be homogeneous
  - [ ] `None` allowed only in JSON lane (`null`)
- [ ] Provide strict conversions and actionable errors (“why treated as JSON”, etc.).
- [ ] Bind minimal blackboard APIs needed for examples (set/get, introspection as needed).

### Stage 4 — Plugins + real-world trees
Prove interoperability with externally-built nodes and file-based trees.
- [ ] Plugin workflow documented and tested (`registerFromPlugin`).
- [ ] Example trees using plugins and subtrees/includes (where supported by BT.CPP parser).
- [ ] Avoid assumptions about build artifact paths; examples should run from the workspace overlay.

### Stage 5 — Introspection + JSON import/export utilities
Expose read-only and utility APIs for debugging and tooling.
- [ ] `Tree.applyVisitor`, `Tree.getNodesByPath` (or equivalent) where feasible.
- [ ] `ExportTreeToJSON` / `ImportTreeFromJSON` bindings.
- [ ] Serialized status snapshot utilities if needed by downstream tooling.

### Stage 6 — Logging integrations (optional, feature-gated)
Bind logger classes in a way that gracefully degrades when deps are missing.
- [ ] `StdCoutLogger`, `FileLogger2`, `MinitraceLogger`.
- [ ] `SqliteLogger` (requires sqlite).
- [ ] `Groot2Publisher` (requires ZeroMQ).

### Stage 7 — Packaging convenience (ROS + CI matrix)
- [ ] Provide ROS package metadata/build for convenience (ROS-agnostic core).
- [ ] CI covers: humble/jazzy/kilted/rolling (at least smoke + Stage 2/3).

## Testing
- [ ] Add a minimal test suite that runs against the workspace BT.CPP build.
- [ ] Add CI recipe (container or local instructions).

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
