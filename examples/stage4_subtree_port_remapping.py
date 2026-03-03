import behaviortree_py as bt


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="x:=41"/>
      <SubTree ID="AddOne" in="{x}" out="{x}"/>
    </Sequence>
  </BehaviorTree>

  <BehaviorTree ID="AddOne">
    <Script code="out:=in+1"/>
  </BehaviorTree>
</root>
""".strip()
    )

    status = tree.tick_once()
    x_val = tree.root_blackboard().get("x")
    print("tick:", status)
    print("x:", x_val)


if __name__ == "__main__":
    main()
