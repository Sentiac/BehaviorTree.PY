from __future__ import annotations

import os
import tempfile
from pathlib import Path

import behaviortree_py as bt


def _plugin_path() -> Path:
    prefix = Path(
        os.environ.get("BTCPP_INSTALL_PREFIX", "/home/rishab_vm/tools/BehaviorTree.CPP_4_8_3/install")
    )
    return prefix / "share" / "behaviortree_cpp" / "bt_plugins" / "libcrossdoor_nodes_dyn.so"


class ThinkWhatToSay(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "text", "type": "str"}]}

    def tick(self) -> bt.NodeStatus:
        self.set_output("text", "The answer is 42")
        return bt.NodeStatus.SUCCESS


class SayRuntime(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [{"name": "message", "type": "str"}], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        print("Robot says:", self.get_input("message"))
        return bt.NodeStatus.SUCCESS


class BatteryOK(bt.ConditionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        return bt.NodeStatus.SUCCESS


class MoveBaseAsync(bt.StatefulActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": []}

    def __init__(self, name: str):
        super().__init__(name)
        self._ticks = 0

    def on_start(self) -> bt.NodeStatus:
        self._ticks = 0
        return bt.NodeStatus.RUNNING

    def on_running(self) -> bt.NodeStatus:
        self._ticks += 1
        if self._ticks < 3:
            return bt.NodeStatus.RUNNING
        return bt.NodeStatus.SUCCESS

    def on_halted(self) -> None:
        return


class ActionA(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "out_a", "type": "str"}]}

    def __init__(self, name: str, arg_int: int, arg_str: str):
        super().__init__(name)
        self._arg_int = arg_int
        self._arg_str = arg_str

    def tick(self) -> bt.NodeStatus:
        self.set_output("out_a", f"{self._arg_int}/{self._arg_str}")
        return bt.NodeStatus.SUCCESS


class NodeWithDefaultPorts(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {
            "inputs": [
                {"name": "input", "type": "int"},
                {"name": "pointA", "type": "int", "default": "1"},
                {"name": "pointB", "type": "int", "default": "{point}"},
                {"name": "pointC", "type": "int", "default": "5"},
                {"name": "pointD", "type": "int", "default": "{=}"},
            ],
            "outputs": [],
        }

    def tick(self) -> bt.NodeStatus:
        print(
            "defaults:",
            self.get_input("input"),
            self.get_input("pointA"),
            self.get_input("pointB"),
            self.get_input("pointC"),
            self.get_input("pointD"),
        )
        return bt.NodeStatus.SUCCESS


class PushIntoVector(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {
            "inputs": [{"name": "value", "type": "int"}],
            "inouts": [{"name": "vector", "type": "list[int]"}],
        }

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("value")
        locked = self.get_locked_port_content("vector")
        if locked.empty():
            locked.set([value])
        else:
            locked.append(value)
        return bt.NodeStatus.SUCCESS


class GenerateWaypoints(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "waypoints", "type": "queue[pose2d]"}]}

    def tick(self) -> bt.NodeStatus:
        self.set_output(
            "waypoints",
            [
                {"x": 0.0, "y": 0.0, "theta": 0.0},
                {"x": 1.0, "y": 1.0, "theta": 0.0},
                {"x": 2.0, "y": 2.0, "theta": 0.0},
            ],
        )
        return bt.NodeStatus.SUCCESS


class UseWaypoint(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [{"name": "waypoint", "type": "pose2d"}], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        wp = self.get_input("waypoint")
        print("waypoint:", wp)
        return bt.NodeStatus.SUCCESS


def run_t02_ex02() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(ThinkWhatToSay)
    factory.register_sync_action(SayRuntime)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <ThinkWhatToSay text="{the_answer}"/>
      <SayRuntime message="{the_answer}"/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    print("t02/ex02:", tree.tick_once(), tree.root_blackboard().get("the_answer"))


def run_t04() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_condition(BatteryOK)
    factory.register_stateful_action(MoveBaseAsync)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <ReactiveSequence>
      <BatteryOK/>
      <MoveBaseAsync/>
    </ReactiveSequence>
  </BehaviorTree>
</root>
""".strip()
    )
    print("t04:", tree.tick_while_running(1))


def run_t05() -> None:
    plugin = _plugin_path()
    if not plugin.exists():
        print("t05: skipped (missing plugin)", plugin)
        return
    factory = bt.BehaviorTreeFactory()
    factory.register_from_plugin(str(plugin))
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Fallback>
        <Inverter><IsDoorClosed/></Inverter>
        <SubTree ID="DoorClosed"/>
      </Fallback>
      <PassThroughDoor/>
    </Sequence>
  </BehaviorTree>
  <BehaviorTree ID="DoorClosed">
    <Fallback>
      <OpenDoor/>
      <RetryUntilSuccessful num_attempts="5"><PickLock/></RetryUntilSuccessful>
      <SmashDoor/>
    </Fallback>
  </BehaviorTree>
</root>
""".strip()
    )
    print("t05:", tree.tick_while_running(1))


def run_t08() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(ActionA, 42, "hello_world")
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <ActionA out_a="{out_a}"/>
  </BehaviorTree>
</root>
""".strip()
    )
    print("t08:", tree.tick_once(), tree.root_blackboard().get("out_a"))


def run_t11_ex03_sqlite() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Sleep msec="1"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    with tempfile.TemporaryDirectory() as td:
        btlog = Path(td) / "tree.btlog"
        trace = Path(td) / "trace.json"
        db = Path(td) / "tree.db3"
        _pub = bt.Groot2Publisher(tree, 17777)
        flog = bt.FileLogger2(tree, str(btlog))
        mlog = bt.MinitraceLogger(tree, str(trace))
        slog = bt.SqliteLogger(tree, str(db), False)
        print("t11/ex03:", tree.tick_while_running(1))
        flog.flush()
        mlog.flush()
        slog.flush()
        print("logs:", btlog.exists(), trace.exists(), db.exists())
        del slog
        del mlog
        del flog
        del _pub


def run_t12() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(NodeWithDefaultPorts)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <NodeWithDefaultPorts input="-1"/>
  </BehaviorTree>
</root>
""".strip()
    )
    bb = tree.root_blackboard()
    bb.set("point", 3)
    bb.set("pointD", 7)
    print("t12:", tree.tick_once())


def run_t13() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(PushIntoVector)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <PushIntoVector vector="{vect}" value="3"/>
      <PushIntoVector vector="{vect}" value="5"/>
      <PushIntoVector vector="{vect}" value="7"/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    print("t13:", tree.tick_once(), tree.root_blackboard().get("vect"))


def run_t18() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_loop_node("LoopPose", "pose2d")
    factory.register_sync_action(GenerateWaypoints)
    factory.register_sync_action(UseWaypoint)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <GenerateWaypoints waypoints="{wps}"/>
      <LoopPose queue="{wps}" value="{wp}">
        <UseWaypoint waypoint="{wp}"/>
      </LoopPose>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    print("t18:", tree.tick_while_running(1))


def main() -> None:
    run_t02_ex02()
    run_t04()
    run_t05()
    run_t08()
    run_t11_ex03_sqlite()
    run_t12()
    run_t13()
    run_t18()


if __name__ == "__main__":
    main()
