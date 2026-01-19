#include <chrono>
#include <cctype>
#include <cstdint>
#include <algorithm>
#include <filesystem>
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
#include <behaviortree_cpp/condition_node.h>
#include <behaviortree_cpp/control_node.h>
#include <behaviortree_cpp/decorator_node.h>
#include <behaviortree_cpp/json_export.h>
#include <behaviortree_cpp/loggers/bt_cout_logger.h>
#include <behaviortree_cpp/loggers/bt_file_logger_v2.h>
#include <behaviortree_cpp/loggers/bt_minitrace_logger.h>
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

  auto get_list = [&](const char* key) -> py::list {
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

  auto normalize_type = [](std::string type_str) -> std::string {
    std::string out;
    out.reserve(type_str.size());
    for (unsigned char c : type_str)
    {
      if (std::isspace(c))
      {
        continue;
      }
      out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
  };

  auto insert_port = [&](BT::PortDirection direction, const std::string& name,
                         const std::string& type_str) {
    const std::string t = normalize_type(type_str);
    if (t.empty() || t == "any")
    {
      ports.insert(BT::CreatePort<BT::AnyTypeAllowed>(direction, name));
      return;
    }
    if (t == "bool")
    {
      ports.insert(BT::CreatePort<bool>(direction, name));
      return;
    }
    if (t == "int" || t == "int64")
    {
      ports.insert(BT::CreatePort<int64_t>(direction, name));
      return;
    }
    if (t == "float" || t == "double")
    {
      ports.insert(BT::CreatePort<double>(direction, name));
      return;
    }
    if (t == "str" || t == "string")
    {
      ports.insert(BT::CreatePort<std::string>(direction, name));
      return;
    }
    if (t == "list[bool]")
    {
      ports.insert(BT::CreatePort<std::vector<bool>>(direction, name));
      return;
    }
    if (t == "list[int]" || t == "list[int64]")
    {
      ports.insert(BT::CreatePort<std::vector<int64_t>>(direction, name));
      return;
    }
    if (t == "list[float]" || t == "list[double]")
    {
      ports.insert(BT::CreatePort<std::vector<double>>(direction, name));
      return;
    }
    if (t == "list[str]" || t == "list[string]")
    {
      ports.insert(BT::CreatePort<std::vector<std::string>>(direction, name));
      return;
    }
    if (t == "json")
    {
      ports.insert(BT::CreatePort<nlohmann::json>(direction, name));
      return;
    }

    throw py::value_error("Unsupported port type: '" + type_str
                          + "' (supported: any, bool, int, float, str, json, list[...])");
  };

  auto add_ports = [&](BT::PortDirection direction, const char* key) {
    for (py::handle item : get_list(key))
    {
      if (py::isinstance<py::str>(item))
      {
        insert_port(direction, py::cast<std::string>(item), "");
        continue;
      }
      if (py::isinstance<py::dict>(item))
      {
        const py::dict spec = py::reinterpret_borrow<py::dict>(item);
        if (!spec.contains("name"))
        {
          throw py::type_error(std::string("Port spec dict in provided_ports()['") + key
                               + "'] must contain key 'name'");
        }
        const py::handle name_obj = spec["name"];
        if (!py::isinstance<py::str>(name_obj))
        {
          throw py::type_error(std::string("Port spec 'name' in provided_ports()['") + key
                               + "'] must be a string");
        }
        const std::string name = py::cast<std::string>(name_obj);

        std::string type_str;
        if (spec.contains("type") && !spec["type"].is_none())
        {
          const py::handle type_obj = spec["type"];
          if (!py::isinstance<py::str>(type_obj))
          {
            throw py::type_error(std::string("Port spec 'type' in provided_ports()['") + key
                                 + "'] must be a string");
          }
          type_str = py::cast<std::string>(type_obj);
        }

        insert_port(direction, name, type_str);
        continue;
      }

      throw py::type_error(std::string("Port specs in provided_ports()['") + key
                           + "'] must be strings or dicts");
    }
  };

  add_ports(BT::PortDirection::INPUT, "inputs");
  add_ports(BT::PortDirection::OUTPUT, "outputs");
  add_ports(BT::PortDirection::INOUT, "inouts");

  return ports;
}

std::string py_type_name(const py::handle& obj)
{
  try
  {
    return py::cast<std::string>(py::type::of(obj).attr("__name__"));
  }
  catch (...)
  {
    return "<unknown>";
  }
}

const char* json_value_type_name(nlohmann::json::value_t type)
{
  switch (type)
  {
  case nlohmann::json::value_t::null:
    return "null";
  case nlohmann::json::value_t::object:
    return "object";
  case nlohmann::json::value_t::array:
    return "array";
  case nlohmann::json::value_t::string:
    return "string";
  case nlohmann::json::value_t::boolean:
    return "boolean";
  case nlohmann::json::value_t::number_integer:
    return "int";
  case nlohmann::json::value_t::number_unsigned:
    return "uint";
  case nlohmann::json::value_t::number_float:
    return "float";
  case nlohmann::json::value_t::binary:
    return "binary";
  case nlohmann::json::value_t::discarded:
    return "discarded";
  default:
    return "unknown";
  }
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
  size_t index = 0;

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
      throw py::type_error(std::string("JSON arrays must be homogeneous: element 0 is ")
                           + json_value_type_name(element_type) + ", element "
                           + std::to_string(index) + " is " + json_value_type_name(child.type()));
    }
    array.push_back(std::move(child));
    ++index;
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
        throw py::type_error(std::string("JSON object keys must be strings (got ")
                             + py_type_name(key_obj) + ")");
      }
      const auto key = py::cast<std::string>(key_obj);
      json_obj[key] = py_to_json_strict(item.second);
    }
    return json_obj;
  }

  throw py::type_error(std::string("Value is not JSON-serializable under BehaviorTree.PY rules: ")
                       + py_type_name(obj));
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
  const BT::PortInfo* port_info = nullptr;
  if (const auto* manifest = static_cast<const BT::TreeNode&>(node).config().manifest)
  {
    auto it = manifest->ports.find(key);
    if (it != manifest->ports.end())
    {
      port_info = &it->second;
    }
  }

  if (port_info && port_info->isStronglyTyped())
  {
    const std::type_index& expected_type = port_info->type();
    const std::string expected_name = port_info->typeName();

    auto fail = [&](const std::string& msg) -> void {
      throw py::type_error("Port '" + key + "' expects " + expected_name + ": " + msg);
    };

    auto json_lane_reason = [&](const py::handle& obj) -> std::string {
      if (obj.is_none())
      {
        return "None -> JSON null";
      }
      if (py::isinstance<py::dict>(obj))
      {
        return "dict -> JSON object";
      }
      if (py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj))
      {
        const py::sequence seq = py::reinterpret_borrow<py::sequence>(obj);
        const auto n = py::len(seq);
        if (n == 0)
        {
          return "empty list -> JSON array (element type unknown)";
        }
        const py::handle first = seq[0];
        if (py::isinstance<py::list>(first) || py::isinstance<py::tuple>(first))
        {
          return "nested list -> JSON array";
        }
        if (py::isinstance<py::dict>(first))
        {
          return "list of dict -> JSON array";
        }
      }
      return "value treated as JSON lane";
    };

    if (expected_type == typeid(nlohmann::json))
    {
      return node.setOutput(key, py_to_json_strict(value));
    }

    if (value.is_none())
    {
      fail(std::string("None is only allowed for JSON ports (") + json_lane_reason(value) + ")");
    }
    if (py::isinstance<py::dict>(value))
    {
      fail(std::string("dict is only allowed for JSON ports (") + json_lane_reason(value) + ")");
    }

    if (expected_type == typeid(bool))
    {
      if (!py::isinstance<py::bool_>(value))
      {
        fail("got " + py_type_name(value));
      }
      return node.setOutput(key, py::cast<bool>(value));
    }

    if (expected_type == typeid(int64_t))
    {
      if (!py::isinstance<py::int_>(value) || py::isinstance<py::bool_>(value))
      {
        fail("got " + py_type_name(value));
      }
      return node.setOutput(key, py_int_to_int64(value));
    }

    if (expected_type == typeid(double))
    {
      if (!py::isinstance<py::float_>(value))
      {
        fail("got " + py_type_name(value));
      }
      return node.setOutput(key, py::cast<double>(value));
    }

    if (expected_type == typeid(std::string))
    {
      if (!py::isinstance<py::str>(value))
      {
        fail("got " + py_type_name(value));
      }
      return node.setOutput(key, py::cast<std::string>(value));
    }

    if (expected_type == typeid(std::vector<bool>))
    {
      if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
      {
        fail("got " + py_type_name(value));
      }
      const py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
      const auto n = py::len(seq);
      std::vector<bool> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::bool_>(item))
        {
          fail(std::string("element ") + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<bool>(item));
        ++index;
      }
      return node.setOutput(key, out);
    }

    if (expected_type == typeid(std::vector<int64_t>))
    {
      if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
      {
        fail("got " + py_type_name(value));
      }
      const py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
      const auto n = py::len(seq);
      std::vector<int64_t> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::int_>(item) || py::isinstance<py::bool_>(item))
        {
          fail(std::string("element ") + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py_int_to_int64(item));
        ++index;
      }
      return node.setOutput(key, out);
    }

    if (expected_type == typeid(std::vector<double>))
    {
      if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
      {
        fail("got " + py_type_name(value));
      }
      const py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
      const auto n = py::len(seq);
      std::vector<double> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::float_>(item))
        {
          fail(std::string("element ") + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<double>(item));
        ++index;
      }
      return node.setOutput(key, out);
    }

    if (expected_type == typeid(std::vector<std::string>))
    {
      if (!py::isinstance<py::list>(value) && !py::isinstance<py::tuple>(value))
      {
        fail("got " + py_type_name(value));
      }
      const py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
      const auto n = py::len(seq);
      std::vector<std::string> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::str>(item))
        {
          fail(std::string("element ") + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<std::string>(item));
        ++index;
      }
      return node.setOutput(key, out);
    }

    fail("unsupported strongly-typed port type");
  }

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
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::bool_>(item))
        {
          throw py::type_error(std::string("Typed bool list must contain only bool elements; element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<bool>(item));
        ++index;
      }
      return node.setOutput(key, out);
    }

    if (py::isinstance<py::int_>(first) && !py::isinstance<py::bool_>(first))
    {
      std::vector<int64_t> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::int_>(item) || py::isinstance<py::bool_>(item))
        {
          throw py::type_error(std::string(
                                   "Typed int list must contain only int elements (no bool/float); element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py_int_to_int64(item));
        ++index;
      }
      return node.setOutput(key, out);
    }

    if (py::isinstance<py::float_>(first))
    {
      std::vector<double> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::float_>(item))
        {
          throw py::type_error(std::string("Typed float list must contain only float elements; element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<double>(item));
        ++index;
      }
      return node.setOutput(key, out);
    }

    if (py::isinstance<py::str>(first))
    {
      std::vector<std::string> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::str>(item))
        {
          throw py::type_error(std::string("Typed string list must contain only str elements; element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<std::string>(item));
        ++index;
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

  throw py::type_error(
      std::string("Unsupported value type for set_output(): ") + py_type_name(value)
      + " (supported: bool/int/float/str, lists of primitives, dict/None for JSON lane)");
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
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::bool_>(item))
        {
          throw py::type_error(std::string("Typed bool list must contain only bool elements; element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<bool>(item));
        ++index;
      }
      bb.set(key, out);
      return;
    }
    if (py::isinstance<py::int_>(first) && !py::isinstance<py::bool_>(first))
    {
      std::vector<int64_t> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::int_>(item) || py::isinstance<py::bool_>(item))
        {
          throw py::type_error(std::string(
                                   "Typed int list must contain only int elements (no bool/float); element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py_int_to_int64(item));
        ++index;
      }
      bb.set(key, out);
      return;
    }
    if (py::isinstance<py::float_>(first))
    {
      std::vector<double> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::float_>(item))
        {
          throw py::type_error(std::string("Typed float list must contain only float elements; element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<double>(item));
        ++index;
      }
      bb.set(key, out);
      return;
    }
    if (py::isinstance<py::str>(first))
    {
      std::vector<std::string> out;
      out.reserve(n);
      size_t index = 0;
      for (py::handle item : seq)
      {
        if (!py::isinstance<py::str>(item))
        {
          throw py::type_error(std::string("Typed string list must contain only str elements; element ")
                               + std::to_string(index) + " is " + py_type_name(item));
        }
        out.push_back(py::cast<std::string>(item));
        ++index;
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

  throw py::type_error(
      std::string("Unsupported value type for Blackboard.set(): ") + py_type_name(value)
      + " (supported: bool/int/float/str, lists of primitives, dict/None for JSON lane)");
}

py::dict blackboard_to_py_dict(const BT::Blackboard& bb)
{
  py::dict out;
  for (const auto& key_sv : bb.getKeys())
  {
    std::string key(key_sv);
    if (auto any_ref = bb.getAnyLocked(key))
    {
      if (auto any_ptr = any_ref.get())
      {
        if (!any_ptr->empty())
        {
          out[py::str(key)] = any_to_py(*any_ptr);
        }
      }
    }
  }
  return out;
}

void blackboard_from_py_dict(BT::Blackboard& bb, const py::handle& obj)
{
  if (!py::isinstance<py::dict>(obj))
  {
    throw py::type_error("Expected a dict for blackboard import");
  }

  const py::dict dict = py::reinterpret_borrow<py::dict>(obj);
  for (auto item : dict)
  {
    const py::handle key_obj = item.first;
    if (!py::isinstance<py::str>(key_obj))
    {
      throw py::type_error("Blackboard import dict keys must be strings");
    }
    const auto key = py::cast<std::string>(key_obj);
    blackboard_set_from_py(bb, key, item.second);
  }
}

void add_python_exception_note(py::error_already_set& err, const BT::TreeNode& node,
                               const char* phase)
{
  try
  {
    const py::object& exc = err.value();
    if (exc.is_none() || !py::hasattr(exc, "add_note"))
    {
      return;
    }

    std::string note = std::string("BehaviorTree.PY: exception in ") + phase + " for node '"
                       + node.fullPath() + "' (registration_id='" + node.registrationName()
                       + "')";
    exc.attr("add_note")(py::str(note));
  }
  catch (...)
  {
    // Best-effort: adding context must never mask the original exception.
  }
}

class NodeHandle
{
public:
  explicit NodeHandle(BT::TreeNode* node) : node_(node)
  {}

  [[nodiscard]] py::object get_input(const std::string& key) const
  {
    const BT::PortInfo* port_info = nullptr;
    const BT::TreeNode& node_ref = *node_;
    if (const auto* manifest = node_ref.config().manifest)
    {
      auto it = manifest->ports.find(key);
      if (it != manifest->ports.end())
      {
        port_info = &it->second;
      }
    }

    auto throw_input_error = [&](const std::string& err) -> void {
      throw std::runtime_error(
          "get_input('" + key + "') failed for node '" + node_->fullPath()
          + "' (registration_id='" + node_->registrationName() + "'): " + err);
    };

    if (port_info && port_info->isStronglyTyped())
    {
      const std::type_index& expected = port_info->type();

      if (expected == typeid(bool))
      {
        auto res = node_->getInput<bool>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(int64_t))
      {
        auto res = node_->getInput<int64_t>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(double))
      {
        auto res = node_->getInput<double>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(std::string))
      {
        auto res = node_->getInput<std::string>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(std::vector<bool>))
      {
        auto res = node_->getInput<std::vector<bool>>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(std::vector<int64_t>))
      {
        auto res = node_->getInput<std::vector<int64_t>>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(std::vector<double>))
      {
        auto res = node_->getInput<std::vector<double>>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(std::vector<std::string>))
      {
        auto res = node_->getInput<std::vector<std::string>>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
      if (expected == typeid(nlohmann::json))
      {
        auto res = node_->getInput<nlohmann::json>(key);
        if (!res)
        {
          throw_input_error(res.error());
        }
        return any_to_py(BT::Any(res.value()));
      }
    }

    BT::Any out;
    auto res = node_->getInput(key, out);
    if (!res)
    {
      throw_input_error(res.error());
    }
    return any_to_py(out);
  }

  void set_output(const std::string& key, const py::object& value)
  {
    auto res = set_output_from_py(*node_, key, value);
    if (!res)
    {
      throw std::runtime_error(
          "set_output('" + key + "') failed for node '" + node_->fullPath()
          + "' (registration_id='" + node_->registrationName() + "'): " + res.error());
    }
  }

  [[nodiscard]] std::string name() const
  {
    return node_->name();
  }

  [[nodiscard]] BT::NodeStatus tick_child() const
  {
    auto* decorator = dynamic_cast<BT::DecoratorNode*>(node_);
    if (!decorator)
    {
      throw std::runtime_error("tick_child() is only valid for DecoratorNode");
    }
    BT::TreeNode* child = decorator->child();
    if (!child)
    {
      throw std::runtime_error("DecoratorNode has no child to tick");
    }
    py::gil_scoped_release release;
    return child->executeTick();
  }

  [[nodiscard]] BT::NodeStatus tick_child(size_t index) const
  {
    auto* control = dynamic_cast<BT::ControlNode*>(node_);
    if (!control)
    {
      throw std::runtime_error("tick_child(index) is only valid for ControlNode");
    }
    if (index >= control->childrenCount())
    {
      throw std::out_of_range("ControlNode child index out of range: " + std::to_string(index));
    }
    py::gil_scoped_release release;
    return control->children().at(index)->executeTick();
  }

  void halt_child() const
  {
    auto* decorator = dynamic_cast<BT::DecoratorNode*>(node_);
    if (!decorator)
    {
      throw std::runtime_error("halt_child() is only valid for DecoratorNode");
    }
    py::gil_scoped_release release;
    decorator->haltChild();
  }

  void halt_child(size_t index) const
  {
    auto* control = dynamic_cast<BT::ControlNode*>(node_);
    if (!control)
    {
      throw std::runtime_error("halt_child(index) is only valid for ControlNode");
    }
    if (index >= control->childrenCount())
    {
      throw std::out_of_range("ControlNode child index out of range: " + std::to_string(index));
    }
    py::gil_scoped_release release;
    control->haltChild(index);
  }

  void halt_children() const
  {
    auto* control = dynamic_cast<BT::ControlNode*>(node_);
    if (!control)
    {
      throw std::runtime_error("halt_children() is only valid for ControlNode");
    }
    py::gil_scoped_release release;
    control->haltChildren();
  }

  void reset_child() const
  {
    auto* decorator = dynamic_cast<BT::DecoratorNode*>(node_);
    if (!decorator)
    {
      throw std::runtime_error("reset_child() is only valid for DecoratorNode");
    }
    py::gil_scoped_release release;
    decorator->resetChild();
  }

  void reset_children() const
  {
    auto* control = dynamic_cast<BT::ControlNode*>(node_);
    if (!control)
    {
      throw std::runtime_error("reset_children() is only valid for ControlNode");
    }
    py::gil_scoped_release release;
    control->resetChildren();
  }

  [[nodiscard]] size_t children_count() const
  {
    auto* control = dynamic_cast<BT::ControlNode*>(node_);
    if (!control)
    {
      throw std::runtime_error("children_count() is only valid for ControlNode");
    }
    return control->childrenCount();
  }

private:
  BT::TreeNode* node_;
};

class PyTree
{
public:
  explicit PyTree(BT::Tree&& tree) :
    tree_(std::move(tree)),
    created_on_thread_id_(std::this_thread::get_id())
  {}

  PyTree(const PyTree&) = delete;
  PyTree& operator=(const PyTree&) = delete;

  PyTree(PyTree&&) = default;
  PyTree& operator=(PyTree&&) = default;

  [[nodiscard]] BT::Tree& tree()
  {
    return tree_;
  }

  [[nodiscard]] const BT::Tree& tree() const
  {
    return tree_;
  }

  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }
    throw std::runtime_error(std::string("Tree method called from a different thread: ") + method);
  }

private:
  BT::Tree tree_;
  std::thread::id created_on_thread_id_;
};

class PyStdCoutLogger
{
public:
  explicit PyStdCoutLogger(py::object tree_obj) : tree_obj_(std::move(tree_obj))
  {
    const auto& tree = tree_obj_.cast<const PyTree&>().tree();
    logger_ = std::make_unique<BT::StdCoutLogger>(tree);
  }

  ~PyStdCoutLogger()
  {
    if (!Py_IsInitialized())
    {
      return;
    }
    py::gil_scoped_acquire acquire;
    tree_obj_ = py::none();
  }

  void flush()
  {
    logger_->flush();
  }

private:
  py::object tree_obj_;
  std::unique_ptr<BT::StdCoutLogger> logger_;
};

class PyFileLogger2
{
public:
  PyFileLogger2(py::object tree_obj, const std::string& filepath) :
    tree_obj_(std::move(tree_obj))
  {
    const auto& tree = tree_obj_.cast<const PyTree&>().tree();
    logger_ = std::make_unique<BT::FileLogger2>(tree, std::filesystem::path(filepath));
  }

  ~PyFileLogger2()
  {
    if (!Py_IsInitialized())
    {
      return;
    }
    py::gil_scoped_acquire acquire;
    tree_obj_ = py::none();
  }

  void flush()
  {
    logger_->flush();
  }

private:
  py::object tree_obj_;
  std::unique_ptr<BT::FileLogger2> logger_;
};

class PyMinitraceLogger
{
public:
  PyMinitraceLogger(py::object tree_obj, const std::string& filename_json) :
    tree_obj_(std::move(tree_obj))
  {
    const auto& tree = tree_obj_.cast<const PyTree&>().tree();
    logger_ = std::make_unique<BT::MinitraceLogger>(tree, filename_json.c_str());
  }

  ~PyMinitraceLogger()
  {
    if (!Py_IsInitialized())
    {
      return;
    }
    py::gil_scoped_acquire acquire;
    tree_obj_ = py::none();
  }

  void flush()
  {
    logger_->flush();
  }

private:
  py::object tree_obj_;
  std::unique_ptr<BT::MinitraceLogger> logger_;
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
    try
    {
      py::object result = py_instance_.attr("tick")();
      return result.cast<BT::NodeStatus>();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "tick()");
      throw;
    }
  }

private:
  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }

    throw std::runtime_error(std::string("Python node method called from a different thread: ")
                             + method + " (node='" + fullPath() + "')");
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
    try
    {
      py::object result = py_instance_.attr("on_start")();
      return result.cast<BT::NodeStatus>();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "on_start()");
      throw;
    }
  }

  BT::NodeStatus onRunning() override
  {
    enforce_thread_or_throw("on_running");
    py::gil_scoped_acquire acquire;
    try
    {
      py::object result = py_instance_.attr("on_running")();
      return result.cast<BT::NodeStatus>();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "on_running()");
      throw;
    }
  }

  void onHalted() override
  {
    enforce_thread_or_throw("on_halted");
    py::gil_scoped_acquire acquire;
    try
    {
      py_instance_.attr("on_halted")();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "on_halted()");
      throw;
    }
  }

private:
  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }

    throw std::runtime_error(std::string("Python node method called from a different thread: ")
                             + method + " (node='" + fullPath() + "')");
  }

  std::thread::id created_on_thread_id_;
  py::object py_instance_;
};

