# Splitting Python bindings into `BehaviorTree.PY` (no BehaviorTree.CPP fork)

## Summary / Is this possible?

Yes — with one key constraint:

- The bindings must **not** rely on any BehaviorTree.CPP header/library changes that exist only in a private fork. All interoperability must use public BehaviorTree.CPP APIs.

### The main technical risk

The legacy approach adds C++-side hooks to support `pybind11::object` directly as a port/blackboard value.

In this project we avoid that: the bindings must use **existing public APIs** only. This is doable because BehaviorTree.CPP already supports:

- `getInput<BT::Any>()`
- `Blackboard::getAnyLocked()`

So we can implement Python ↔ C++ value exchange **without storing `pybind11::object` inside BT’s blackboard**.

That does mean we must pick a “data interchange contract” (see **Architecture decisions** below).

---

## Goal State

Create a new repo rooted at `BehaviorTree.PY` that:

1. Builds only a Python extension module (pybind11) and Python package glue.
2. Links against BehaviorTree.CPP via `find_package(behaviortree_cpp CONFIG REQUIRED)` (no vendoring).
3. Does **not** vendor, build, or maintain the C++ core library.
4. Uses the **same version tags** as BehaviorTree.CPP (e.g., `4.7.1`, `4.7.2`, …).
5. Is **ROS-agnostic**: ROS packaging exists only for convenience, not as an architectural dependency.

---

## Proposed layout (in the new `BehaviorTree.PY` repo)

Minimal suggestion:

```
BehaviorTree.PY/
  pyproject.toml
  README.md
  src/
    core/
      bindings.cpp
      value_conv.hpp        # optional helper
  behaviortree_py/
    __init__.py
    ... (Python helpers)
  cmake/
    FindOrConfig.cmake      # optional helper
  tests/                    # optional
```

Avoid copying build artifacts (`*.egg-info/*`, `__pycache__/*`) and any `.git` history.

---

## Build strategy (recommended)

### Recommended: `scikit-build-core` + CMake + pybind11

Reason: clean, modern wheel builds and straightforward CMake integration.

High-level flow:

1. `pyproject.toml` uses `scikit-build-core`.
2. CMakeLists:
   - `find_package(Python COMPONENTS Interpreter Development REQUIRED)`
   - `find_package(pybind11 CONFIG REQUIRED)`
   - `find_package(behaviortree_cpp CONFIG REQUIRED)`
   - `pybind11_add_module(_core ...)`
   - `target_link_libraries(_core PRIVATE behaviortree_cpp::behaviortree_cpp)`

This is a **Python-first entrypoint** (pip/pyproject for users) backed by a **clean CMake core** (so ROS/ament or other CMake-based workflows can build/install it).
Treat CMake as canonical: `CMakeLists.txt` defines the build; `pyproject.toml` just drives it.

### Alternative: keep your current `setup.py` “CMake build_ext”

Possible, but you should treat it as legacy.
The legacy `setup.py` approach typically relies on custom shell scripts and is harder to keep portable.

---

## Step-by-step migration plan

### Phase 0 — Decide the “port value contract” (do this first)

Because upstream BehaviorTree.CPP won’t have any project-specific Python hooks, decide how Python nodes read/write port values:

- **Contract A (recommended): JSON bridge**
  - Python passes/receives only JSON-serializable types (bool/number/string/list/dict).
  - C++ side stores values in BT as concrete C++ types or as JSON blobs (exact representation to be defined).
  - The binding uses `BT::JsonExporter` + `nlohmann::json` + a `pybind11_json` bridge to convert `BT::Any ↔ json ↔ py::object`.
- **Contract B: “String-only ports”**
  - Python ports are always strings; Python is responsible for parsing/formatting.
  - Very stable, lowest coupling, but weaker interoperability with typed C++ ports.
- **Contract C: Limited typed set**
  - Support a fixed set of C++ types (int/double/bool/string/vector<string>/…).
  - Binding converts Python values into those only; errors for anything else.

### Phase 1 — Create the new repo skeleton in `BehaviorTree.PY`

1. Add `pyproject.toml` (scikit-build-core).
2. Add `CMakeLists.txt` that only builds the native extension `behaviortree_py._core` and links against installed BT.
3. Add Python package `behaviortree_py/` containing the public API (`__init__.py` etc.).

### Phase 2 — Make the extension link against system BT
1. Use `find_package(behaviortree_cpp CONFIG REQUIRED)`.
2. Link with `behaviortree_cpp::behaviortree_cpp`.

### Phase 3 — Refactor bindings to remove dependence on patched BT headers

In your current fork, bindings do things like:

- `node.getInput(name, py::object)`
- `node.setOutput(name, py::object)`

This implicitly requires BT core to understand `pybind11::object` in its templates.

To make it work with the system install:

1. Replace `getInput(name, py::object)` with `getInput<BT::Any>()` (or `getLockedPortContent`) and perform conversion in the binding layer.
2. Replace `setOutput(name, py::object)` with one of:
   - Convert Python → a supported concrete C++ type, then call `setOutput<T>()`, or
   - Convert Python → `nlohmann::json`, then use `BT::JsonExporter::fromJson(...)` to obtain `(BT::Any, BT::TypeInfo)` and store it safely (exact method depends on which BT public APIs you choose to use; likely via Blackboard APIs).

### Phase 4 — Packaging/versioning

1. Decide naming:
   - Python package name: `behaviortree_py`?
   - Extension module name: `behaviortree_py._core`?
2. Versioning:
   - Tag the repo `v4.7.1`, `v4.7.2`, … matching BehaviorTree.CPP.
   - Set Python package version to the same.
3. Compatibility policy:
   - Default: require same `MAJOR.MINOR` between `behaviortree_py` and installed BT.CPP; allow `PATCH` mismatch with a warning.

### Phase 5 — Runtime integration

Because we do not bundle `libbehaviortree_cpp.so`, runtime needs to locate it via the platform loader.
In this workspace, always run against the sourced workspace overlay (see repo root `README.md`).

### Phase 6 — CI / developer workflow

Suggested checks:

- Build in a clean ROS Jazzy container with only `ros-jazzy-behaviortree-cpp` installed.
- `pip install .` and run a minimal import + “tick a simple tree” smoke test.

---

## Architecture decisions (questions for you)

1. **Port/blackboard type contract:** Which do you want (A JSON, B string-only, C limited typed set)?
2. **How portable should it be outside this workspace?** (e.g. other ROS distros, non-ROS environments)
3. **Compatibility policy:** Should `behaviortree_py` enforce an exact match to the installed BT version, or allow a range?
4. **Distribution goal:** Do you need pip wheels (manylinux) or is “build from source in the target environment” acceptable?
5. **Python API surface:** Do you want to preserve the current Python API (e.g. generator-style async helper), or is breaking change acceptable?
