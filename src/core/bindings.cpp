#include <chrono>
#include <string>
#include <thread>

#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include <behaviortree_cpp/action_node.h>
#include <behaviortree_cpp/basic_types.h>
#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/bt_factory.h>

namespace py = pybind11;

namespace
{

BT::PortsList extract_ports_list_or_throw(const py::type& type)
{
  if (!py::hasattr(type, "provided_ports"))
  {
    throw py::type_error(
        "Python node types must define @classmethod provided_ports() -> dict");
  }

  py::object port_spec_obj = type.attr("provided_ports")();
  if (port_spec_obj.is_none())
  {
    return {};
  }
  if (!py::isinstance<py::dict>(port_spec_obj))
  {
    throw py::type_error("provided_ports() must return a dict");
  }

  const py::dict port_spec = port_spec_obj.cast<py::dict>();

  auto get_str_list = [&](const char* key) -> py::list {
    if (!port_spec.contains(key))
    {
      return py::list();
    }
    py::object obj = port_spec[key];
    if (obj.is_none())
    {
      return py::list();
    }
    if (!py::isinstance<py::list>(obj) && !py::isinstance<py::tuple>(obj))
    {
      throw py::type_error(std::string("provided_ports()['") + key + "'] must be a list");
    }
    return py::list(obj);
  };

  BT::PortsList ports;

  for (py::handle name_obj : get_str_list("inputs"))
  {
    const auto name = py::cast<std::string>(name_obj);
    ports.insert(BT::InputPort<BT::AnyTypeAllowed>(name));
  }

  for (py::handle name_obj : get_str_list("outputs"))
  {
    const auto name = py::cast<std::string>(name_obj);
    ports.insert(BT::OutputPort<BT::AnyTypeAllowed>(name));
  }

  return ports;
}

class PySyncActionNode final : public BT::SyncActionNode
{
public:
  PySyncActionNode(const std::string& name, const BT::NodeConfig& config, py::type type,
                   py::tuple ctor_args, py::dict ctor_kwargs) :
    BT::SyncActionNode(name, config),
    created_on_thread_id_(std::this_thread::get_id())
  {
    py::gil_scoped_acquire acquire;
    py_instance_ = type(py::str(name), *ctor_args, **ctor_kwargs);
  }

  ~PySyncActionNode() override
  {
    if (!Py_IsInitialized())
    {
      return;
    }
    py::gil_scoped_acquire acquire;
    py_instance_ = py::none();
  }

  BT::NodeStatus tick() override
  {
    enforce_thread_or_throw("tick");
    py::gil_scoped_acquire acquire;
    py::object result = py_instance_.attr("tick")();
    return result.cast<BT::NodeStatus>();
  }

private:
  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }

    throw std::runtime_error(std::string("Python node method called from a different thread: ")
                                 + method);
  }

  std::thread::id created_on_thread_id_;
  py::object py_instance_;
};

class PyStatefulActionNode final : public BT::StatefulActionNode
{
public:
  PyStatefulActionNode(const std::string& name, const BT::NodeConfig& config, py::type type,
                       py::tuple ctor_args, py::dict ctor_kwargs) :
    BT::StatefulActionNode(name, config),
    created_on_thread_id_(std::this_thread::get_id())
  {
    py::gil_scoped_acquire acquire;
    py_instance_ = type(py::str(name), *ctor_args, **ctor_kwargs);
  }

  ~PyStatefulActionNode() override
  {
    if (!Py_IsInitialized())
    {
      return;
    }
    py::gil_scoped_acquire acquire;
    py_instance_ = py::none();
  }

  BT::NodeStatus onStart() override
  {
    enforce_thread_or_throw("on_start");
    py::gil_scoped_acquire acquire;
    py::object result = py_instance_.attr("on_start")();
    return result.cast<BT::NodeStatus>();
  }

  BT::NodeStatus onRunning() override
  {
    enforce_thread_or_throw("on_running");
    py::gil_scoped_acquire acquire;
    py::object result = py_instance_.attr("on_running")();
    return result.cast<BT::NodeStatus>();
  }

  void onHalted() override
  {
    enforce_thread_or_throw("on_halted");
    py::gil_scoped_acquire acquire;
    py_instance_.attr("on_halted")();
  }

private:
  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }

    throw std::runtime_error(std::string("Python node method called from a different thread: ")
                                 + method);
  }

  std::thread::id created_on_thread_id_;
  py::object py_instance_;
};

BT::NodeBuilder make_sync_action_builder(const py::type& type, const py::args& args,
                                        const py::kwargs& kwargs)
{
  py::tuple ctor_args(args);
  py::dict ctor_kwargs(kwargs);

  return [type, ctor_args, ctor_kwargs](const std::string& name,
                                       const BT::NodeConfig& config) -> std::unique_ptr<BT::TreeNode> {
    return std::make_unique<PySyncActionNode>(name, config, type, ctor_args, ctor_kwargs);
  };
}

BT::NodeBuilder make_stateful_action_builder(const py::type& type, const py::args& args,
                                             const py::kwargs& kwargs)
{
  py::tuple ctor_args(args);
  py::dict ctor_kwargs(kwargs);

  return [type, ctor_args, ctor_kwargs](const std::string& name,
                                       const BT::NodeConfig& config) -> std::unique_ptr<BT::TreeNode> {
    return std::make_unique<PyStatefulActionNode>(name, config, type, ctor_args, ctor_kwargs);
  };
}

}   // namespace

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
          "register_sync_action",
          [](BT::BehaviorTreeFactory& factory, const py::type& type, const py::args& args,
             const py::kwargs& kwargs) {
            const std::string name = type.attr("__name__").cast<std::string>();

            BT::TreeNodeManifest manifest;
            manifest.type = BT::NodeType::ACTION;
            manifest.registration_ID = name;
            manifest.ports = extract_ports_list_or_throw(type);
            manifest.metadata = {};

            factory.registerBuilder(manifest, make_sync_action_builder(type, args, kwargs));
          },
          py::arg("node_type"),
          "Register a Python SyncActionNode type.\n\n"
          "The node class must define @classmethod provided_ports() -> dict with keys:\n"
          "  - 'inputs': list[str]\n"
          "  - 'outputs': list[str]\n"
          "The constructor is called as: node_type(name, *args, **kwargs).")
      .def(
          "register_stateful_action",
          [](BT::BehaviorTreeFactory& factory, const py::type& type, const py::args& args,
             const py::kwargs& kwargs) {
            const std::string name = type.attr("__name__").cast<std::string>();

            BT::TreeNodeManifest manifest;
            manifest.type = BT::NodeType::ACTION;
            manifest.registration_ID = name;
            manifest.ports = extract_ports_list_or_throw(type);
            manifest.metadata = {};

            factory.registerBuilder(manifest, make_stateful_action_builder(type, args, kwargs));
          },
          py::arg("node_type"),
          "Register a Python StatefulActionNode type.\n\n"
          "The node class must define @classmethod provided_ports() -> dict with keys:\n"
          "  - 'inputs': list[str]\n"
          "  - 'outputs': list[str]\n"
          "The constructor is called as: node_type(name, *args, **kwargs).")
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