class PyConditionNode final : public BT::ConditionNode
{
public:
  PyConditionNode(const std::string& name, const BT::NodeConfig& config, py::type type,
                  py::tuple ctor_args, py::dict ctor_kwargs) :
    BT::ConditionNode(name, config),
    created_on_thread_id_(std::this_thread::get_id())
  {
    py::gil_scoped_acquire acquire;
    py_instance_ = type(py::str(name), *ctor_args, **ctor_kwargs);
    py_instance_.attr("_bt") = py::cast(NodeHandle(this));
  }

  ~PyConditionNode() override
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
    try
    {
      py::object result = py_instance_.attr("tick")();
      return result.cast<BT::NodeStatus>();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "tick()");
      throw;
    }
  }

private:
  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }

    throw std::runtime_error(std::string("Python node method called from a different thread: ")
                             + method + " (node='" + fullPath() + "')");
  }

  std::thread::id created_on_thread_id_;
  py::object py_instance_;
};

class PyDecoratorNode final : public BT::DecoratorNode
{
public:
  PyDecoratorNode(const std::string& name, const BT::NodeConfig& config, py::type type,
                  py::tuple ctor_args, py::dict ctor_kwargs) :
    BT::DecoratorNode(name, config),
    created_on_thread_id_(std::this_thread::get_id())
  {
    py::gil_scoped_acquire acquire;
    py_instance_ = type(py::str(name), *ctor_args, **ctor_kwargs);
    py_instance_.attr("_bt") = py::cast(NodeHandle(this));
  }

