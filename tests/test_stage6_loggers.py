import behaviortree_py as bt


def test_file_logger2_writes_log(tmp_path) -> None:
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

    path = tmp_path / "test.btlog"
    logger = bt.FileLogger2(tree, str(path))

    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    logger.flush()

    assert path.exists()
    assert path.stat().st_size > 0


def test_minitrace_logger_writes_trace(tmp_path) -> None:
    import gc

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

    path = tmp_path / "trace.json"
    logger = bt.MinitraceLogger(tree, str(path))

    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    logger.flush()
    del logger
    gc.collect()

    assert path.exists()
    assert path.stat().st_size > 0
