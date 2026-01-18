#include <string>

#include <pybind11/pybind11.h>

#include <behaviortree_cpp/behavior_tree.h>

namespace py = pybind11;

PYBIND11_MODULE(_core, m)
{
  m.doc() = "BehaviorTree.PY low-level bindings (scaffolding stage)";

  m.def("btcpp_version_string", []() { return std::string(BT::LibraryVersionString()); });
  m.def("btcpp_version_number", []() { return BT::LibraryVersionNumber(); });

#ifdef BEHAVIORTREE_PY_VERSION
  m.attr("__behaviortree_py_version__") = py::str(BEHAVIORTREE_PY_VERSION);
#endif
}
