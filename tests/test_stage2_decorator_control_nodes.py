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


def test_python_decorator_node_ticks_child() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_decorator(PyInverter)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <PyInverter>
      <AlwaysSuccess/>
    </PyInverter>
  </BehaviorTree>
</root>
""".strip()
    )

    assert tree.tick_once() == bt.NodeStatus.FAILURE


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


def test_python_control_node_ticks_children() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_control(Sequence2)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence2>
      <AlwaysSuccess/>
      <AlwaysFailure/>
    </Sequence2>
  </BehaviorTree>
</root>
""".strip()
    )

    assert tree.tick_once() == bt.NodeStatus.FAILURE


class HaltRecorder(bt.ControlNode):
    halt_calls = 0

    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        return self.tick_child(0)

    def halt(self) -> None:
        type(self).halt_calls += 1


def test_python_control_node_halt_callback_called() -> None:
    HaltRecorder.halt_calls = 0

    factory = bt.BehaviorTreeFactory()
    factory.register_control(HaltRecorder)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <HaltRecorder>
      <Sleep msec="200"/>
    </HaltRecorder>
  </BehaviorTree>
</root>
""".strip()
    )

    assert tree.tick_once() == bt.NodeStatus.RUNNING
    tree.halt_tree()

    assert HaltRecorder.halt_calls == 1
