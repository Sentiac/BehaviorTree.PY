import behaviortree_py as bt


class PyInverter(bt.DecoratorNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        child_status = self.tick_child()
        if child_status == bt.NodeStatus.SUCCESS:
            return bt.NodeStatus.FAILURE
        if child_status == bt.NodeStatus.FAILURE:
            return bt.NodeStatus.SUCCESS
        return child_status


class Sequence2(bt.ControlNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        for i in range(self.children_count):
            status = self.tick_child(i)
            if status != bt.NodeStatus.SUCCESS:
                return status
        return bt.NodeStatus.SUCCESS


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_decorator(PyInverter)
    factory.register_control(Sequence2)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence2>
      <PyInverter>
        <AlwaysSuccess/>
      </PyInverter>
      <AlwaysSuccess/>
    </Sequence2>
  </BehaviorTree>
</root>
""".strip()
    )

    print("tree tick:", tree.tick_once())


if __name__ == "__main__":
    main()

