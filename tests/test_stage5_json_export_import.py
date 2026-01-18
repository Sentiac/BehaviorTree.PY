import behaviortree_py as bt


class SetOut(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": ["in"], "outputs": ["out"]}

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("in")
        self.set_output("out", value)
        return bt.NodeStatus.SUCCESS


def test_tree_to_json_roundtrip() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(SetOut)

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <SetOut in="{in_val}" out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()

    tree1 = factory.create_tree_from_text(xml)
    bb1 = tree1.root_blackboard()

    bb1.set("in_val", {"a": 1, "b": [1, 2, 3]})
    tree1.tick_once()

    exported = tree1.to_json()

    tree2 = factory.create_tree_from_text(xml)
    tree2.from_json(exported)
    bb2 = tree2.root_blackboard()

    assert bb2.get("in_val") == {"a": 1, "b": [1, 2, 3]}
    assert bb2.get("out_val") == {"a": 1, "b": [1, 2, 3]}


def test_blackboard_to_json_roundtrip() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <AlwaysSuccess/>
  </BehaviorTree>
</root>
""".strip()
    )
    bb = tree.root_blackboard()

    bb.set("x", [1, 2, 3])
    exported = bb.to_json()

    tree2 = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <AlwaysSuccess/>
  </BehaviorTree>
</root>
""".strip()
    )
    bb2 = tree2.root_blackboard()
    bb2.from_json(exported)

    assert bb2.get("x") == [1, 2, 3]

