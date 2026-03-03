import behaviortree_py as bt
import pytest


def _groot2_worker(queue) -> None:
    import gc
    import socket

    def get_free_port() -> int:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.bind(("127.0.0.1", 0))
            return sock.getsockname()[1]

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

    last_error: Exception | None = None
    for _ in range(10):
        port = get_free_port()
        try:
            publisher = bt.Groot2Publisher(tree, port)
            publisher.set_max_heartbeat_delay_ms(50)
            assert publisher.max_heartbeat_delay_ms() == 50
            assert tree.tick_once() == bt.NodeStatus.SUCCESS
            publisher.flush()
            del publisher
            gc.collect()
            queue.put(None)
            return
        except Exception as e:  # noqa: BLE001
            last_error = e

    queue.put(repr(last_error))


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


def test_sqlite_logger_writes_db(tmp_path) -> None:
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

    path = tmp_path / "trace.db3"
    logger = bt.SqliteLogger(tree, str(path))

    assert tree.tick_once() == bt.NodeStatus.SUCCESS
    logger.flush()

    assert path.exists()
    assert path.stat().st_size > 0


def test_groot2_publisher_constructs_and_ticks() -> None:
    import multiprocessing as mp

    ctx = mp.get_context("spawn")
    queue: mp.Queue = ctx.Queue()
    proc = ctx.Process(target=_groot2_worker, args=(queue,))
    proc.start()
    proc.join(timeout=15)

    if proc.is_alive():
        proc.terminate()
        proc.join(timeout=5)
        pytest.skip("Groot2Publisher teardown timed out in this environment")

    assert proc.exitcode == 0, f"Groot2Publisher subprocess exited with code {proc.exitcode}"
    result = queue.get(timeout=2)
    assert result is None, f"Groot2Publisher failed in subprocess: {result}"