  ~PyDecoratorNode() override
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
    try
    {
      py::object result = py_instance_.attr("tick")();
      return result.cast<BT::NodeStatus>();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "tick()");
      throw;
    }
  }

  void halt() override
  {
    enforce_thread_or_throw("halt");

    const bool was_running = (status() == BT::NodeStatus::RUNNING);
    BT::DecoratorNode::halt();

    if (!was_running || !Py_IsInitialized())
    {
      return;
    }
    py::gil_scoped_acquire acquire;
    try
    {
      py_instance_.attr("halt")();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "halt()");
      throw;
    }
  }

private:
  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }

    throw std::runtime_error(std::string("Python node method called from a different thread: ")
                             + method + " (node='" + fullPath() + "')");
  }

  std::thread::id created_on_thread_id_;
  py::object py_instance_;
};

class PyControlNode final : public BT::ControlNode
{
public:
  PyControlNode(const std::string& name, const BT::NodeConfig& config, py::type type,
                py::tuple ctor_args, py::dict ctor_kwargs) :
    BT::ControlNode(name, config),
    created_on_thread_id_(std::this_thread::get_id())
  {
    py::gil_scoped_acquire acquire;
    py_instance_ = type(py::str(name), *ctor_args, **ctor_kwargs);
    py_instance_.attr("_bt") = py::cast(NodeHandle(this));
  }

