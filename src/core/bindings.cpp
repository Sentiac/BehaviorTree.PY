#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include <behaviortree_cpp/action_node.h>
#include <behaviortree_cpp/basic_types.h>
#include <behaviortree_cpp/blackboard.h>
#include <behaviortree_cpp/behavior_tree.h>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/json_export.h>
#include <behaviortree_cpp/utils/demangle_util.h>

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

int64_t py_int_to_int64(const py::handle& obj)
{
  const py::int_ i = py::reinterpret_borrow<py::int_>(obj);
  int overflow = 0;
  const long long value = PyLong_AsLongLongAndOverflow(i.ptr(), &overflow);
  if (value == -1 && PyErr_Occurred())
  {
    throw py::error_already_set();
  }
  if (overflow != 0)
  {
    throw py::value_error("Python int is out of range for int64");
  }
  return static_cast<int64_t>(value);
}

nlohmann::json py_to_json_strict(const py::handle& obj);

nlohmann::json py_sequence_to_json_array_strict(const py::handle& seq_handle)
{
  const py::sequence seq = py::reinterpret_borrow<py::sequence>(seq_handle);
  nlohmann::json array = nlohmann::json::array();

  bool first = true;
  nlohmann::json::value_t element_type = nlohmann::json::value_t::null;

  for (py::handle item : seq)
  {
    nlohmann::json child = py_to_json_strict(item);
    if (first)
    {
      element_type = child.type();
      first = false;
    }
    else if (child.type() != element_type)
    {
      throw py::type_error("JSON arrays must be homogeneous (mixed element types)");
    }
    array.push_back(std::move(child));
  }

  return array;
}

nlohmann::json py_to_json_strict(const py::handle& obj)
{
  if (obj.is_none())
  {
    return nullptr;
  }
  if (py::isinstance<py::bool_>(obj))
  {
    return py::cast<bool>(obj);
  }
  if (py::isinstance<py::int_>(obj) && !py::isinstance<py::bool_>(obj))
  {
    return py_int_to_int64(obj);
  }
  if (py::isinstance<py::float_>(obj))
  {
    return py::cast<double>(obj);
  }
  if (py::isinstance<py::str>(obj))
  {
    return py::cast<std::string>(obj);
  }
  if (py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj))
  {
    return py_sequence_to_json_array_strict(obj);
  }
  if (py::isinstance<py::dict>(obj))
  {
    const py::dict dict = py::reinterpret_borrow<py::dict>(obj);
    nlohmann::json json_obj = nlohmann::json::object();
    for (auto item : dict)
    {
      const py::handle key_obj = item.first;
      if (!py::isinstance<py::str>(key_obj))
      {
        throw py::type_error("JSON object keys must be strings");
      }
      const auto key = py::cast<std::string>(key_obj);
      json_obj[key] = py_to_json_strict(item.second);
    }
    return json_obj;
  }

  throw py::type_error("Value is not JSON-serializable under BehaviorTree.PY rules");
}

py::object json_to_py(const nlohmann::json& json)
{
  switch (json.type())
  {
    case nlohmann::json::value_t::null:
      return py::none();
    case nlohmann::json::value_t::boolean:
      return py::bool_(json.get<bool>());
    case nlohmann::json::value_t::number_integer:
      return py::int_(json.get<int64_t>());
    case nlohmann::json::value_t::number_unsigned:
      return py::int_(json.get<uint64_t>());
    case nlohmann::json::value_t::number_float:
      return py::float_(json.get<double>());
    case nlohmann::json::value_t::string:
      return py::str(json.get<std::string>());
    case nlohmann::json::value_t::array:
    {
      py::list out;
      for (const auto& item : json)
      {
        out.append(json_to_py(item));
      }
      return out;
    }
    case nlohmann::json::value_t::object:
    {
      py::dict out;
      for (const auto& kv : json.items())
      {
        out[py::str(kv.key())] = json_to_py(kv.value());
      }
      return out;
    }
    default:
      throw py::type_error("Unsupported JSON value type for conversion to Python");
  }
}

