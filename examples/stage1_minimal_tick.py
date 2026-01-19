import behaviortree_py as bt


def main() -> None:
    print("BehaviorTree.PY version:", bt.__version__)
    print("BehaviorTree.CPP version:", bt.btcpp_version_string())

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

    print("tick_once ->", tree.tick_once())
    print("nodes:", [n.full_path for n in tree.nodes()])


if __name__ == "__main__":
    main()

