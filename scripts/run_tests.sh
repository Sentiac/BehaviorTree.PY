#!/usr/bin/env bash
set -euo pipefail

# Run the BehaviorTree.PY pytest suite with ROS launch_testing plugins disabled.
# In ROS environments, these plugins are often auto-loaded and may crash or
# raise internal errors when running non-launch unit tests.

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/.." && pwd)"
ws_dir="$(cd "${repo_dir}/../.." && pwd)"
orig_cwd="$(pwd)"
python_bin="$(command -v python3)"

ros_distro="${ROS_DISTRO:-jazzy}"
if [[ -f "/opt/ros/${ros_distro}/setup.bash" ]]; then
  set +u
  # shellcheck disable=SC1090
  source "/opt/ros/${ros_distro}/setup.bash"
  set -u
fi

if [[ -f "${ws_dir}/install/setup.bash" ]]; then
  set +u
  # shellcheck disable=SC1090
  source "${ws_dir}/install/setup.bash"
  set -u
fi

args=("$@")
if [[ ${#args[@]} -eq 0 ]]; then
  args=("${repo_dir}/tests")
else
  resolved_args=()
  for arg in "${args[@]}"; do
    if [[ "${arg}" == -* ]]; then
      resolved_args+=("${arg}")
    elif [[ "${arg}" = /* ]]; then
      resolved_args+=("${arg}")
    elif [[ -e "${orig_cwd}/${arg}" ]]; then
      resolved_args+=("${orig_cwd}/${arg}")
    elif [[ -e "${repo_dir}/${arg}" ]]; then
      resolved_args+=("${repo_dir}/${arg}")
    else
      resolved_args+=("${arg}")
    fi
  done
  args=("${resolved_args[@]}")
fi

export PYTEST_DISABLE_PLUGIN_AUTOLOAD="${PYTEST_DISABLE_PLUGIN_AUTOLOAD:-1}"

cd "${ws_dir}"

exec "${python_bin}" -m pytest -q \
  --rootdir "${ws_dir}" \
  -p no:launch_testing \
  -p no:launch_ros \
  -p no:launch_pytest \
  "${args[@]}"
