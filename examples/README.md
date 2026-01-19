# BehaviorTree.PY examples

These scripts are a small “cookbook” for `behaviortree_py`.

They are intended to be run against the **workspace overlay** build (not a system install).

## Prerequisites

From the workspace root:

```bash
cd ros2_ws
source /opt/ros/<distro>/setup.bash
colcon build --symlink-install
source install/setup.bash
```

## Run

Examples are plain Python scripts:

```bash
python3 ros2_ws/src/BehaviorTree.PY/examples/stage1_minimal_tick.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage2_sync_action.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage2_stateful_action.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage2_condition_node.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage3_value_contract.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage4_create_from_file.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage4_plugin_loading.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage5_json_roundtrip.py
python3 ros2_ws/src/BehaviorTree.PY/examples/stage5_introspection.py
```

Notes:
- `stage4_plugin_loading.py` compiles a small C++ plugin at runtime; it requires a working compiler toolchain and CMake.
- Regression tests live under `../tests/` and can be run with `../scripts/run_tests.sh`.

