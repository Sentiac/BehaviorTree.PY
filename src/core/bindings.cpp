#include <chrono>
#include <string>

#include <pybind11/gil.h>
#include <pybind11/pybind11.h>

#include <behaviortree_cpp/basic_types.h>
#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/bt_factory.h>

namespace py = pybind11;

PYBIND11_MODULE(_core, m)
{
  m.doc() = "BehaviorTree.PY low-level bindings";

  m.def("btcpp_version_string", []() { return std::string(BT::LibraryVersionString()); });
  m.def("btcpp_version_number", []() { return BT::LibraryVersionNumber(); });

  py::enum_<BT::NodeStatus>(m, "NodeStatus")
      .value("IDLE", BT::NodeStatus::IDLE)
      .value("RUNNING", BT::NodeStatus::RUNNING)
      .value("SUCCESS", BT::NodeStatus::SUCCESS)
      .value("FAILURE", BT::NodeStatus::FAILURE)
      .value("SKIPPED", BT::NodeStatus::SKIPPED)
      .export_values();

  py::class_<BT::Tree>(m, "Tree")
      .def(
          "tick_once",
          [](BT::Tree& tree) {
            py::gil_scoped_release release;
            return tree.tickOnce();
          },
          "Tick the tree once.")
      .def(
          "tick_exactly_once",
          [](BT::Tree& tree) {
            py::gil_scoped_release release;
            return tree.tickExactlyOnce();
          },
          "Tick the tree once, expecting completion in a single tick.")
      .def(
          "tick_while_running",
          [](BT::Tree& tree, int sleep_ms) {
            py::gil_scoped_release release;
            return tree.tickWhileRunning(std::chrono::milliseconds(sleep_ms));
          },
          py::arg("sleep_ms") = 10,
          "Tick until completion, sleeping between ticks.")
      .def(
          "halt_tree",
          [](BT::Tree& tree) {
            py::gil_scoped_release release;
            tree.haltTree();
          },
          "Halt execution of the tree.");

  py::class_<BT::BehaviorTreeFactory>(m, "BehaviorTreeFactory")
      .def(py::init<>())
      .def(
          "register_from_plugin",
          &BT::BehaviorTreeFactory::registerFromPlugin,
          py::arg("path"),
          "Register nodes from a shared library plugin.")
      .def(
          "create_tree_from_text",
          [](BT::BehaviorTreeFactory& factory, const std::string& text) {
            return factory.createTreeFromText(text);
          },
          py::arg("text"),
          py::return_value_policy::move,
          "Create a Tree from an XML string.")
      .def(
          "create_tree_from_file",
          [](BT::BehaviorTreeFactory& factory, const std::string& path) {
            return factory.createTreeFromFile(path);
          },
          py::arg("path"),
          py::return_value_policy::move,
          "Create a Tree from an XML file path.");

#ifdef BEHAVIORTREE_PY_VERSION
  m.attr("__behaviortree_py_version__") = py::str(BEHAVIORTREE_PY_VERSION);
#endif
}
