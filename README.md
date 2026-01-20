# BehaviorTree.PY (`behaviortree_py`)

Python bindings for BehaviorTree.CPP.

This project is:
- ROS-agnostic at the core (ROS packaging is a convenience layer)
- CMake-canonical (`CMakeLists.txt` defines the build)
- pip-friendly via `pyproject.toml` (Python-first entrypoint)

Requires Python `>= 3.10`.

## Build and test

This repo is developed inside a colcon workspace and must be built/tested against the workspace copy of BehaviorTree.CPP.

See the workspace-level instructions in `README.md` at the repo root.

## Quickstart (workspace overlay)

From the workspace root:

```bash
cd ros2_ws
source /opt/ros/<distro>/setup.bash
colcon build --symlink-install
source install/setup.bash

python3 -c "import behaviortree_py as bt; print(bt.btcpp_version_string())"
python3 ros2_ws/src/BehaviorTree.PY/examples/stage1_minimal_tick.py
```

## pip install (non-ROS workflow)

`behaviortree_py` is pip-installable via `pyproject.toml`, but you must point CMake at an existing
`behaviortree_cpp` install (system or workspace overlay).

Against the workspace build:

```bash
cd /path/to/behavior_trees/ros2_ws
source /opt/ros/<distro>/setup.bash
colcon build --symlink-install
source install/setup.bash

cd src/BehaviorTree.PY
CMAKE_PREFIX_PATH="$(cd ../../install && pwd)" pip install .
python3 -c "import behaviortree_py as bt; print(bt.btcpp_version_string())"
```

Notes:
- If your `behaviortree_cpp` was installed as a ROS/ament package, its generated CMake config may depend on ament
  packages; make sure you sourced `/opt/ros/<distro>/setup.bash` before running `pip install .`.

Smoke helper (creates a venv and installs from source):

```bash
./scripts/pip_install_smoke.sh
```

## Threading / GIL contract

- `Tree.tick_*()` releases the GIL while executing BT.CPP code.
- Python overrides (`tick`, `on_start`, `on_running`, `on_halted`, `halt`) are invoked with the GIL held.
- Initial safety scope: `Tree` methods must be called from the creating Python thread; other threads fail fast.

## Data interchange (ports + blackboard)

Recommended patterns:
- Use typed ports (`{"name": "...", "type": "int"}` etc.) for values that must interoperate with C++ nodes.
- Use a dedicated JSON “payload/config” port (string type `"json"`) for structured data (`dict`, nested lists, `None`).

Rules (enforced):
- `None` and `dict` are only allowed for JSON-typed ports (map to JSON `null` / object).
- Flat homogeneous lists of primitives are supported as typed lists (`list[int]`, `list[str]`, etc.).
- JSON arrays must be homogeneous; mixed-type arrays raise.

## Plugins (C++ nodes)

`BehaviorTreeFactory.register_from_plugin(path)` loads a shared library plugin that exports BT.CPP nodes.

Notes:
- Plugins must use BT.CPP’s `BT_REGISTER_NODES(factory)` entrypoint.
- On some toolchains, you may need to compile the plugin with `-DBT_PLUGIN_EXPORT` to ensure the symbol is exported.
- See `examples/stage4_plugin_loading.py` for an end-to-end build+load pattern.

## Version compatibility

On import, `behaviortree_py` checks the linked BehaviorTree.CPP version:
- requires same `MAJOR.MINOR`
- warns on `PATCH` mismatch
