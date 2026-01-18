import subprocess
from pathlib import Path

import behaviortree_py as bt


def _build_test_plugin(tmp_path: Path) -> Path:
    src_dir = tmp_path / "btpy_test_plugin"
    build_dir = tmp_path / "build"
    src_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    (src_dir / "plugin.cpp").write_text(
        r"""
#include <behaviortree_cpp/action_node.h>
#include <behaviortree_cpp/bt_factory.h>

class PluginAlwaysSuccess : public BT::SyncActionNode
{
public:
  PluginAlwaysSuccess(const std::string& name, const BT::NodeConfig& config) :
    BT::SyncActionNode(name, config)
  {}

  BT::NodeStatus tick() override
  {
    return BT::NodeStatus::SUCCESS;
  }

  static BT::PortsList providedPorts()
  {
    return {};
  }
};

BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<PluginAlwaysSuccess>("PluginAlwaysSuccess");
}
""".lstrip()
    )

    (src_dir / "CMakeLists.txt").write_text(
        r"""
cmake_minimum_required(VERSION 3.16)
project(btpy_test_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(behaviortree_cpp CONFIG REQUIRED)

add_library(btpy_test_plugin SHARED plugin.cpp)
target_link_libraries(btpy_test_plugin PRIVATE behaviortree_cpp::behaviortree_cpp)
target_compile_definitions(btpy_test_plugin PRIVATE BT_PLUGIN_EXPORT)
""".lstrip()
    )

    subprocess.run(["cmake", "-S", str(src_dir), "-B", str(build_dir)], check=True)
    subprocess.run(["cmake", "--build", str(build_dir), "--parallel"], check=True)

    lib = build_dir / "libbtpy_test_plugin.so"
    assert lib.exists()
    return lib


def test_register_from_plugin(tmp_path) -> None:
    plugin_path = _build_test_plugin(tmp_path)

    factory = bt.BehaviorTreeFactory()
    factory.register_from_plugin(str(plugin_path))

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <PluginAlwaysSuccess/>
  </BehaviorTree>
</root>
""".strip()

    tree = factory.create_tree_from_text(xml)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

