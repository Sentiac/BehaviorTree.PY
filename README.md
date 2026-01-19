# BehaviorTree.PY (`behaviortree_py`)

Python bindings for BehaviorTree.CPP.

This project is:
- ROS-agnostic at the core (ROS packaging is a convenience layer)
- CMake-canonical (`CMakeLists.txt` defines the build)
- pip-friendly via `pyproject.toml` (Python-first entrypoint)

## Build and test

This repo is developed inside a colcon workspace and must be built/tested against the workspace copy of BehaviorTree.CPP.

See the workspace-level instructions in `README.md` at the repo root.

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

## Version compatibility

On import, `behaviortree_py` checks the linked BehaviorTree.CPP version:
- requires same `MAJOR.MINOR`
- warns on `PATCH` mismatch
