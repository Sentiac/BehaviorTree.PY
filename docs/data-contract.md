# Data Contract

BehaviorTree.PY enforces explicit conversion rules between Python and BT.CPP types.

## Typed Port Types

Supported `provided_ports()` types:

- `bool`
- `int`
- `float`
- `str`
- `list[bool]`
- `list[int]`
- `list[float]`
- `list[str]`
- `json`
- `pose2d`
- `queue[pose2d]`

## JSON Lane

Use `type: "json"` for structured payloads:

- `dict`
- nested lists
- `None` (`null`)

JSON arrays are required to be homogeneous.

## Default Values In Port Specs

Port dict entries may include:

- `default`
- `description`

Example:

```python
{"name": "pointA", "type": "int", "default": "1", "description": "fallback value"}
```

## Lock-Scoped Port Access

For by-reference mutation semantics:

```python
locked = self.get_locked_port_content("vector")
if locked.empty():
    locked.set([3])
else:
    locked.append(5)
```

This mirrors BT.CPP lock-scoped content access patterns for mutable values.
