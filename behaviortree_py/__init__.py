import re
import warnings

from ._version import __version__
from ._core import (
    Blackboard,
    BehaviorTreeFactory,
    NodeStatus,
    Tree,
    btcpp_version_number,
    btcpp_version_string,
)
from .nodes import StatefulActionNode, SyncActionNode


_VERSION_RE = re.compile(r"(\d+)\.(\d+)\.(\d+)")


def _parse_semver(version: str) -> tuple[int, int, int] | None:
    match = _VERSION_RE.search(version)
    if not match:
        return None
    return (int(match.group(1)), int(match.group(2)), int(match.group(3)))


def _check_btcpp_compatibility() -> None:
    behaviortree_py_v = _parse_semver(__version__)
    btcpp_v = _parse_semver(btcpp_version_string())
    if not behaviortree_py_v or not btcpp_v:
        warnings.warn(
            "behaviortree_py compatibility check skipped (unparseable versions): "
            f"behaviortree_py={__version__!r}, btcpp={btcpp_version_string()!r}",
            RuntimeWarning,
            stacklevel=2,
        )
        return

    btpy_major, btpy_minor, btpy_patch = behaviortree_py_v
    btcpp_major, btcpp_minor, btcpp_patch = btcpp_v

    if (btpy_major, btpy_minor) != (btcpp_major, btcpp_minor):
        raise RuntimeError(
            "BehaviorTree.PY / BehaviorTree.CPP version mismatch: "
            f"behaviortree_py={__version__}, btcpp={btcpp_version_string()} "
            "(expected same MAJOR.MINOR)."
        )

    if btpy_patch != btcpp_patch:
        warnings.warn(
            "BehaviorTree.PY / BehaviorTree.CPP patch mismatch: "
            f"behaviortree_py={__version__}, btcpp={btcpp_version_string()} "
            "(allowed, but not guaranteed).",
            RuntimeWarning,
            stacklevel=2,
        )


_check_btcpp_compatibility()

__all__ = [
    "__version__",
    "Blackboard",
    "BehaviorTreeFactory",
    "NodeStatus",
    "StatefulActionNode",
    "SyncActionNode",
    "Tree",
    "btcpp_version_number",
    "btcpp_version_string",
]
