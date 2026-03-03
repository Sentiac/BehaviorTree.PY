import pprint

import behaviortree_py as bt


def snapshot_statuses(tree: bt.Tree) -> dict[str, bt.NodeStatus]:
    out: dict[str, bt.NodeStatus] = {}

    def visitor(node: bt.TreeNode) -> None:
        out[node.full_path] = node.status

    tree.apply_visitor(visitor)
    return out


def main() -> None:
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

    before = snapshot_statuses(tree)
    status = tree.tick_exactly_once()
    after = snapshot_statuses(tree)

    transitions: dict[str, tuple[bt.NodeStatus, bt.NodeStatus]] = {}
    for path, old_status in before.items():
        new_status = after.get(path, old_status)
        if new_status != old_status:
            transitions[path] = (old_status, new_status)

    print("tick:", status)
    print("transitions:")
    pprint.pprint(transitions)
    tree.halt_tree()


if __name__ == "__main__":
    main()
