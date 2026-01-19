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

    samples = [
        True,
        123,
        1.25,
        "hello",
        [1, 2, 3],
        {"a": 1, "b": [1, 2, 3]},
        None,
        [],
    ]

    for value in samples:
        bb.set("in_val", value)
        status = tree.tick_once()
        out = bb.get("out_val")
        print("in:", value, "status:", status, "out:", out)

    try:
        bb.set("in_val", [1, "a"])  # mixed list is rejected
        tree.tick_once()
    except Exception as exc:  # noqa: BLE001
        print("expected error:", exc)


if __name__ == "__main__":
    main()

