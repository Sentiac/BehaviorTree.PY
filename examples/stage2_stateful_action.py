import behaviortree_py as bt


class TwoTickSuccess(bt.StatefulActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self._ticks = 0

    def on_start(self) -> bt.NodeStatus:
        self._ticks = 0
        print("on_start")
        return bt.NodeStatus.RUNNING

    def on_running(self) -> bt.NodeStatus:
        self._ticks += 1
        print("on_running", self._ticks)
        return bt.NodeStatus.SUCCESS if self._ticks >= 2 else bt.NodeStatus.RUNNING

    def on_halted(self) -> None:
        print("on_halted")


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_stateful_action(TwoTickSuccess)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <TwoTickSuccess/>
  </BehaviorTree>
</root>
""".strip()
    )

    print("tick_while_running ->", tree.tick_while_running(sleep_ms=0))


if __name__ == "__main__":
    main()

