from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

import behaviortree_py as bt


def main() -> None:
    factory = bt.BehaviorTreeFactory()

    with tempfile.TemporaryDirectory(prefix="btpy_plugin_") as tmp:
        tmp_path = Path(tmp)
        src_dir = tmp_path / "src"
        build_dir = tmp_path / "build"
        src_dir.mkdir(parents=True, exist_ok=True)
        build_dir.mkdir(parents=True, exist_ok=True)

        (src_dir / "CMakeLists.txt").write_text(
            """
cmake_minimum_required(VERSION 3.16)
project(btpy_example_plugin LANGUAGES CXX)

find_package(behaviortree_cpp CONFIG REQUIRED)

add_library(btpy_example_plugin SHARED plugin.cpp)
target_compile_features(btpy_example_plugin PRIVATE cxx_std_17)
target_link_libraries(btpy_example_plugin PRIVATE behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(btpy_example_plugin PRIVATE BT_PLUGIN_EXPORT)
""".strip()
            + "\n"
        )

        (src_dir / "plugin.cpp").write_text(
            r"""
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/action_node.h>

namespace
{
class HelloFromPlugin final : public BT::SyncActionNode
{
public:
  using BT::SyncActionNode::SyncActionNode;

  static BT::PortsList providedPorts()
  {
    return {};
  }

  BT::NodeStatus tick() override
  {
    return BT::NodeStatus::SUCCESS;
  }
};
}  // namespace

BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<HelloFromPlugin>("HelloFromPlugin");
}
""".lstrip()
            + "\n"
        )

        subprocess.check_call(["cmake", "-S", str(src_dir), "-B", str(build_dir)])
        subprocess.check_call(["cmake", "--build", str(build_dir)])

        lib_path = build_dir / "libbtpy_example_plugin.so"
        factory.register_from_plugin(str(lib_path))

        tree = factory.create_tree_from_text(
            """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <HelloFromPlugin/>
  </BehaviorTree>
</root>
""".strip()
        )

        print("loaded plugin:", lib_path)
        print("tick_once ->", tree.tick_once())


if __name__ == "__main__":
    main()
