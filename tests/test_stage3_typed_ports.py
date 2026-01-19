import pytest

import behaviortree_py as bt


class TypedCopyInt(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {
            "inputs": [{"name": "in", "type": "int"}],
            "outputs": [{"name": "out", "type": "int"}],
        }

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("in")
        self.set_output("out", value)
        return bt.NodeStatus.SUCCESS


def test_typed_ports_can_roundtrip_and_convert_from_string() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(TypedCopyInt)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <TypedCopyInt in="{in_val}" out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()
    )
    bb = tree.root_blackboard()

    bb.set("in_val", "123")
    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert bb.get("out_val") == 123


class TypedRejectJson(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "out", "type": "int"}]}

    def tick(self) -> bt.NodeStatus:
        self.set_output("out", {"a": 1})
        return bt.NodeStatus.SUCCESS


def test_typed_int_port_rejects_dict_output() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(TypedRejectJson)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <TypedRejectJson out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()
    )

    with pytest.raises(TypeError, match=r"dict is only allowed for JSON ports"):
        tree.tick_once()


class TypedRejectNone(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "out", "type": "int"}]}

    def tick(self) -> bt.NodeStatus:
        self.set_output("out", None)
        return bt.NodeStatus.SUCCESS


def test_typed_int_port_rejects_none_output() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(TypedRejectNone)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <TypedRejectNone out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()
    )

    with pytest.raises(TypeError, match=r"None is only allowed for JSON ports"):
        tree.tick_once()


class TypedEmitEmptyList(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {"inputs": [], "outputs": [{"name": "out", "type": "list[int]"}]}

    def tick(self) -> bt.NodeStatus:
        self.set_output("out", [])
        return bt.NodeStatus.SUCCESS


def test_typed_list_allows_empty_list() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(TypedEmitEmptyList)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <TypedEmitEmptyList out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()
    )
    bb = tree.root_blackboard()

    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert bb.get("out_val") == []

