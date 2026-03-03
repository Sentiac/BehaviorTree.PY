# BehaviorTree.PY

Production-ready Python bindings for BehaviorTree.CPP, maintained by **Sentiac**.

BehaviorTree.PY allows teams to author and execute BT.CPP behavior trees from Python while preserving strong interoperability with C++ nodes, plugins, and XML definitions.

## Table Of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Running Examples](#running-examples)
- [Documentation](#documentation)
- [Testing](#testing)
- [API Usage](#api-usage)
- [Interoperability Notes](#interoperability-notes)
- [Version Compatibility](#version-compatibility)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [Support](#support)
- [License](#license)

## Overview

BehaviorTree.PY is designed for mixed-language robotics and autonomy stacks that need:

- BT.CPP execution performance
- Python authoring ergonomics
- ROS-agnostic core usage
- Plugin compatibility with existing C++ BT nodes

The project keeps CMake as the canonical native build while providing a pip-first Python developer workflow.

## Key Features

- Create and tick trees from XML text or files
- Register Python node classes (sync, stateful, condition, decorator, control)
- Typed ports with strict conversion semantics
- JSON data lane for structured payloads
- Plugin loading for C++ BT nodes
- Tree introspection and visitor-based monitoring
- Blackboard backup/restore and JSON import/export support
- Logger integrations (`StdCoutLogger`, `FileLogger2`, `MinitraceLogger`, `SqliteLogger`, `Groot2Publisher`)
- Parity-focused coverage against BT.CPP example behaviors

## Requirements

- Python `>= 3.10`
- A compatible `behaviortree_cpp` installation discoverable by CMake
- C++ toolchain compatible with C++17

Recommended for ROS/ament-based environments:

- Source `/opt/ros/<distro>/setup.bash` before build/install commands when your BT.CPP install depends on ROS/ament CMake packages.

## Installation

### Option 1: Workspace Overlay (recommended for ROS users)

```bash
cd ros2_ws
source /opt/ros/<distro>/setup.bash
colcon build --symlink-install
source install/setup.bash

python3 -c "import behaviortree_py as bt; print(bt.btcpp_version_string())"
```

### Option 2: pip Install From Source

Point `CMAKE_PREFIX_PATH` at your BT.CPP install prefix:

```bash
cd /path/to/BehaviorTree.PY
source /opt/ros/<distro>/setup.bash  # if needed by your BT.CPP CMake config
CMAKE_PREFIX_PATH=/path/to/btcpp/install pip install .
python3 -c "import behaviortree_py as bt; print(bt.btcpp_version_string())"
```

Smoke install helper:

```bash
./scripts/pip_install_smoke.sh
```

## Quick Start

```python
import behaviortree_py as bt

factory = bt.BehaviorTreeFactory()
tree = factory.create_tree_from_text(
    """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
)

status = tree.tick_once()
print(status)
```

## Running Examples

Core examples are under `examples/`.

```bash
python3 examples/stage1_minimal_tick.py
python3 examples/stage4_plugin_loading.py
python3 examples/stage6_loggers.py
python3 examples/btcpp_remaining_suite.py
```

For example-level details, see:

- parity test commands in this README
- [docs/parity-limitations.md](./docs/parity-limitations.md)

## Documentation

Focused reference documentation:

- [API Overview](./docs/api-overview.md)
- [Data Contract](./docs/data-contract.md)
- [Parity Limitations](./docs/parity-limitations.md)

## Testing

Run all Python tests:

```bash
./scripts/run_tests.sh
```

Run BT.CPP parity suites directly:

```bash
python3 -m pytest -q tests/test_btcpp_examples_parity.py
python3 -m pytest -q tests/test_btcpp_examples_remaining_parity.py
```

## API Usage

### Register Python Nodes

Use class-based registration:

- `register_sync_action(...)`
- `register_stateful_action(...)`
- `register_condition(...)`
- `register_decorator(...)`
- `register_control(...)`

### Port Types

Supported typed port declarations in `provided_ports()` include:

- `bool`, `int`, `float`, `str`
- `list[bool]`, `list[int]`, `list[float]`, `list[str]`
- `json`
- `pose2d`
- `queue[pose2d]`

### Plugins

Load C++ plugins at runtime:

```python
factory.register_from_plugin("/abs/path/to/libyour_nodes.so")
```

### LoopNode Registration

Register BT.CPP `LoopNode<T>` from Python:

```python
factory.register_loop_node("LoopPose", "pose2d")
factory.register_loop_node("LoopDouble", "double")
```

### Lock-Scoped Port Content (by-reference mutation)

Python nodes can use lock-scoped mutable access:

```python
locked = self.get_locked_port_content("vector")
if locked.empty():
    locked.set([1])
else:
    locked.append(2)
```

## Interoperability Notes

- `Tree.tick_*()` releases the GIL while BT.CPP executes.
- Python callbacks (`tick`, `on_start`, `on_running`, `on_halted`, `halt`) run with the GIL held.
- Tree operations are currently constrained to the creating Python thread.
- For custom C++ types beyond built-ins, use type plugins and explicit conversion strategies.

Known fundamental non-one-to-one parity cases are documented in:

- [docs/parity-limitations.md](./docs/parity-limitations.md)

## Version Compatibility

On import, `behaviortree_py` validates linked BT.CPP version compatibility:

- same `MAJOR.MINOR` required
- `PATCH` mismatch allowed with warning

## Troubleshooting

- `Could not find behaviortree_cppConfig.cmake`:
  - Ensure `CMAKE_PREFIX_PATH` includes your BT.CPP install prefix.
- `libament_*` not found:
  - Source `/opt/ros/<distro>/setup.bash` before Python execution.
- Plugin symbol not found:
  - Build plugin with BT.CPP plugin macro/export flags (`BT_REGISTER_NODES`, `BT_PLUGIN_EXPORT` when required).

## Contributing

Contributions are welcome.
See [CONTRIBUTING.md](./CONTRIBUTING.md) for development setup, test expectations, and pull request guidelines.

## Support

For usage questions, integration support, or enterprise inquiries:

- **Company**: Sentiac
- **Email**: contact@sentiac.com

## License

See [LICENSE](./LICENSE).
