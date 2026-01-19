import behaviortree_py as bt


class TypedCopyInt(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls):
        return {
            "inputs": [{"name": "in", "type": "int"}],
            "outputs": [{"name": "out", "type": "int"}],
        }

    def tick(self) -> bt.NodeStatus:
        value = self.get_input("in")
        self.set_output("out", value)
        return bt.NodeStatus.SUCCESS


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(TypedCopyInt)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <TypedCopyInt in="{in_val}" out="{out_val}"/>
  </BehaviorTree>
</root>
""".strip()
    )
    bb = tree.root_blackboard()

    bb.set("in_val", "123")
    print("tick:", tree.tick_once())
    print("out_val:", bb.get("out_val"), type(bb.get("out_val")))


if __name__ == "__main__":
    main()

