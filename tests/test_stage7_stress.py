import gc
import weakref

import behaviortree_py as bt


def test_repeat_create_tick_destroy_builtin_tree() -> None:
    factory = bt.BehaviorTreeFactory()

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <AlwaysSuccess/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()

    for i in range(200):
        tree = factory.create_tree_from_text(xml)
        assert tree.tick_once() == bt.NodeStatus.SUCCESS
        del tree
        if i % 25 == 0:
            gc.collect()


class _LifetimeProbe(bt.SyncActionNode):
    refs: list[weakref.ReferenceType] = []

    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def __init__(self, name: str) -> None:
        super().__init__(name)
        type(self).refs.append(weakref.ref(self))

    def tick(self) -> bt.NodeStatus:
        return bt.NodeStatus.SUCCESS


def test_repeat_create_tick_destroy_releases_python_node_instances() -> None:
    _LifetimeProbe.refs = []

    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(_LifetimeProbe)

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <_LifetimeProbe/>
  </BehaviorTree>
</root>
""".strip()

    for _ in range(100):
        tree = factory.create_tree_from_text(xml)
        assert tree.tick_once() == bt.NodeStatus.SUCCESS
        del tree
        gc.collect()

    assert _LifetimeProbe.refs
    assert all(ref() is None for ref in _LifetimeProbe.refs)


def test_python_node_instances_stay_alive_while_tree_exists() -> None:
    _LifetimeProbe.refs = []

    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(_LifetimeProbe)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <_LifetimeProbe/>
  </BehaviorTree>
</root>
""".strip()
    )
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

    assert _LifetimeProbe.refs
    assert any(ref() is not None for ref in _LifetimeProbe.refs)

    del tree
    gc.collect()
    assert all(ref() is None for ref in _LifetimeProbe.refs)
