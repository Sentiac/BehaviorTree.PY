import behaviortree_py as bt


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.clear_substitution_rules()

    factory.add_substitution_rule("*AlwaysFailure*", "AlwaysSuccess")
    factory.add_substitution_rule("*AlwaysSuccess*", {"return_status": bt.NodeStatus.FAILURE})

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <AlwaysFailure/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    print("tick:", tree.tick_once())
    print("rules:", factory.substitution_rules())


if __name__ == "__main__":
    main()

