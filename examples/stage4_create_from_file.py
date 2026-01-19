from pathlib import Path

import behaviortree_py as bt


def main() -> None:
    tree_xml = Path(__file__).resolve().parent / "trees" / "always_success.xml"
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_file(str(tree_xml))
    print("loaded:", tree_xml)
    print("tick_once ->", tree.tick_once())


if __name__ == "__main__":
    main()

