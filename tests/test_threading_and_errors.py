import threading

import pytest

import behaviortree_py as bt


class Boom(bt.SyncActionNode):
    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> bt.NodeStatus:
        raise ValueError("boom")


def test_python_exception_includes_context_note() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_sync_action(Boom)

    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Boom/>
  </BehaviorTree>
</root>
""".strip()
    )

    with pytest.raises(ValueError) as excinfo:
        tree.tick_once()

    notes = getattr(excinfo.value, "__notes__", [])
    assert any("exception in tick()" in n and "registration_id='Boom'" in n for n in notes)


def test_tree_tick_from_other_thread_fails_fast() -> None:
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

    raised: Exception | None = None

    def worker() -> None:
        nonlocal raised
        try:
            tree.tick_once()
        except Exception as exc:  # pragma: no cover
            raised = exc

    thread = threading.Thread(target=worker)
    thread.start()
    thread.join()

    assert isinstance(raised, RuntimeError)
    assert "different thread" in str(raised)


def test_tick_while_running_releases_gil() -> None:
    factory = bt.BehaviorTreeFactory()
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sleep msec="200"/>
  </BehaviorTree>
</root>
""".strip()
    )

    go = threading.Event()
    stop = threading.Event()
    counter = {"value": 0}

    def worker() -> None:
        go.wait()
        while not stop.is_set():
            counter["value"] += 1

    thread = threading.Thread(target=worker)
    thread.start()
    go.set()

    status = tree.tick_while_running(sleep_ms=1)
    stop.set()
    thread.join()

    assert status == bt.NodeStatus.SUCCESS
    assert counter["value"] > 1_000
