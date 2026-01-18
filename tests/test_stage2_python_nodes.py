import behaviortree_py as bt


class MySync(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        return bt.NodeStatus.SUCCESS


class MyStateful(bt.StatefulActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self._ticks = 0

    def on_start(self) -> bt.NodeStatus:
        self._ticks = 0
        return bt.NodeStatus.RUNNING

    def on_running(self) -> bt.NodeStatus:
        self._ticks += 1
        return bt.NodeStatus.SUCCESS

    def on_halted(self) -> None:
        return None


def test_python_sync_action_node_ticks() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(MySync)

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <MySync/>
  </BehaviorTree>
</root>
""".strip()

    tree = factory.create_tree_from_text(xml)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS


def test_python_stateful_action_node_ticks() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_stateful_action(MyStateful)

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <MyStateful/>
  </BehaviorTree>
</root>
""".strip()

    tree = factory.create_tree_from_text(xml)
    assert tree.tick_while_running(sleep_ms=0) == bt.NodeStatus.SUCCESS

