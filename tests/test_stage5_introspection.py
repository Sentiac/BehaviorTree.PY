import behaviortree_py as bt


def test_tree_root_node_and_children() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    root = tree.root_node
    assert root is not None
    assert root.type == bt.NodeType.CONTROL
    assert root.name == "Sequence"

    children = root.children()
    assert len(children) == 1
    assert children[0].name == "AlwaysSuccess"


def test_get_nodes_by_path_matches() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    matched = tree.get_nodes_by_path("*")
    assert any(n.name == "Sequence" for n in matched)
    assert any(n.name == "AlwaysSuccess" for n in matched)


def test_tree_node_ports_introspection() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sleep msec="1"/>
  </BehaviorTree>
</root>
""".strip()
    )

    root = tree.root_node
    assert root is not None
    ports = root.ports
    assert "msec" in ports
    assert ports["msec"]["direction"] == bt.PortDirection.INPUT


def test_tree_apply_visitor() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    visited: list[str] = []

    def visitor(node: bt.TreeNode) -> None:
        visited.append(node.name)

    tree.apply_visitor(visitor)
    assert "Sequence" in visited
    assert "AlwaysSuccess" in visited
