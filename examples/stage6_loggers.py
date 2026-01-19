import gc
import tempfile

import behaviortree_py as bt


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <AlwaysSuccess/>
  </BehaviorTree>
</root>
""".strip()
    )

    print("tick:", tree.tick_once())

    bt.StdCoutLogger(tree).flush()

    with tempfile.TemporaryDirectory() as tmpdir:
        path_btlog = f"{tmpdir}/tree.btlog"
        file_logger = bt.FileLogger2(tree, path_btlog)
        tree.tick_once()
        file_logger.flush()
        print("wrote:", path_btlog)
        del file_logger

        path_trace = f"{tmpdir}/trace.json"
        minitrace = bt.MinitraceLogger(tree, path_trace)
        tree.tick_once()
        minitrace.flush()
        del minitrace
        gc.collect()
        print("wrote:", path_trace)


if __name__ == "__main__":
    main()

