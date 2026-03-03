from __future__ import annotations

import os
import sqlite3
import tempfile
import time
from pathlib import Path

import pytest

import behaviortree_py as bt


def _install_prefix() -> Path:
    return Path(
        os.environ.get("BTCPP_INSTALL_PREFIX", "/home/rishab_vm/tools/BehaviorTree.CPP_4_8_3/install")
    )


class BatteryOK(bt.ConditionNode):
    calls = 0

    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        type(self).calls += 1
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
        _ = self.get_input("message")
        return bt.NodeStatus.SUCCESS


class CalculateGoal(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "goal", "type": "str"}]}

    def tick(self) -> bt.NodeStatus:
        self.set_output("goal", "1.1;2.3")
        return bt.NodeStatus.SUCCESS


class PrintTarget(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [{"name": "target", "type": "str"}], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        target = self.get_input("target")
        if not isinstance(target, str):
            return bt.NodeStatus.FAILURE
        parts = target.split(";")
        if len(parts) != 2:
            return bt.NodeStatus.FAILURE
        _ = (float(parts[0]), float(parts[1]))
        return bt.NodeStatus.SUCCESS


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


class ActionB(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "out_b", "type": "str"}]}

    def __init__(self, name: str, arg_int: int, arg_str: str):
        super().__init__(name)
        self._arg_int = arg_int
        self._arg_str = arg_str

    def tick(self) -> bt.NodeStatus:
        self.set_output("out_b", f"{self._arg_int}/{self._arg_str}")
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
        if self.get_input("input") != -1:
            return bt.NodeStatus.FAILURE
        if self.get_input("pointA") != 1:
            return bt.NodeStatus.FAILURE
        if self.get_input("pointB") != 3:
            return bt.NodeStatus.FAILURE
        if self.get_input("pointC") != 5:
            return bt.NodeStatus.FAILURE
        if self.get_input("pointD") != 7:
            return bt.NodeStatus.FAILURE
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


