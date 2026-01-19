import behaviortree_py as bt


class CopyInToOut(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": ["in"], "outputs": ["out"]}

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("in")
        self.set_output("out", value)
        return bt.NodeStatus.SUCCESS


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(CopyInToOut)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <CopyInToOut in="{in_val}" out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()
    )

    bb = tree.root_blackboard()
    bb.set("in_val", {"hello": [1, 2, 3]})

    print("tick_once ->", tree.tick_once())
    print("out_val ->", bb.get("out_val"))


if __name__ == "__main__":
    main()