py::object any_to_py(const BT::Any& any)
{
  if (any.empty())
  {
    return py::none();
  }

  if (any.type() == typeid(nlohmann::json))
  {
    return json_to_py(any.cast<nlohmann::json>());
  }

  if (any.type() == typeid(bool))
  {
    return py::bool_(any.cast<bool>());
  }
  if (any.type() == typeid(int64_t))
  {
    return py::int_(any.cast<int64_t>());
  }
  if (any.type() == typeid(double))
  {
    return py::float_(any.cast<double>());
  }
  if (any.type() == typeid(std::string))
  {
    return py::str(any.cast<std::string>());
  }
  if (any.type() == typeid(std::vector<bool>))
  {
    py::list out;
    for (const bool v : any.cast<std::vector<bool>>())
    {
      out.append(py::bool_(v));
    }
    return out;
  }
  if (any.type() == typeid(std::vector<int64_t>))
  {
    py::list out;
    for (const int64_t v : any.cast<std::vector<int64_t>>())
    {
      out.append(py::int_(v));
    }
    return out;
  }
  if (any.type() == typeid(std::vector<double>))
  {
    py::list out;
    for (const double v : any.cast<std::vector<double>>())
    {
      out.append(py::float_(v));
    }
    return out;
  }
  if (any.type() == typeid(std::vector<std::string>))
  {
    py::list out;
    for (const auto& v : any.cast<std::vector<std::string>>())
    {
      out.append(py::str(v));
    }
    return out;
  }

  nlohmann::json json;
  if (BT::JsonExporter::get().toJson(any, json))
  {
    return json_to_py(json);
  }

  throw py::type_error("Unsupported BT blackboard value type: " + BT::demangle(any.type()));
}

BT::Result set_output_from_py(BT::TreeNode& node, const std::string& key,
                              const py::handle& value)
{
  if (value.is_none())
  {
    return node.setOutput(key, nlohmann::json(nullptr));
  }
  if (py::isinstance<py::bool_>(value))
  {
    return node.setOutput(key, py::cast<bool>(value));
  }
  if (py::isinstance<py::int_>(value) && !py::isinstance<py::bool_>(value))
  {
    return node.setOutput(key, py_int_to_int64(value));
  }
  if (py::isinstance<py::float_>(value))
  {
    return node.setOutput(key, py::cast<double>(value));
  }
  if (py::isinstance<py::str>(value))
  {
    return node.setOutput(key, py::cast<std::string>(value));
  }

  if (py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value))
  {
    const py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
    const auto n = py::len(seq);
    if (n == 0)
    {
      return node.setOutput(key, nlohmann::json::array());
    }

    const py::handle first = seq[0];

    if (py::isinstance<py::bool_>(first))
    {
      std::vector<bool> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::bool_>(item))
        {
          throw py::type_error("Typed bool list must contain only bool elements");
        }
        out.push_back(py::cast<bool>(item));
      }
      return node.setOutput(key, out);
    }

    if (py::isinstance<py::int_>(first) && !py::isinstance<py::bool_>(first))
    {
      std::vector<int64_t> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::int_>(item) || py::isinstance<py::bool_>(item))
        {
          throw py::type_error("Typed int list must contain only int elements (no bool/float)");
        }
        out.push_back(py_int_to_int64(item));
      }
      return node.setOutput(key, out);
    }

    if (py::isinstance<py::float_>(first))
    {
      std::vector<double> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::float_>(item))
        {
          throw py::type_error("Typed float list must contain only float elements");
        }
        out.push_back(py::cast<double>(item));
      }
      return node.setOutput(key, out);
    }

    if (py::isinstance<py::str>(first))
    {
      std::vector<std::string> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::str>(item))
        {
          throw py::type_error("Typed string list must contain only str elements");
        }
        out.push_back(py::cast<std::string>(item));
      }
      return node.setOutput(key, out);
    }

    // JSON lane (nested lists / dicts / etc)
    return node.setOutput(key, py_to_json_strict(value));
  }

  if (py::isinstance<py::dict>(value))
  {
    return node.setOutput(key, py_to_json_strict(value));
  }

  throw py::type_error("Unsupported value type for set_output()");
}

