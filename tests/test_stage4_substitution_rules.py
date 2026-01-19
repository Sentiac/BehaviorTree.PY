import behaviortree_py as bt


def test_substitution_rule_string_replaces_node_type() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.clear_substitution_rules()
    factory.add_substitution_rule("*AlwaysFailure*", "AlwaysSuccess")

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <AlwaysFailure/>
  </BehaviorTree>
</root>
""".strip()
    )
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

    rules = factory.substitution_rules()
    assert rules["*AlwaysFailure*"] == "AlwaysSuccess"


def test_substitution_rule_test_node_config_can_force_status() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.clear_substitution_rules()
    factory.add_substitution_rule("*AlwaysSuccess*", {"return_status": bt.NodeStatus.FAILURE})

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <AlwaysSuccess/>
  </BehaviorTree>
</root>
""".strip()
    )
    assert tree.tick_once() == bt.NodeStatus.FAILURE


def test_load_substitution_rules_from_json_exposes_rules() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.clear_substitution_rules()

    json_text = r"""
{
  "TestNodeConfigs": {
    "ForceFailure": {
      "return_status": "FAILURE",
      "async_delay": 0
    }
  },
  "SubstitutionRules": {
    "actionA": "ForceFailure",
    "actionB": "AlwaysSuccess"
  }
}
""".strip()

    factory.load_substitution_rules_from_json(json_text)
    rules = factory.substitution_rules()

    assert "actionA" in rules
    assert rules["actionA"]["return_status"] == bt.NodeStatus.FAILURE
    assert rules["actionA"]["async_delay_ms"] == 0
    assert rules["actionB"] == "AlwaysSuccess"