class UseWaypoint(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [{"name": "waypoint", "type": "pose2d"}], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        wp = self.get_input("waypoint")
        if not isinstance(wp, dict):
            return bt.NodeStatus.FAILURE
        if "x" not in wp or "y" not in wp or "theta" not in wp:
            return bt.NodeStatus.FAILURE
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


def test_t01_and_ex01_wrap_legacy_style_nodes() -> None:
    class MoveTo(bt.SyncActionNode):
        @classmethod
        def provided_ports(cls):
            return {"inputs": [{"name": "goal", "type": "str"}], "outputs": []}

        def tick(self) -> bt.NodeStatus:
            goal = self.get_input("goal")
            parts = [float(x) for x in goal.split(";")]
            assert len(parts) == 3
            return bt.NodeStatus.SUCCESS

    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(MoveTo)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <MoveTo goal="-1;3;0.5"/>
  </BehaviorTree>
</root>
""".strip()
    )
    assert tree.tick_once() == bt.NodeStatus.SUCCESS


def test_t02_basic_ports_and_ex02_runtime_ports_parity() -> None:
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
    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert tree.root_blackboard().get("the_answer") == "The answer is 42"


def test_t04_reactive_sequence_calls_condition_each_tick() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_condition(BatteryOK)
    factory.register_stateful_action(MoveBaseAsync)

    BatteryOK.calls = 0
    seq_tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <BatteryOK/>
      <MoveBaseAsync/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    assert seq_tree.tick_while_running(1) == bt.NodeStatus.SUCCESS
    seq_calls = BatteryOK.calls

    BatteryOK.calls = 0
    reactive_tree = factory.create_tree_from_text(
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
    assert reactive_tree.tick_while_running(1) == bt.NodeStatus.SUCCESS
    reactive_calls = BatteryOK.calls

    assert seq_calls == 1
    assert reactive_calls > seq_calls


def test_t03_generic_ports_style_with_json_and_string_conversion() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(CalculateGoal)
    factory.register_sync_action(PrintTarget)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <CalculateGoal goal="{GoalPosition}"/>
      <PrintTarget target="{GoalPosition}"/>
      <Script code="OtherGoal:='-1;3'"/>
      <PrintTarget target="{OtherGoal}"/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    assert tree.tick_once() == bt.NodeStatus.SUCCESS


def test_t05_crossdoor_with_plugin_nodes() -> None:
    plugin = _install_prefix() / "share" / "behaviortree_cpp" / "bt_plugins" / "libcrossdoor_nodes_dyn.so"
    if not plugin.exists():
        pytest.skip(f"crossdoor plugin not found: {plugin}")

    factory = bt.BehaviorTreeFactory()
    factory.register_from_plugin(str(plugin))
    factory.register_behavior_tree_from_text(
        """
<root BTCPP_format="4">
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
    status = factory.create_tree("MainTree").tick_while_running(1)
    assert status in (bt.NodeStatus.SUCCESS, bt.NodeStatus.FAILURE)


def test_t08_additional_constructor_args() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(ActionA, 42, "hello_world")
    factory.register_sync_action(ActionB, 69, "interesting_value")

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <ActionA out_a="{out_a}"/>
          <ActionB out_b="{out_b}"/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    bb = tree.root_blackboard()
    assert bb.get("out_a") == "42/hello_world"
    assert bb.get("out_b") == "69/interesting_value"


def test_t11_groot_and_file_loggers() -> None:
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
        minij = Path(td) / "trace.json"
        _publisher = bt.Groot2Publisher(tree, 17777)
        flog = bt.FileLogger2(tree, str(btlog))
        mlog = bt.MinitraceLogger(tree, str(minij))
        assert tree.tick_while_running(1) == bt.NodeStatus.SUCCESS
        flog.flush()
        mlog.flush()
        assert btlog.exists()
        assert minij.exists()


def test_t12_default_ports_parity() -> None:
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
    assert tree.tick_once() == bt.NodeStatus.SUCCESS


def test_t13_inout_vector_accumulation() -> None:
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
    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert tree.root_blackboard().get("vect") == [3, 5, 7]


def test_t15_nodes_mocking_json_rules() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.clear_substitution_rules()
    json_text = r"""
{
  "TestNodeConfigs": {
    "NewMessage": {"async_delay": 0, "return_status": "SUCCESS", "post_script": "msg :='message SUBSTITUTED'"},
    "NoCounting": {"return_status": "SUCCESS"}
  },
  "SubstitutionRules": {
    "mysub/action_*": "AlwaysSuccess",
    "set_message": "NewMessage",
    "counting": "NoCounting"
  }
}
""".strip()
    factory.load_substitution_rules_from_json(json_text)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <SubTree ID="MySub" name="mysub"/>
      <Script name="set_message" code="msg:= 'the original message'"/>
      <Sequence name="counting">
        <AlwaysSuccess/>
        <AlwaysSuccess/>
      </Sequence>
    </Sequence>
  </BehaviorTree>
  <BehaviorTree ID="MySub">
    <Sequence>
      <AlwaysSuccess name="action_subA"/>
      <AlwaysSuccess name="action_subB"/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    assert tree.tick_while_running(1) == bt.NodeStatus.SUCCESS
    assert tree.root_blackboard().get("msg") == "message SUBSTITUTED"


def test_t18_waypoints_pattern_with_python_decorator() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(GenerateWaypoints)
    factory.register_loop_node("LoopPose", "pose2d")
    factory.register_sync_action(UseWaypoint)
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <LoopDouble queue="1;2;3" value="{num}">
        <AlwaysSuccess/>
      </LoopDouble>
      <GenerateWaypoints waypoints="{wps}"/>
      <LoopPose queue="{wps}" value="{wp}">
        <UseWaypoint waypoint="{wp}"/>
      </LoopPose>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )
    assert tree.tick_while_running(1) == bt.NodeStatus.SUCCESS


def test_ex03_sqlite_logger_core_flow() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="type:='A'"/>
      <Sleep msec="1"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    with tempfile.TemporaryDirectory() as td:
        db = Path(td) / "log.db3"
        logger = bt.SqliteLogger(tree, str(db), False)
        assert tree.tick_while_running(1) == bt.NodeStatus.SUCCESS
        logger.flush()
        deadline = time.time() + 2.0
        while time.time() < deadline and not db.exists():
            time.sleep(0.05)
        assert db.exists()

        with sqlite3.connect(db) as conn:
            rows = conn.execute("SELECT COUNT(*) FROM sqlite_master").fetchone()
            assert rows is not None
            assert rows[0] > 0
        del logger
