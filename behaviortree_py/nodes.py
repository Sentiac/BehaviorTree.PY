from __future__ import annotations

from ._core import NodeStatus


class SyncActionNode:
    def __init__(self, name: str, *args: object, **kwargs: object) -> None:
        self.name = name

    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> NodeStatus:
        raise NotImplementedError


class StatefulActionNode:
    def __init__(self, name: str, *args: object, **kwargs: object) -> None:
        self.name = name

    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def on_start(self) -> NodeStatus:
        raise NotImplementedError

    def on_running(self) -> NodeStatus:
        raise NotImplementedError

    def on_halted(self) -> None:
        raise NotImplementedError