void blackboard_set_from_py(BT::Blackboard& bb, const std::string& key, const py::handle& value)
{
  if (value.is_none())
  {
    bb.set(key, nlohmann::json(nullptr));
    return;
  }
  if (py::isinstance<py::bool_>(value))
  {
    bb.set(key, py::cast<bool>(value));
    return;
  }
  if (py::isinstance<py::int_>(value) && !py::isinstance<py::bool_>(value))
  {
    bb.set(key, py_int_to_int64(value));
    return;
  }
  if (py::isinstance<py::float_>(value))
  {
    bb.set(key, py::cast<double>(value));
    return;
  }
  if (py::isinstance<py::str>(value))
  {
    bb.set(key, py::cast<std::string>(value));
    return;
  }
  if (py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value))
  {
    const py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
    const auto n = py::len(seq);
    if (n == 0)
    {
      bb.set(key, nlohmann::json::array());
      return;
    }

    const py::handle first = seq[0];
    if (py::isinstance<py::bool_>(first))
    {
      std::vector<bool> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::bool_>(item))
        {
          throw py::type_error("Typed bool list must contain only bool elements");
        }
        out.push_back(py::cast<bool>(item));
      }
      bb.set(key, out);
      return;
    }
    if (py::isinstance<py::int_>(first) && !py::isinstance<py::bool_>(first))
    {
      std::vector<int64_t> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::int_>(item) || py::isinstance<py::bool_>(item))
        {
          throw py::type_error("Typed int list must contain only int elements (no bool/float)");
        }
        out.push_back(py_int_to_int64(item));
      }
      bb.set(key, out);
      return;
    }
    if (py::isinstance<py::float_>(first))
    {
      std::vector<double> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::float_>(item))
        {
          throw py::type_error("Typed float list must contain only float elements");
        }
        out.push_back(py::cast<double>(item));
      }
      bb.set(key, out);
      return;
    }
    if (py::isinstance<py::str>(first))
    {
      std::vector<std::string> out;
      out.reserve(n);
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::str>(item))
        {
          throw py::type_error("Typed string list must contain only str elements");
        }
        out.push_back(py::cast<std::string>(item));
      }
      bb.set(key, out);
      return;
    }

    bb.set(key, py_to_json_strict(value));
    return;
  }
  if (py::isinstance<py::dict>(value))
  {
    bb.set(key, py_to_json_strict(value));
    return;
  }

  throw py::type_error("Unsupported value type for Blackboard.set()");
}

class NodeHandle
{
public:
  explicit NodeHandle(BT::TreeNode* node) : node_(node)
  {}

  [[nodiscard]] py::object get_input(const std::string& key) const
  {
    BT::Any out;
    auto res = node_->getInput(key, out);
    if (!res)
    {
      throw std::runtime_error(res.error());
    }
    return any_to_py(out);
  }

  void set_output(const std::string& key, const py::object& value)
  {
    auto res = set_output_from_py(*node_, key, value);
    if (!res)
    {
      throw std::runtime_error(res.error());
    }
  }

  [[nodiscard]] std::string name() const
  {
    return node_->name();
  }

private:
  BT::TreeNode* node_;
};

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
    py_instance_.attr("_bt") = py::cast(NodeHandle(this));
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
    py_instance_.attr("_bt") = py::cast(NodeHandle(this));
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

  py::class_<NodeHandle>(m, "_NodeHandle")
      .def("get_input", &NodeHandle::get_input, py::arg("key"))
      .def("set_output", &NodeHandle::set_output, py::arg("key"), py::arg("value"))
      .def_property_readonly("name", &NodeHandle::name);

  py::class_<BT::Blackboard, BT::Blackboard::Ptr>(m, "Blackboard")
      .def(
          "keys",
          [](const BT::Blackboard& bb) {
            py::list out;
            for (const auto& key : bb.getKeys())
            {
              out.append(py::str(std::string(key)));
            }
            return out;
          },
          "List keys in this blackboard.")
      .def(
          "get",
          [](const BT::Blackboard& bb, const std::string& key) {
            if (auto any_ref = bb.getAnyLocked(key))
            {
              if (any_ref.get()->empty())
              {
                throw std::runtime_error(
                    "Blackboard entry exists but has not been initialized: " + key);
              }
              return any_to_py(*any_ref.get());
            }
            throw py::key_error("Missing blackboard key: " + key);
          },
          py::arg("key"),
          "Get a value from the blackboard (strict; missing keys raise).")
      .def(
          "set",
          [](BT::Blackboard& bb, const std::string& key, const py::object& value) {
            blackboard_set_from_py(bb, key, value);
          },
          py::arg("key"),
          py::arg("value"),
          "Set a value in the blackboard using BehaviorTree.PY conversion rules.")
      .def("unset", &BT::Blackboard::unset, py::arg("key"), "Unset a blackboard key.");

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
      .def("root_blackboard", [](BT::Tree& tree) { return tree.rootBlackboard(); },
           "Return the root blackboard.")
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
