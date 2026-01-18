# dev_notes.md

This file tracks decisions, surprises, and any blocked items encountered while implementing `BehaviorTree.PY` in this repo.

## 2026-01-18

### Build/install (ROS/colcon)
- **Issue**: After `colcon build --symlink-install` and `source install/setup.bash`, `import behaviortree_py` failed because the install did not register a `PYTHONPATH` hook and the package was installed into a path not on `sys.path` by default.
- **Fix (decision)**: Use `ament_cmake_python`'s `ament_python_install_package(behaviortree_py)` when building under `ament_cmake` to:
  - install the pure-Python package into `PYTHON_INSTALL_DIR`,
  - register the standard `PYTHONPATH` environment hook so the overlay makes the package importable.
  - Keep non-ament builds (pip/scikit-build) using the normal CMake install path (`SKBUILD_PLATLIB_DIR` when set).

