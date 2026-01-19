# dev_notes.md

This file tracks decisions, surprises, and any blocked items encountered while implementing `BehaviorTree.PY` in this repo.

## 2026-01-18

### Build/install (ROS/colcon)
- **Issue**: After `colcon build --symlink-install` and `source install/setup.bash`, `import behaviortree_py` failed because the install did not register a `PYTHONPATH` hook and the package was installed into a path not on `sys.path` by default.
- **Fix (decision)**: Use `ament_cmake_python`'s `ament_python_install_package(behaviortree_py)` when building under `ament_cmake` to:
  - install the pure-Python package into `PYTHON_INSTALL_DIR`,
  - register the standard `PYTHONPATH` environment hook so the overlay makes the package importable.
  - Keep non-ament builds (pip/scikit-build) using the normal CMake install path (`SKBUILD_PLATLIB_DIR` when set).

### Stage 1 bindings (C++ nodes only)
- **Decision**: Bind `BehaviorTreeFactory`, `Tree`, and `NodeStatus` directly against upstream BehaviorTree.CPP public APIs.
- **GIL policy (initial)**: release the GIL for `Tree.tick_*` and `halt_tree` calls since Stage 1 has no Python callbacks.

### Stage 2 Python nodes (initial design)
- **Ports spec (decision)**: `@classmethod provided_ports()` must return `{"inputs": list[str], "outputs": list[str]}` (untyped for now; treated as `AnyTypeAllowed`).
- **Constructor call (decision)**: the factory calls node constructors as `node_type(name, *args, **kwargs)` (name first).
- **Implementation approach (decision)**: C++ owns the BT node instance; each node holds a strong `py::object` reference to a separate Python object instance and calls `tick()` / `on_start()` / `on_running()` / `on_halted()` with the GIL held.

### Stage 3 ports/blackboard values (initial design)
- **Typed lane**: `bool`, `int`→`int64`, `float`→`double`, `str`, plus *flat homogeneous* lists of those primitives.
- **JSON lane**: `dict` / nested lists / mixed containers / `None` → stored as `nlohmann::json` in the blackboard; JSON dict keys must be strings; JSON arrays must be homogeneous (mixed element types raise).
- **Empty list**: treated as JSON lane (stored as an empty JSON array) since element type cannot be inferred.

### Stage 5 JSON export/import
- **Decision**: `Tree.to_json()/from_json()` and `Blackboard.to_json()/from_json()` use BehaviorTree.PY’s own value conversion rules so they round-trip JSON-lane values cleanly.
- **Note**: `Tree.to_bt_json()/from_bt_json()` and `Blackboard.to_bt_json()/from_bt_json()` expose BT.CPP’s `Export*ToJSON` / `Import*FromJSON` behavior, which is limited by BT.CPP `JsonExporter` support (may omit/skip unsupported types).

### Stage 4 plugins (test strategy)
- **Decision**: Add a pytest integration test that compiles a tiny BT.CPP plugin (`BT_REGISTER_NODES`) into a temp dir and loads it via `BehaviorTreeFactory.register_from_plugin()`.

## 2026-01-19

### Testing (pytest + ROS env)
- **Issue**: Running the full test suite with `pytest` in a ROS (jazzy) environment intermittently crashed (SIGSEGV) due to auto-loaded `launch_testing*` pytest plugins, which are not needed for this project’s unit tests.
- **Workaround (decision)**: Run tests with ROS plugins disabled explicitly:
  - `python3 -m pytest -q -p no:launch_testing -p no:launch_ros -p no:launch_pytest <tests...>`
  - or `PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 python3 -m pytest -q <tests...>`
  - A small wrapper script is provided at `scripts/run_tests.sh`.

### Threading + exception context
- **Decision**: Fail fast when calling `Tree.tick_*()` and `Tree.halt_tree()` from any thread other than the one that created the `Tree`.
  - Implementation: store a per-tree creator thread id and validate at the start of each of those methods.
- **Decision**: Preserve Python exception types/tracebacks from Python-defined nodes, but add context using `BaseException.add_note(...)` (PEP 678):
  - node `fullPath()`
  - node `registrationName()`
  - phase (`tick()`, `on_start()`, `on_running()`, `on_halted()`)

### Condition nodes
- **Decision**: Provide Python `ConditionNode` subclassing support (BT.CPP `NodeType::CONDITION`) using the same `_bt` handle + `provided_ports()` contract as action nodes.

### Introspection (TreeNode)
- **Added**: Read-only introspection bindings:
  - `Tree.root_node`, `Tree.nodes()`, `Tree.get_nodes_by_path()`, `Tree.apply_visitor()`
  - `TreeNode` properties: `name`, `full_path`, `registration_name`, `uid`, `status`, `type`, `ports`, `blackboard`, `input_ports`, `output_ports`
  - `TreeNode.children()` for Control/Decorator nodes
- **Bugfix**: `Tree.from_json()` previously used a temporary `py::str` stored as `py::handle`, causing a dangling reference and a potential segfault under repeated calls. Fixed by owning the key as `py::str`.
