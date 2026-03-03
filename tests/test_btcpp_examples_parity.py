import behaviortree_py as bt


SUBTREE_REMAP_XML = """
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


MULTI_TREE_XML = """
<root BTCPP_format="4">
  <BehaviorTree ID="SuccessTree">
    <AlwaysSuccess/>
  </BehaviorTree>
  <BehaviorTree ID="FailureTree">
    <AlwaysFailure/>
  </BehaviorTree>
</root>
""".strip()

SCRIPTING_T09_XML = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="msg:='hello world'"/>
      <Script code="A:=THE_ANSWER; B:=3.14; color:=RED"/>
      <Precondition if="A>-B && color != BLUE" else="FAILURE">
        <Sequence>
          <AlwaysSuccess/>
        </Sequence>
      </Precondition>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()

SUBTREE_MODEL_T14_XML = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <TreeNodesModel>
    <SubTree ID="MySub">
      <input_port name="sub_in_value" default="42"/>
      <input_port name="sub_in_name"/>
      <output_port name="sub_out_result" default="{out_result}"/>
      <output_port name="sub_out_state"/>
    </SubTree>
  </TreeNodesModel>

  <BehaviorTree ID="MySub">
    <Sequence>
      <ScriptCondition code="sub_in_value==42 && sub_in_name=='john'"/>
      <Script code="sub_out_result:=69; sub_out_state:='ACTIVE'"/>
    </Sequence>
  </BehaviorTree>

  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="in_name:='john'"/>
      <SubTree ID="MySub" sub_in_name="{in_name}" sub_out_state="{out_state}"/>
      <ScriptCondition code="out_result==69 && out_state=='ACTIVE'"/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()

GLOBAL_BB_T16_XML = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="@value_sqr:=@value*@value"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()

BACKUP_T17_XML = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="val_A:='john'"/>
      <Script code="val_B:=42"/>
      <SubTree ID="Sub" val="{val_A}" _autoremap="true"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
  <BehaviorTree ID="Sub">
    <Sequence>
      <Script code="reply:='done'"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()


def test_subtree_port_remapping_parity() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(SUBTREE_REMAP_XML)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert tree.root_blackboard().get("x") == 42


def test_register_multiple_trees_from_one_xml() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_behavior_tree_from_text(MULTI_TREE_XML)

    assert sorted(factory.registered_behavior_trees()) == ["FailureTree", "SuccessTree"]
    assert factory.create_tree("SuccessTree").tick_once() == bt.NodeStatus.SUCCESS
    assert factory.create_tree("FailureTree").tick_once() == bt.NodeStatus.FAILURE


def test_observer_monitor_with_apply_visitor() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Sleep msec="200"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    before: dict[str, bt.NodeStatus] = {}

    def before_visitor(node: bt.TreeNode) -> None:
        before[node.full_path] = node.status

    tree.apply_visitor(before_visitor)
    assert tree.tick_exactly_once() == bt.NodeStatus.RUNNING

    after: dict[str, bt.NodeStatus] = {}

    def after_visitor(node: bt.TreeNode) -> None:
        after[node.full_path] = node.status

    tree.apply_visitor(after_visitor)
    assert before
    assert after
    assert any(status == bt.NodeStatus.RUNNING for status in after.values())
    tree.halt_tree()


def test_global_blackboard_backup_restore() -> None:
    factory = bt.BehaviorTreeFactory()
    shared = bt.Blackboard.create()
    shared.set("counter", 0)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="counter:=counter+1"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip(),
        shared,
    )

    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert shared.get("counter") == 1

    backup = tree.backup_blackboards()
    shared.set("counter", 99)
    tree.restore_blackboards(backup)

    assert shared.get("counter") == 1


def test_scripting_enums_parity_t09() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_scripting_enum("THE_ANSWER", 42)
    factory.register_scripting_enums({"RED": 1, "BLUE": 2, "GREEN": 3})

    tree = factory.create_tree_from_text(SCRIPTING_T09_XML)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

    bb = tree.root_blackboard()
    assert bb.get("A") == 42.0
    assert bb.get("B") == 3.14
    assert bb.get("color") == 1.0
    assert bb.get("msg") == "hello world"


def test_subtree_model_defaults_parity_t14() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(SUBTREE_MODEL_T14_XML)

    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    bb = tree.root_blackboard()
    assert bb.get("out_result") == 69.0
    assert bb.get("out_state") == "ACTIVE"


def test_global_blackboard_prefix_parity_t16() -> None:
    factory = bt.BehaviorTreeFactory()

    global_bb = bt.Blackboard.create()
    root_bb = bt.Blackboard.create(global_bb)
    tree = factory.create_tree_from_text(GLOBAL_BB_T16_XML, root_bb)

    global_bb.set("value", 5)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    assert global_bb.get("value_sqr") == 25.0


def test_blackboard_backup_restore_and_json_roundtrip_t17() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(BACKUP_T17_XML)

    backup = tree.backup_blackboards()
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

    bb = tree.root_blackboard()
    assert bb.get("val_A") == "john"
    assert bb.get("val_B") == 42.0
    assert bb.get("reply") == "done"

    tree.restore_blackboards(backup)
    assert tree.tick_once() == bt.NodeStatus.SUCCESS

    exported = tree.to_bt_json()
    assert "MainTree" in exported
    tree.from_bt_json(exported)
