import behaviortree_py as bt


def test_create_tree_from_file(tmp_path) -> None:
    tree_xml = tmp_path / "tree.xml"
    tree_xml.write_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <AlwaysSuccess/>
  </BehaviorTree>
</root>
""".strip()
    )

    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_file(str(tree_xml))
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

