# API Overview

## Core Types

- `BehaviorTreeFactory`
- `Tree`
- `TreeNode`
- `Blackboard`
- Node base classes:
  - `SyncActionNode`
  - `StatefulActionNode`
  - `ConditionNode`
  - `DecoratorNode`
  - `ControlNode`

## Registering Python Nodes

- `register_sync_action(node_type, *ctor_args, **ctor_kwargs)`
- `register_stateful_action(node_type, *ctor_args, **ctor_kwargs)`
- `register_condition(node_type, *ctor_args, **ctor_kwargs)`
- `register_decorator(node_type, *ctor_args, **ctor_kwargs)`
- `register_control(node_type, *ctor_args, **ctor_kwargs)`

## Tree Construction

- `create_tree_from_text(xml, blackboard=None)`
- `create_tree_from_file(path, blackboard=None)`
- `register_behavior_tree_from_text(xml)`
- `register_behavior_tree_from_file(path)`
- `create_tree(tree_id, blackboard=None)`

## Execution

- `tick_once()`
- `tick_exactly_once()`
- `tick_while_running(sleep_ms=10)`
- `halt_tree()`

## Introspection

- `tree.nodes()`
- `tree.root_node`
- `tree.apply_visitor(fn)`
- `tree.get_nodes_by_path(wildcard)`

## Blackboard

- `Blackboard.create(parent=None)`
- `set(key, value)`
- `get(key)`
- `keys()`
- `to_json() / from_json()`
- `to_bt_json() / from_bt_json()`

## Scripting / Substitution / Loop

- `register_scripting_enum(name, value)`
- `register_scripting_enums({name: value})`
- `add_substitution_rule(filter, rule)`
- `load_substitution_rules_from_json(json_text)`
- `register_loop_node(registration_id, value_type)`
