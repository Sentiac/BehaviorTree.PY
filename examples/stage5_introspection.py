import behaviortree_py as bt


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Sleep msec="1"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    root = tree.root_node
    assert root is not None

    print("root:", root.name, root.type, root.full_path)
    print("children:", [c.name for c in root.children()])

    def visitor(node: bt.TreeNode) -> None:
        print("-", node.full_path, node.type, node.status)

    tree.apply_visitor(visitor)

    sleep_nodes = tree.get_nodes_by_path("*Sleep*")
    for node in sleep_nodes:
        print("ports for", node.full_path, node.ports)


if __name__ == "__main__":
    main()

