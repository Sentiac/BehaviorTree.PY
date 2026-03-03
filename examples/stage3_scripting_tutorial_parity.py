import behaviortree_py as bt


def main() -> None:
    factory = bt.BehaviorTreeFactory()
    factory.register_scripting_enum("THE_ANSWER", 42)
    factory.register_scripting_enums({"RED": 1, "BLUE": 2, "GREEN": 3})
    tree = factory.create_tree_from_text(
        """
<root BTCPP_format="4" main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="msg:='hello world'"/>
      <Script code="A:=THE_ANSWER; B:=3.14; color:=RED"/>
      <Precondition if="A>-B && color != BLUE" else="FAILURE">
        <AlwaysSuccess/>
      </Precondition>
    </Sequence>
  </BehaviorTree>
</root>
""".strip()
    )

    status = tree.tick_once()
    bb = tree.root_blackboard()
    print("tick:", status)
    print("A:", bb.get("A"))
    print("B:", bb.get("B"))
    print("msg:", bb.get("msg"))
    print("color:", bb.get("color"))

    print("note: Script works for primitive/list arithmetic, boolean expressions, and enum constants.")
    print(
        "note: Script cannot materialize Python-only/custom C++ types; use explicit JSON ports or a C++ type plugin for custom structs."
    )


if __name__ == "__main__":
    main()
