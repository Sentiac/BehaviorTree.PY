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

    def get_input(self, key: str):
        return self._bt.get_input(key)  # set internally by the native wrapper

    def set_output(self, key: str, value) -> None:
        self._bt.set_output(key, value)  # set internally by the native wrapper


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

    def get_input(self, key: str):
        return self._bt.get_input(key)  # set internally by the native wrapper

    def set_output(self, key: str, value) -> None:
        self._bt.set_output(key, value)  # set internally by the native wrapper
