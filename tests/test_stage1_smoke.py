import behaviortree_py as bt


def test_import_and_versions() -> None:
    assert bt.__version__
    assert bt.btcpp_version_string()
    assert bt.btcpp_version_number() > 0


def test_tick_always_success() -> None:
    factory = bt.BehaviorTreeFactory()

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()

    tree = factory.create_tree_from_text(xml)
    status = tree.tick_once()

    assert status == bt.NodeStatus.SUCCESS
