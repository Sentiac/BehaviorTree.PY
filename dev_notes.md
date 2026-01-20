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
- **Issue**: Running the test suite from inside the source tree (`ros2_ws/src/BehaviorTree.PY`) can accidentally import the *source* `behaviortree_py/` package (which does not contain the compiled `_core`), causing `ModuleNotFoundError: behaviortree_py._core`.
- **Fix**: `scripts/run_tests.sh` now forces a safe pytest rootdir and runs from the workspace directory so imports resolve to the overlay-installed package.

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

### Ports + factory registry APIs
- **Ports spec**: `provided_ports()` now also accepts `{"inouts": [...]}` and maps it to BT.CPP `BidirectionalPort`.
- **Factory**: Bound the registered-tree workflow:
  - `register_behavior_tree_from_file/text`
  - `registered_behavior_trees`, `clear_registered_behavior_trees`
  - `create_tree(tree_name)`
- **Introspection**: Exposed `TypeInfo` and `PortInfo` classes; `TreeNode.ports_info` returns `PortInfo` objects per port.

### Decorator + control nodes (Python)
- **Added**: Python subclassing support for:
  - `DecoratorNode` (single-child semantics via `_bt.tick_child()` / `_bt.halt_child()`),
  - `ControlNode` (multi-child semantics via `_bt.children_count` / `_bt.tick_child(i)` / `_bt.halt_child(i)`).
- **Halt callback semantics**: Python `halt()` is invoked only when the node was `RUNNING`, mirroring BT.CPP’s `StatefulActionNode::halt()` pattern.
  - Reason: BT.CPP `Tree::haltTree()` calls `haltNode()` on the root twice (direct + recursive visitor) and expects node `halt()` implementations to be safe/no-op when not running.

### Value contract error UX
- **Improved errors**:
  - typed list validation errors include the failing element index and Python type,
  - JSON array homogeneity errors include index + expected/actual JSON type,
  - `get_input(...)` / `set_output(...)` errors include node `fullPath()` and `registration_id`.

### Typed ports (Python `provided_ports()`)
- **Added**: `provided_ports()` entries may be either strings (untyped `AnyTypeAllowed`) or dicts like `{"name": "x", "type": "int"}`.
- **Supported `type` strings**: `any`, `bool`, `int`, `float`, `str`, `json`, `list[bool]`, `list[int]`, `list[float]`, `list[str]`.
- **Enforcement**:
  - `set_output()` rejects JSON-lane values (`dict`, `None`) for non-JSON typed ports.
  - `get_input()` uses BT.CPP `getInput<T>` for strongly-typed ports (including conversion from string when available).

### Logging bindings
- **Added**: basic logger bindings that attach to a `Tree` and expose `flush()`:
  - `StdCoutLogger`
  - `FileLogger2`
  - `MinitraceLogger`
- **Added**: optional BT.CPP loggers (available when the linked BT.CPP build includes their deps):
  - `SqliteLogger(tree, filepath, append=False)` (requires `.db3` suffix)
  - `Groot2Publisher(tree, server_port=1667)` (binds a TCP port; ZeroMQ)

### Substitution rules (factory)
- **Added**: bindings for BT.CPP substitution rules:
  - `BehaviorTreeFactory.clear_substitution_rules()`
  - `BehaviorTreeFactory.add_substitution_rule(filter, rule)` where `rule` is either a node type string or a `TestNodeConfig` dict
  - `BehaviorTreeFactory.load_substitution_rules_from_json(json_text)`
  - `BehaviorTreeFactory.substitution_rules()`
- **TestNodeConfig dict keys**: `return_status`, `async_delay_ms` (also accepts BT.CPP-style `async_delay`), `success_script`, `failure_script`, `post_script`.
- **Design choice**: `TestNodeConfig.complete_func` is intentionally not supported from Python because it requires storing a `std::function` with tricky lifetime/capture semantics across the language boundary.

### pip install workflow
- **Status**: `pip install .` works against the workspace overlay only if the ROS environment is sourced.
- **Reason**: the workspace `behaviortree_cpp` is installed as a ROS/ament package and its generated CMake config may
  `find_package(ament_cmake_libraries)` via `ament_cmake_export_dependencies`. Without `/opt/ros/<distro>` on
  `CMAKE_PREFIX_PATH`, pure pip builds cannot configure against that BT.CPP install.
- **Mitigation**: `scripts/pip_install_smoke.sh` sources `/opt/ros/$ROS_DISTRO/setup.bash` when available before running
  the pip build.

### CI
- **Added**: GitHub Actions workflow that builds BT.CPP from source (matching `pyproject.toml` version) and runs
  `pip install .` + `./scripts/run_tests.sh` in ROS containers.
