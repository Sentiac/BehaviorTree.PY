import behaviortree_py as bt


class SetOut(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": ["in"], "outputs": ["out"]}

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("in")
        self.set_output("out", value)
        return bt.NodeStatus.SUCCESS


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(SetOut)

    xml = """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <SetOut in="{in_val}" out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()

    tree1 = factory.create_tree_from_text(xml)
    bb1 = tree1.root_blackboard()
    bb1.set("in_val", {"a": 1, "b": [1, 2, 3]})
    tree1.tick_once()

    exported = tree1.to_json()
    print("exported:", exported)

    tree2 = factory.create_tree_from_text(xml)
    tree2.from_json(exported)
    bb2 = tree2.root_blackboard()
    print("roundtrip in_val:", bb2.get("in_val"))
    print("roundtrip out_val:", bb2.get("out_val"))


if __name__ == "__main__":
    main()

