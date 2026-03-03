import behaviortree_py as bt


XML = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="a:=2; b:=40"/>
      <SubTree ID="Accumulate" lhs="{a}" rhs="{b}" out="{sum}"/>
      <Script code="done:=sum==42"/>
    </Sequence>
  </BehaviorTree>

  <BehaviorTree ID="Accumulate">
    <Sequence>
      <Script code="out:=lhs+rhs"/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_behavior_tree_from_text(XML)
    tree = factory.create_tree("MainTree")

    status = tree.tick_once()
    bb = tree.root_blackboard()
    print("tick:", status)
    print("sum:", bb.get("sum"))
    print("done:", bb.get("done"))


if __name__ == "__main__":
    main()
