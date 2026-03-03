import behaviortree_py as bt


XML = """
<root BTCPP_format="4">
  <BehaviorTree ID="SuccessTree">
    <AlwaysSuccess/>
  </BehaviorTree>

  <BehaviorTree ID="FailureTree">
    <AlwaysFailure/>
  </BehaviorTree>
</root>
""".strip()


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_behavior_tree_from_text(XML)

    print("registered:", sorted(factory.registered_behavior_trees()))

    success_tree = factory.create_tree("SuccessTree")
    failure_tree = factory.create_tree("FailureTree")

    print("SuccessTree:", success_tree.tick_once())
    print("FailureTree:", failure_tree.tick_once())


if __name__ == "__main__":
    main()
