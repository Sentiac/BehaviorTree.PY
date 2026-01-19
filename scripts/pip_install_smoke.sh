#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WS_INSTALL_DIR="$(cd "${ROOT_DIR}/../../install" && pwd)"
ROS_DISTRO="${ROS_DISTRO:-jazzy}"

if [[ -f "/opt/ros/${ROS_DISTRO}/setup.bash" ]]; then
  # Needed when BT.CPP was built/installed as a ROS/ament package (its CMake config may depend on ament packages).
  set +u
  source "/opt/ros/${ROS_DISTRO}/setup.bash"
  set -u
fi

ROS_PYTHONPATH="${PYTHONPATH:-}"
ROS_PREFIX="/opt/ros/${ROS_DISTRO}"

if [[ ! -d "${WS_INSTALL_DIR}" ]]; then
  echo "Expected workspace install dir at: ${WS_INSTALL_DIR}" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
cleanup() { rm -rf "${tmpdir}"; }
trap cleanup EXIT

python3 -m venv --system-site-packages "${tmpdir}/venv"
source "${tmpdir}/venv/bin/activate"

python -m pip install -U pip

if [[ -n "${ROS_PYTHONPATH}" ]]; then
  export PYTHONPATH="${ROS_PYTHONPATH}"
fi

export CMAKE_PREFIX_PATH="${WS_INSTALL_DIR}${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}"
if [[ -d "${ROS_PREFIX}" ]]; then
  export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}:${ROS_PREFIX}"
fi

# Disable build isolation so that the ROS-provided Python packages (e.g. ament_package) visible via PYTHONPATH
# remain importable when ament_cmake runs Python helper scripts during CMake configure.
python -m pip install -U scikit-build-core pybind11 cmake ninja
python -m pip install -v --no-build-isolation "${ROOT_DIR}"

cd "${tmpdir}"
python - <<'PY'
import importlib.util
import pathlib
import site
import sys
import traceback

spec = importlib.util.find_spec("behaviortree_py")
print("behaviortree_py spec:", spec)

try:
    import behaviortree_py as bt
except Exception:
    print("sys.path:", sys.path)
    for base in site.getsitepackages():
        pkg_dir = pathlib.Path(base) / "behaviortree_py"
        if pkg_dir.exists():
            print("package dir:", pkg_dir)
            print("package contents:", sorted([p.name for p in pkg_dir.iterdir()]))
            print("_core files:", sorted([p.name for p in pkg_dir.glob("_core*.so")]))
    traceback.print_exc()
    raise

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
assert tree.tick_once() == bt.NodeStatus.SUCCESS
print("pip smoke: OK (btcpp:", bt.btcpp_version_string(), ")")
PY
