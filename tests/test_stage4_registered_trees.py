import behaviortree_py as bt


def test_register_behavior_tree_from_text_and_create() -> None:
    factory = bt.BehaviorTreeFactory()

    xml = """
<root BTCPP_format="4">
  <BehaviorTree ID="MyTree">
    <AlwaysSuccess/>
  </BehaviorTree>
</root>
""".strip()

    factory.register_behavior_tree_from_text(xml)
    assert "MyTree" in factory.registered_behavior_trees()

    tree = factory.create_tree("MyTree")
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

    factory.clear_registered_behavior_trees()
    assert factory.registered_behavior_trees() == []
