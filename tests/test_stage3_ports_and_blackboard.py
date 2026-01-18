import pytest

import behaviortree_py as bt


class CopyInToOut(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": ["in"], "outputs": ["out"]}

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("in")
        self.set_output("out", value)
        return bt.NodeStatus.SUCCESS


@pytest.mark.parametrize(
    "value",
    [
        True,
        123,
        1.25,
        "hello",
        [1, 2, 3],
        [1.0, 2.0, 3.0],
        ["a", "b"],
        [True, False, True],
        {"a": 1, "b": [1, 2, 3]},
        None,
        [],
    ],
)
def test_ports_roundtrip_via_blackboard(value) -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(CopyInToOut)

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <CopyInToOut in="{in_val}" out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()

    tree = factory.create_tree_from_text(xml)
    bb = tree.root_blackboard()

    bb.set("in_val", value)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert bb.get("out_val") == value


class BadOutMixedList(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": ["out"]}

    def tick(self) -> bt.NodeStatus:
        self.set_output("out", [1, "a"])
        return bt.NodeStatus.SUCCESS


def test_mixed_list_rejected() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(BadOutMixedList)

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <BadOutMixedList out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()

    tree = factory.create_tree_from_text(xml)

    with pytest.raises(TypeError):
        tree.tick_once()

