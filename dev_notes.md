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
