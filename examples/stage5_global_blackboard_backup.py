import behaviortree_py as bt


XML = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="counter:=counter+1"/>
      <AlwaysSuccess/>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()


def main() -> None:
    factory = bt.BehaviorTreeFactory()

    shared_bb = bt.Blackboard.create()
    shared_bb.set("counter", 0)
    tree = factory.create_tree_from_text(XML, shared_bb)

    print("tick1:", tree.tick_once(), "counter:", shared_bb.get("counter"))

    backup = tree.backup_blackboards()

    shared_bb.set("counter", 99)
    print("mutated counter:", shared_bb.get("counter"))

    tree.restore_blackboards(backup)
    print("restored counter:", shared_bb.get("counter"))


if __name__ == "__main__":
    main()