  ~PyControlNode() override
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
    try
    {
      py::object result = py_instance_.attr("tick")();
      return result.cast<BT::NodeStatus>();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "tick()");
      throw;
    }
  }

  void halt() override
  {
    enforce_thread_or_throw("halt");

    const bool was_running = (status() == BT::NodeStatus::RUNNING);
    BT::ControlNode::halt();

    if (!was_running || !Py_IsInitialized())
    {
      return;
    }
    py::gil_scoped_acquire acquire;
    try
    {
      py_instance_.attr("halt")();
    }
    catch (py::error_already_set& err)
    {
      add_python_exception_note(err, *this, "halt()");
      throw;
    }
  }

private:
  void enforce_thread_or_throw(const char* method) const
  {
    if (std::this_thread::get_id() == created_on_thread_id_)
    {
      return;
    }

    throw std::runtime_error(std::string("Python node method called from a different thread: ")
                             + method + " (node='" + fullPath() + "')");
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

BT::NodeBuilder make_condition_builder(const py::type& type, const py::args& args,
                                       const py::kwargs& kwargs)
{
  py::tuple ctor_args(args);
  py::dict ctor_kwargs(kwargs);

  return [type, ctor_args, ctor_kwargs](const std::string& name,
                                        const BT::NodeConfig& config) -> std::unique_ptr<BT::TreeNode> {
    return std::make_unique<PyConditionNode>(name, config, type, ctor_args, ctor_kwargs);
  };
}

BT::NodeBuilder make_decorator_builder(const py::type& type, const py::args& args,
                                      const py::kwargs& kwargs)
{
  py::tuple ctor_args(args);
  py::dict ctor_kwargs(kwargs);

  return [type, ctor_args, ctor_kwargs](const std::string& name,
                                       const BT::NodeConfig& config) -> std::unique_ptr<BT::TreeNode> {
    return std::make_unique<PyDecoratorNode>(name, config, type, ctor_args, ctor_kwargs);
  };
}

BT::NodeBuilder make_control_builder(const py::type& type, const py::args& args,
                                    const py::kwargs& kwargs)
{
  py::tuple ctor_args(args);
  py::dict ctor_kwargs(kwargs);

  return [type, ctor_args, ctor_kwargs](const std::string& name,
                                       const BT::NodeConfig& config) -> std::unique_ptr<BT::TreeNode> {
    return std::make_unique<PyControlNode>(name, config, type, ctor_args, ctor_kwargs);
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

  py::enum_<BT::NodeType>(m, "NodeType")
      .value("UNDEFINED", BT::NodeType::UNDEFINED)
      .value("ACTION", BT::NodeType::ACTION)
      .value("CONDITION", BT::NodeType::CONDITION)
      .value("CONTROL", BT::NodeType::CONTROL)
      .value("DECORATOR", BT::NodeType::DECORATOR)
      .value("SUBTREE", BT::NodeType::SUBTREE)
      .export_values();

  py::enum_<BT::PortDirection>(m, "PortDirection")
      .value("INPUT", BT::PortDirection::INPUT)
      .value("OUTPUT", BT::PortDirection::OUTPUT)
      .value("INOUT", BT::PortDirection::INOUT)
      .export_values();

  py::class_<BT::TypeInfo>(m, "TypeInfo")
      .def_property_readonly("type_name", &BT::TypeInfo::typeName)
      .def_property_readonly("is_strongly_typed", &BT::TypeInfo::isStronglyTyped);

  py::class_<BT::PortInfo, BT::TypeInfo>(m, "PortInfo")
      .def_property_readonly("direction", &BT::PortInfo::direction)
      .def_property_readonly("description", &BT::PortInfo::description)
      .def_property_readonly("default_value_string", &BT::PortInfo::defaultValueString);

  py::class_<BT::TreeNode>(m, "TreeNode")
      .def_property_readonly("uid", &BT::TreeNode::UID)
      .def_property_readonly("name", [](const BT::TreeNode& node) { return node.name(); })
      .def_property_readonly("full_path",
                             [](const BT::TreeNode& node) { return node.fullPath(); })
      .def_property_readonly(
          "registration_name",
          [](const BT::TreeNode& node) { return node.registrationName(); },
          "Factory registration ID for this node instance.")
      .def_property_readonly("status", &BT::TreeNode::status)
      .def_property_readonly("type", &BT::TreeNode::type)
      .def_property_readonly(
          "blackboard",
          [](const BT::TreeNode& node) { return node.config().blackboard; },
          "Blackboard associated with this node (usually the subtree blackboard).")
      .def_property_readonly("input_ports",
                             [](const BT::TreeNode& node) {
                               py::dict out;
                               for (const auto& kv : node.config().input_ports)
                               {
                                 out[py::str(kv.first)] = py::str(kv.second);
                               }
                               return out;
                             })
      .def_property_readonly("output_ports",
                             [](const BT::TreeNode& node) {
                               py::dict out;
                               for (const auto& kv : node.config().output_ports)
                               {
                                 out[py::str(kv.first)] = py::str(kv.second);
                               }
                               return out;
                             })
      .def_property_readonly(
          "ports",
          [](const BT::TreeNode& node) {
            py::dict out;
            const auto* manifest = node.config().manifest;
            if (!manifest)
            {
              return out;
            }

            for (const auto& kv : manifest->ports)
            {
              const std::string& port_name = kv.first;
              const BT::PortInfo& info = kv.second;

              py::dict port;
              port[py::str("direction")] = info.direction();
              port[py::str("type")] = py::str(info.typeName());
              port[py::str("is_strongly_typed")] = py::bool_(info.isStronglyTyped());
              if (!info.description().empty())
              {
                port[py::str("description")] = py::str(info.description());
              }
              if (!info.defaultValueString().empty())
              {
                port[py::str("default_value")] = py::str(info.defaultValueString());
              }

              out[py::str(port_name)] = port;
            }
            return out;
          },
          "Ports declared for this node (from its manifest).")
      .def_property_readonly("ports_info",
                             [](const BT::TreeNode& node) {
                               py::dict out;
                               const auto* manifest = node.config().manifest;
                               if (!manifest)
                               {
                                 return out;
                               }
                               for (const auto& kv : manifest->ports)
                               {
                                 out[py::str(kv.first)] = py::cast(kv.second);
                               }
                               return out;
                             })
      .def(
          "children",
          [](const py::object& self) {
            const auto* node = self.cast<const BT::TreeNode*>();

            py::list out;
            if (auto control = dynamic_cast<const BT::ControlNode*>(node))
            {
              for (const auto* child : control->children())
              {
                out.append(py::cast(child, py::return_value_policy::reference_internal, self));
              }
            }
            else if (auto decorator = dynamic_cast<const BT::DecoratorNode*>(node))
            {
              if (const auto* child = decorator->child())
              {
                out.append(py::cast(child, py::return_value_policy::reference_internal, self));
              }
            }

            return out;
          },
          "Return child nodes (ControlNode children or DecoratorNode child).");

  py::class_<NodeHandle>(m, "_NodeHandle")
      .def("get_input", &NodeHandle::get_input, py::arg("key"))
      .def("set_output", &NodeHandle::set_output, py::arg("key"), py::arg("value"))
      .def("tick_child",
           py::overload_cast<>(&NodeHandle::tick_child, py::const_),
           "Tick the child of a DecoratorNode and return its status.")
      .def("tick_child",
           py::overload_cast<size_t>(&NodeHandle::tick_child, py::const_),
           py::arg("index"),
           "Tick the child at index of a ControlNode and return its status.")
      .def("halt_child",
           py::overload_cast<>(&NodeHandle::halt_child, py::const_),
           "Halt the child of a DecoratorNode.")
      .def("halt_child",
           py::overload_cast<size_t>(&NodeHandle::halt_child, py::const_),
           py::arg("index"),
           "Halt the child at index of a ControlNode.")
      .def("halt_children", &NodeHandle::halt_children, "Halt all children of a ControlNode.")
      .def("reset_child", &NodeHandle::reset_child, "Reset the child of a DecoratorNode to IDLE.")
      .def("reset_children", &NodeHandle::reset_children, "Reset all children of a ControlNode to IDLE.")
      .def_property_readonly("children_count",
                             &NodeHandle::children_count,
                             "Number of children (ControlNode only).")
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
      .def("unset", &BT::Blackboard::unset, py::arg("key"), "Unset a blackboard key.")
      .def(
          "to_json",
          [](const BT::Blackboard& bb) { return blackboard_to_py_dict(bb); },
          "Export the blackboard to a JSON-compatible Python object.")
      .def(
          "from_json",
          [](BT::Blackboard& bb, const py::object& obj) { blackboard_from_py_dict(bb, obj); },
          py::arg("obj"),
          "Import values into the blackboard from a JSON-compatible Python object.")
      .def(
          "to_bt_json",
          [](const BT::Blackboard& bb) { return json_to_py(BT::ExportBlackboardToJSON(bb)); },
          "Export using BT.CPP JsonExporter (may omit unsupported types).")
      .def(
          "from_bt_json",
          [](BT::Blackboard& bb, const py::object& obj) {
            BT::ImportBlackboardFromJSON(py_to_json_strict(obj), bb);
          },
          py::arg("obj"),
          "Import using BT.CPP JsonExporter (requires BT.CPP-supported JSON formats).");

  py::class_<PyTree>(m, "Tree")
      .def_property_readonly(
          "root_node",
          [](const py::object& self) -> py::object {
            const BT::Tree& tree = self.cast<const PyTree&>().tree();
            if (auto* root = tree.rootNode())
            {
              return py::cast(root, py::return_value_policy::reference_internal, self);
            }
            return py::none();
          },
          "Return the root TreeNode (or None).")
      .def(
          "nodes",
          [](const py::object& self) {
            const BT::Tree& tree = self.cast<const PyTree&>().tree();
            py::list out;
            for (const auto& subtree : tree.subtrees)
            {
              for (const auto& node : subtree->nodes)
              {
                out.append(py::cast(node.get(), py::return_value_policy::reference_internal, self));
              }
            }
            return out;
          },
          "Return all nodes in the tree.")
      .def(
          "get_nodes_by_path",
          [](const py::object& self, const std::string& wildcard_filter) {
            const BT::Tree& tree = self.cast<const PyTree&>().tree();
            py::list out;
            for (const auto& subtree : tree.subtrees)
            {
              for (const auto& node : subtree->nodes)
              {
                if (BT::WildcardMatch(node->fullPath(), wildcard_filter))
                {
                  out.append(
                      py::cast(node.get(), py::return_value_policy::reference_internal, self));
                }
              }
            }
            return out;
          },
          py::arg("wildcard_filter"),
          "Return nodes whose full_path matches the wildcard filter.")
      .def(
          "apply_visitor",
          [](const py::object& self, const py::function& visitor) {
            const BT::Tree& tree = self.cast<const PyTree&>().tree();
            for (const auto& subtree : tree.subtrees)
            {
              for (const auto& node : subtree->nodes)
              {
                visitor(py::cast(node.get(), py::return_value_policy::reference_internal, self));
              }
            }
          },
          py::arg("visitor"),
          "Call visitor(TreeNode) for each node in the tree.")
      .def(
          "tick_once",
          [](PyTree& self) {
            self.enforce_thread_or_throw("tick_once");
            py::gil_scoped_release release;
            return self.tree().tickOnce();
          },
          "Tick the tree once.")
      .def(
          "tick_exactly_once",
          [](PyTree& self) {
            self.enforce_thread_or_throw("tick_exactly_once");
            py::gil_scoped_release release;
            return self.tree().tickExactlyOnce();
          },
          "Tick the tree once, expecting completion in a single tick.")
      .def(
          "tick_while_running",
          [](PyTree& self, int sleep_ms) {
            self.enforce_thread_or_throw("tick_while_running");
            py::gil_scoped_release release;
            return self.tree().tickWhileRunning(std::chrono::milliseconds(sleep_ms));
          },
          py::arg("sleep_ms") = 10,
          "Tick until completion, sleeping between ticks.")
      .def("root_blackboard", [](PyTree& self) { return self.tree().rootBlackboard(); },
           "Return the root blackboard.")
      .def("to_json",
           [](const PyTree& self) {
             const BT::Tree& tree = self.tree();
             py::dict out;
             for (const auto& subtree : tree.subtrees)
             {
               auto name = subtree->instance_name;
               if (name.empty())
               {
                 name = subtree->tree_ID;
               }
               out[py::str(name)] = blackboard_to_py_dict(*subtree->blackboard);
             }
             return out;
           },
           "Export the tree blackboards to a JSON-compatible Python object.")
      .def("from_json",
           [](PyTree& self, const py::object& obj) {
             BT::Tree& tree = self.tree();
             if (!py::isinstance<py::dict>(obj))
             {
               throw py::type_error("Expected a dict for tree import");
             }
             const py::dict dict = py::reinterpret_borrow<py::dict>(obj);
             for (const auto& subtree : tree.subtrees)
             {
               auto name = subtree->instance_name;
               if (name.empty())
               {
                 name = subtree->tree_ID;
               }
               const py::str key(name);
               if (!dict.contains(key))
               {
                 throw py::key_error("Missing subtree blackboard in import data: " + name);
               }
               blackboard_from_py_dict(*subtree->blackboard, dict[key]);
             }
           },
           py::arg("obj"),
           "Import values into all tree blackboards from a JSON-compatible Python object.")
      .def("to_bt_json",
           [](const PyTree& self) { return json_to_py(BT::ExportTreeToJSON(self.tree())); },
           "Export using BT.CPP JsonExporter (may omit unsupported types).")
      .def("from_bt_json",
           [](PyTree& self, const py::object& obj) {
             BT::ImportTreeFromJSON(py_to_json_strict(obj), self.tree());
           },
           py::arg("obj"),
           "Import using BT.CPP JsonExporter (requires BT.CPP-supported JSON formats).")
      .def(
          "halt_tree",
          [](PyTree& self) {
            self.enforce_thread_or_throw("halt_tree");
            py::gil_scoped_release release;
            self.tree().haltTree();
          },
          "Halt execution of the tree.");

  py::class_<PyStdCoutLogger>(m, "StdCoutLogger")
      .def(py::init<py::object>(), py::arg("tree"))
      .def("flush", &PyStdCoutLogger::flush);

  py::class_<PyFileLogger2>(m, "FileLogger2")
      .def(py::init<py::object, std::string>(), py::arg("tree"), py::arg("filepath"))
      .def("flush", &PyFileLogger2::flush);

  py::class_<PyMinitraceLogger>(m, "MinitraceLogger")
      .def(py::init<py::object, std::string>(),
           py::arg("tree"),
           py::arg("filename_json"))
      .def("flush", &PyMinitraceLogger::flush);

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
          "  - 'inputs': list[str|dict]\n"
          "  - 'outputs': list[str|dict]\n"
          "  - 'inouts': list[str|dict] (optional)\n"
          "Port entries may be either a string name or a dict {'name': str, 'type': str}.\n"
          "Supported 'type' strings: any, bool, int, float, str, json, list[bool|int|float|str].\n"
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
          "  - 'inputs': list[str|dict]\n"
          "  - 'outputs': list[str|dict]\n"
          "  - 'inouts': list[str|dict] (optional)\n"
          "Port entries may be either a string name or a dict {'name': str, 'type': str}.\n"
          "Supported 'type' strings: any, bool, int, float, str, json, list[bool|int|float|str].\n"
          "The constructor is called as: node_type(name, *args, **kwargs).")
      .def(
          "register_condition",
          [](BT::BehaviorTreeFactory& factory, const py::type& type, const py::args& args,
             const py::kwargs& kwargs) {
            const std::string name = type.attr("__name__").cast<std::string>();

            BT::TreeNodeManifest manifest;
            manifest.type = BT::NodeType::CONDITION;
            manifest.registration_ID = name;
            manifest.ports = extract_ports_list_or_throw(type);
            manifest.metadata = {};

            factory.registerBuilder(manifest, make_condition_builder(type, args, kwargs));
          },
          py::arg("node_type"),
          "Register a Python ConditionNode type.\n\n"
          "The node class must define @classmethod provided_ports() -> dict with keys:\n"
          "  - 'inputs': list[str|dict]\n"
          "  - 'outputs': list[str|dict]\n"
          "  - 'inouts': list[str|dict] (optional)\n"
          "Port entries may be either a string name or a dict {'name': str, 'type': str}.\n"
          "Supported 'type' strings: any, bool, int, float, str, json, list[bool|int|float|str].\n"
          "The constructor is called as: node_type(name, *args, **kwargs).")
      .def(
          "register_decorator",
          [](BT::BehaviorTreeFactory& factory, const py::type& type, const py::args& args,
             const py::kwargs& kwargs) {
            const std::string name = type.attr("__name__").cast<std::string>();

            BT::TreeNodeManifest manifest;
            manifest.type = BT::NodeType::DECORATOR;
            manifest.registration_ID = name;
            manifest.ports = extract_ports_list_or_throw(type);
            manifest.metadata = {};

            factory.registerBuilder(manifest, make_decorator_builder(type, args, kwargs));
          },
          py::arg("node_type"),
          "Register a Python DecoratorNode type.\n\n"
          "The node class must define @classmethod provided_ports() -> dict with keys:\n"
          "  - 'inputs': list[str|dict]\n"
          "  - 'outputs': list[str|dict]\n"
          "  - 'inouts': list[str|dict] (optional)\n"
          "Port entries may be either a string name or a dict {'name': str, 'type': str}.\n"
          "Supported 'type' strings: any, bool, int, float, str, json, list[bool|int|float|str].\n"
          "The constructor is called as: node_type(name, *args, **kwargs).")
      .def(
          "register_control",
          [](BT::BehaviorTreeFactory& factory, const py::type& type, const py::args& args,
             const py::kwargs& kwargs) {
            const std::string name = type.attr("__name__").cast<std::string>();

            BT::TreeNodeManifest manifest;
            manifest.type = BT::NodeType::CONTROL;
            manifest.registration_ID = name;
            manifest.ports = extract_ports_list_or_throw(type);
            manifest.metadata = {};

            factory.registerBuilder(manifest, make_control_builder(type, args, kwargs));
          },
          py::arg("node_type"),
          "Register a Python ControlNode type.\n\n"
          "The node class must define @classmethod provided_ports() -> dict with keys:\n"
          "  - 'inputs': list[str|dict]\n"
          "  - 'outputs': list[str|dict]\n"
          "  - 'inouts': list[str|dict] (optional)\n"
          "Port entries may be either a string name or a dict {'name': str, 'type': str}.\n"
          "Supported 'type' strings: any, bool, int, float, str, json, list[bool|int|float|str].\n"
          "The constructor is called as: node_type(name, *args, **kwargs).")
      .def(
          "register_from_plugin",
          &BT::BehaviorTreeFactory::registerFromPlugin,
          py::arg("path"),
          "Register nodes from a shared library plugin.")
      .def(
          "create_tree_from_text",
          [](BT::BehaviorTreeFactory& factory, const std::string& text) {
            return PyTree(factory.createTreeFromText(text));
          },
          py::arg("text"),
          "Create a Tree from an XML string.")
      .def(
          "create_tree_from_file",
          [](BT::BehaviorTreeFactory& factory, const std::string& path) {
            return PyTree(factory.createTreeFromFile(path));
          },
          py::arg("path"),
          "Create a Tree from an XML file path.")
      .def(
          "register_behavior_tree_from_file",
          [](BT::BehaviorTreeFactory& factory, const std::string& path) {
            factory.registerBehaviorTreeFromFile(path);
          },
          py::arg("path"),
          "Register one or more BehaviorTree definitions from an XML file (no instantiation).")
      .def(
          "register_behavior_tree_from_text",
          [](BT::BehaviorTreeFactory& factory, const std::string& text) {
            factory.registerBehaviorTreeFromText(text);
          },
          py::arg("text"),
          "Register one or more BehaviorTree definitions from XML text (no instantiation).")
      .def(
          "registered_behavior_trees",
          [](const BT::BehaviorTreeFactory& factory) {
            py::list out;
            for (const auto& name : factory.registeredBehaviorTrees())
            {
              out.append(py::str(name));
            }
            return out;
          },
          "Return IDs of BehaviorTrees registered via register_behavior_tree_*.")
      .def("clear_registered_behavior_trees",
           &BT::BehaviorTreeFactory::clearRegisteredBehaviorTrees,
           "Clear previously-registered BehaviorTrees.")
      .def(
          "create_tree",
          [](BT::BehaviorTreeFactory& factory, const std::string& tree_name) {
            return PyTree(factory.createTree(tree_name));
          },
          py::arg("tree_name"),
          "Instantiate a previously-registered BehaviorTree by ID.");

#ifdef BEHAVIORTREE_PY_VERSION
  m.attr("__behaviortree_py_version__") = py::str(BEHAVIORTREE_PY_VERSION);
#endif
}
