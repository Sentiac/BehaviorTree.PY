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


class ConditionNode:
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


class DecoratorNode:
    def __init__(self, name: str, *args: object, **kwargs: object) -> None:
        self.name = name

    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> NodeStatus:
        raise NotImplementedError

    def halt(self) -> None:
        return

    def tick_child(self) -> NodeStatus:
        return self._bt.tick_child()  # set internally by the native wrapper

    def halt_child(self) -> None:
        self._bt.halt_child()  # set internally by the native wrapper

    def reset_child(self) -> None:
        self._bt.reset_child()  # set internally by the native wrapper

    def get_input(self, key: str):
        return self._bt.get_input(key)  # set internally by the native wrapper

    def set_output(self, key: str, value) -> None:
        self._bt.set_output(key, value)  # set internally by the native wrapper


class ControlNode:
    def __init__(self, name: str, *args: object, **kwargs: object) -> None:
        self.name = name

    @classmethod
    def provided_ports(cls) -> dict[str, list[str]]:
        return {"inputs": [], "outputs": []}

    def tick(self) -> NodeStatus:
        raise NotImplementedError

    def halt(self) -> None:
        return

    @property
    def children_count(self) -> int:
        return self._bt.children_count  # set internally by the native wrapper

    def tick_child(self, index: int) -> NodeStatus:
        return self._bt.tick_child(index)  # set internally by the native wrapper

    def halt_child(self, index: int) -> None:
        self._bt.halt_child(index)  # set internally by the native wrapper

    def halt_children(self) -> None:
        self._bt.halt_children()  # set internally by the native wrapper

    def reset_children(self) -> None:
        self._bt.reset_children()  # set internally by the native wrapper

    def get_input(self, key: str):
        return self._bt.get_input(key)  # set internally by the native wrapper

    def set_output(self, key: str, value) -> None:
        self._bt.set_output(key, value)  # set internally by the native wrapper
