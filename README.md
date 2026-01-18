# BehaviorTree.PY (`behaviortree_py`)

Python bindings for BehaviorTree.CPP.

This project is:
- ROS-agnostic at the core (ROS packaging is a convenience layer)
- CMake-canonical (`CMakeLists.txt` defines the build)
- pip-friendly via `pyproject.toml` (Python-first entrypoint)

## Build and test

This repo is developed inside a colcon workspace and must be built/tested against the workspace copy of BehaviorTree.CPP.

See the workspace-level instructions in `README.md` at the repo root.

## Version compatibility

On import, `behaviortree_py` checks the linked BehaviorTree.CPP version:
- requires same `MAJOR.MINOR`
- warns on `PATCH` mismatch
