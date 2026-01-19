import behaviortree_py as bt


class IncrementInOut(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": [], "inouts": ["x"]}

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("x")
        self.set_output("x", value + 1)
        return bt.NodeStatus.SUCCESS


def test_inout_port_roundtrip() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(IncrementInOut)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <IncrementInOut x="{x}"/>
  </BehaviorTree>
</root>
""".strip()
    )

    bb = tree.root_blackboard()
    bb.set("x", 1)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert bb.get("x") == 2
