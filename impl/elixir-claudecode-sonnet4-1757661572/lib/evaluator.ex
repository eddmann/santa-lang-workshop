defmodule ElfLang.Evaluator do
  @moduledoc """
  Evaluator for elf-lang AST
  """

  defstruct env: %{}, mutable: MapSet.new(), closure_refs: %{}, next_closure_id: 0

  def new() do
    %ElfLang.Evaluator{}
  end

  def evaluate(ast, state \\ new()) do
    case ast do
      %{"type" => "Program", "statements" => statements} ->
        evaluate_statements(statements, state, nil)

      _ ->
        evaluate_expression(ast, state)
    end
  end

  defp evaluate_statements([], state, last_result), do: {last_result || nil_value(), state}

  defp evaluate_statements([stmt | rest], state, last_result) do
    {result, new_state} = evaluate_statement(stmt, state)
    # Comments don't contribute to the final result
    new_last_result = if stmt["type"] == "Comment", do: last_result, else: result
    evaluate_statements(rest, new_state, new_last_result)
  end

  defp evaluate_statement(%{"type" => "Expression", "value" => expr}, state) do
    evaluate_expression(expr, state)
  end

  defp evaluate_statement(%{"type" => "Comment"}, state) do
    # Comments don't produce any result and don't change state
    {nil_value(), state}
  end

  defp evaluate_expression(expr, state) do
    case expr["type"] do
      "Integer" ->
        # Normalize by removing underscores
        normalized = String.replace(expr["value"], "_", "")
        value = String.to_integer(normalized)
        {value, state}

      "Decimal" ->
        # Normalize by removing underscores
        normalized = String.replace(expr["value"], "_", "")
        value = String.to_float(normalized)
        {value, state}

      "String" ->
        {expr["value"], state}

      "Boolean" ->
        {expr["value"], state}

      "Nil" ->
        {nil_value(), state}

      "Let" ->
        {value, new_state} = evaluate_expression(expr["value"], state)
        var_name = expr["name"]["name"]
        env = Map.put(new_state.env, var_name, value)
        {nil_value(), %{new_state | env: env}}

      "MutableLet" ->
        {value, new_state} = evaluate_expression(expr["value"], state)
        var_name = expr["name"]["name"]
        env = Map.put(new_state.env, var_name, value)
        mutable = MapSet.put(new_state.mutable, var_name)
        {nil_value(), %{new_state | env: env, mutable: mutable}}

      "Identifier" ->
        var_name = expr["name"]
        case Map.get(state.env, var_name) do
          nil -> 
            # Check if it's a built-in function
            case var_name do
              "puts" -> {"puts", state}
              "+" -> {"+", state}
              "-" -> {"-", state}
              "*" -> {"*", state}
              "/" -> {"/", state}
              "size" -> {"size", state}
              "push" -> {"push", state}
              "first" -> {"first", state}
              "rest" -> {"rest", state}
              "assoc" -> {"assoc", state}
              "map" -> {"map", state}
              "filter" -> {"filter", state}
              "fold" -> {"fold", state}
              _ -> raise "Identifier can not be found: #{var_name}"
            end
          value -> 
            {value, state}
        end

      "Infix" ->
        evaluate_infix(expr, state)

      "Unary" ->
        evaluate_unary(expr, state)

      "FunctionCall" ->
        evaluate_function_call(expr, state)
      
      "Call" ->
        evaluate_call(expr, state)

      "Assignment" ->
        evaluate_assignment(expr, state)

      "List" ->
        evaluate_list(expr, state)

      "Set" ->
        evaluate_set(expr, state)

      "Dictionary" ->
        evaluate_dict(expr, state)

      "Index" ->
        evaluate_index(expr, state)

      "Function" ->
        evaluate_function_literal(expr, state)

      "Block" ->
        evaluate_block(expr, state)

      "FunctionComposition" ->
        evaluate_function_composition(expr, state)

      "FunctionThread" ->
        evaluate_function_thread(expr, state)

      "If" ->
        evaluate_if(expr, state)

      _ ->
        raise "Unknown expression type: #{expr["type"]}"
    end
  end

  defp evaluate_infix(expr, state) do
    {left, state1} = evaluate_expression(expr["left"], state)
    {right, state2} = evaluate_expression(expr["right"], state1)
    operator = expr["operator"]

    result = case operator do
      "+" -> add(left, right)
      "-" -> subtract(left, right)
      "*" -> multiply(left, right)
      "/" -> divide(left, right)
      "&&" -> logical_and(left, right)
      "||" -> logical_or(left, right)
      "==" -> equal(left, right)
      "!=" -> not equal(left, right)
      ">" -> greater_than(left, right)
      "<" -> less_than(left, right)
      ">=" -> greater_than_or_equal(left, right)
      "<=" -> less_than_or_equal(left, right)
      ">>" -> compose_functions(left, right)
      "|>" -> thread_value(left, right, state2)
      _ -> raise "Unknown operator: #{operator}"
    end

    {result, state2}
  end

  defp evaluate_unary(expr, state) do
    {operand, new_state} = evaluate_expression(expr["operand"], state)
    operator = expr["operator"]

    result = case operator do
      "-" -> unary_minus(operand)
      "!" -> unary_not(operand)
      _ -> raise "Unknown unary operator: #{operator}"
    end

    {result, new_state}
  end

  defp evaluate_assignment(expr, state) do
    var_name = expr["name"]["name"]
    
    # Check if variable exists and is mutable
    case Map.get(state.env, var_name) do
      nil ->
        raise "Unknown variable: #{var_name}"
      _value ->
        if MapSet.member?(state.mutable, var_name) do
          {new_value, new_state} = evaluate_expression(expr["value"], state)
          env = Map.put(new_state.env, var_name, new_value)
          {new_value, %{new_state | env: env}}  # Return the assigned value instead of nil
        else
          raise "Variable '#{var_name}' is not mutable"
        end
    end
  end

  defp evaluate_function_call(expr, state) do
    function_name = expr["name"]
    arguments = expr["arguments"]

    case function_name do
      "puts" ->
        evaluate_puts(arguments, state)
      "+" ->
        evaluate_operator_function("+", arguments, state)
      "-" ->
        evaluate_operator_function("-", arguments, state)
      "*" ->
        evaluate_operator_function("*", arguments, state)
      "/" ->
        evaluate_operator_function("/", arguments, state)
      "push" ->
        evaluate_push(arguments, state)
      "first" ->
        evaluate_first(arguments, state)
      "rest" ->
        evaluate_rest(arguments, state)
      "size" ->
        evaluate_size(arguments, state)
      "assoc" ->
        evaluate_assoc(arguments, state)
      "map" ->
        evaluate_map(arguments, state)
      "filter" ->
        evaluate_filter(arguments, state)
      "fold" ->
        evaluate_fold(arguments, state)
      _ ->
        # Check if function_name is a variable containing a function
        case Map.get(state.env, function_name) do
          nil -> 
            raise "Identifier can not be found: #{function_name}"
          function_value ->
            {args, final_state} = evaluate_arguments(arguments, state, [])
            apply_function(function_value, args, final_state)
        end
    end
  end

  defp evaluate_call(expr, state) do
    # Evaluate the function expression first
    {function_value, state1} = evaluate_expression(expr["function"], state)
    arguments = expr["arguments"]

    # Handle built-in functions by their values
    case function_value do
      "puts" ->
        evaluate_puts(arguments, state1)
      "+" ->
        evaluate_operator_function("+", arguments, state1)
      "-" ->
        evaluate_operator_function("-", arguments, state1)
      "*" ->
        evaluate_operator_function("*", arguments, state1)
      "/" ->
        evaluate_operator_function("/", arguments, state1)
      "push" ->
        evaluate_push(arguments, state1)
      "first" ->
        evaluate_first(arguments, state1)
      "rest" ->
        evaluate_rest(arguments, state1)
      "size" ->
        evaluate_size(arguments, state1)
      "assoc" ->
        evaluate_assoc(arguments, state1)
      "map" ->
        evaluate_map(arguments, state1)
      "filter" ->
        evaluate_filter(arguments, state1)
      "fold" ->
        evaluate_fold(arguments, state1)
      _ ->
        # Check if it's a function we can call
        if is_function_value(function_value) do
          {args, final_state} = evaluate_arguments(arguments, state1, [])
          apply_function(function_value, args, final_state)
        else
          # Generate the appropriate error message
          type_name = get_type_name(function_value)
          raise "Expected a Function, found: #{type_name}"
        end
    end
  end

  defp evaluate_puts(arguments, state) do
    {values, final_state} = evaluate_arguments(arguments, state, [])
    
    # Print each argument separated by spaces, then a trailing space
    output = values
             |> Enum.map(&format_value/1)
             |> Enum.join(" ")
    
    IO.write(output <> " \n")
    {nil_value(), final_state}
  end

  defp evaluate_operator_function(operator, arguments, state) do
    case length(arguments) do
      2 ->
        [left_arg, right_arg] = arguments
        {left, state1} = evaluate_expression(left_arg, state)
        {right, state2} = evaluate_expression(right_arg, state1)
        
        result = case operator do
          "+" -> add(left, right)
          "-" -> subtract(left, right)
          "*" -> multiply(left, right)
          "/" -> divide(left, right)
        end
        
        {result, state2}
      
      1 ->
        # Partial application
        {args, final_state} = evaluate_arguments(arguments, state, [])
        partial_fn = %{type: :partial_function, name: operator, args: args, arity: 2}
        {partial_fn, final_state}
      
      _ ->
        raise "Operator #{operator} requires exactly 1 or 2 arguments, got #{length(arguments)}"
    end
  end

  # Built-in function: push(item, collection)
  defp evaluate_push(arguments, state) do
    case length(arguments) do
      2 ->
        [item_arg, collection_arg] = arguments
        {item, state1} = evaluate_expression(item_arg, state)
        {collection, state2} = evaluate_expression(collection_arg, state1)
        
        result = case collection do
          %{type: :list, items: items} ->
            %{type: :list, items: items ++ [item]}
          %{type: :set, items: items} ->
            # Sets avoid duplicates and maintain sorted order
            new_items = if item in items do
              items
            else
              (items ++ [item]) |> Enum.sort()
            end
            %{type: :set, items: new_items}
          _ ->
            raise "push can only be used with lists and sets"
        end
        
        {result, state2}
      
      1 ->
        # Partial application
        {args, final_state} = evaluate_arguments(arguments, state, [])
        partial_fn = %{type: :partial_function, name: "push", args: args, arity: 2}
        {partial_fn, final_state}
      
      _ ->
        raise "push requires exactly 1 or 2 arguments: item [and collection]"
    end
  end

  # Built-in function: first(collection)
  defp evaluate_first([collection_arg], state) do
    {collection, final_state} = evaluate_expression(collection_arg, state)
    
    result = case collection do
      %{type: :list, items: []} -> nil_value()
      %{type: :list, items: [first | _]} -> first
      %{type: :set, items: []} -> nil_value()
      %{type: :set, items: [first | _]} -> first
      value when is_binary(value) ->
        if String.length(value) > 0 do
          String.first(value)
        else
          nil_value()
        end
      _ ->
        raise "first can only be used with lists, sets, and strings"
    end
    
    {result, final_state}
  end
  
  defp evaluate_first(_, _), do: raise "first requires exactly 1 argument"

  # Built-in function: rest(collection)
  defp evaluate_rest([collection_arg], state) do
    {collection, final_state} = evaluate_expression(collection_arg, state)
    
    result = case collection do
      %{type: :list, items: []} -> %{type: :list, items: []}
      %{type: :list, items: [_ | rest]} -> %{type: :list, items: rest}
      %{type: :set, items: []} -> %{type: :set, items: []}
      %{type: :set, items: [_ | rest]} -> %{type: :set, items: rest}
      value when is_binary(value) ->
        if String.length(value) > 0 do
          String.slice(value, 1..-1//1)
        else
          ""
        end
      _ ->
        raise "rest can only be used with lists, sets, and strings"
    end
    
    {result, final_state}
  end
  
  defp evaluate_rest(_, _), do: raise "rest requires exactly 1 argument"

  # Built-in function: size(collection)
  defp evaluate_size([collection_arg], state) do
    {collection, final_state} = evaluate_expression(collection_arg, state)
    
    result = case collection do
      %{type: :list, items: items} -> length(items)
      %{type: :set, items: items} -> length(items)
      %{type: :dict, entries: entries} -> map_size(entries)
      value when is_binary(value) -> byte_size(value)
      _ ->
        raise "size can only be used with lists, sets, dicts, and strings"
    end
    
    {result, final_state}
  end
  
  defp evaluate_size(_, _), do: raise "size requires exactly 1 argument"

  # Built-in function: assoc(key, value, dict)
  defp evaluate_assoc(arguments, state) do
    case length(arguments) do
      3 ->
        [key_arg, value_arg, dict_arg] = arguments
        {key, state1} = evaluate_expression(key_arg, state)
        {value, state2} = evaluate_expression(value_arg, state1)
        {dict, state3} = evaluate_expression(dict_arg, state2)
        
        # Check if key is a dictionary - not allowed as dictionary keys
        if match?(%{type: :dict}, key) do
          raise "Unable to use a Dictionary as a Dictionary key"
        end
        
        result = case dict do
          %{type: :dict, entries: entries} ->
            new_entries = Map.put(entries, key, value)
            %{type: :dict, entries: new_entries}
          _ ->
            raise "assoc can only be used with dictionaries"
        end
        
        {result, state3}
      
      1 ->
        # Partial application: assoc(key) -> function that takes value and dict
        {args, final_state} = evaluate_arguments(arguments, state, [])
        partial_fn = %{type: :partial_function, name: "assoc", args: args, arity: 3}
        {partial_fn, final_state}
      
      2 ->
        # Partial application: assoc(key, value) -> function that takes dict
        {args, final_state} = evaluate_arguments(arguments, state, [])
        partial_fn = %{type: :partial_function, name: "assoc", args: args, arity: 3}
        {partial_fn, final_state}
      
      _ ->
        raise "assoc requires 1, 2, or 3 arguments"
    end
  end

  # Built-in function: map(fn, list)
  defp evaluate_map([fn_arg, list_arg], state) do
    {fn_value, state1} = evaluate_expression(fn_arg, state)
    {list_value, state2} = evaluate_expression(list_arg, state1)
    
    # Validate arguments
    fn_type = get_type_name(fn_value)
    list_type = get_type_name(list_value)
    
    cond do
      not is_function_value(fn_value) ->
        raise "Unexpected argument: map(#{fn_type}, #{list_type})"
      not match?(%{type: :list}, list_value) ->
        raise "Unexpected argument: map(#{fn_type}, #{list_type})"
      true ->
        # Valid arguments, proceed with mapping
        mapped_items = Enum.map(list_value.items, fn item ->
          # Apply function to item
          apply_function(fn_value, [item], state2) |> elem(0)
        end)
        result = %{type: :list, items: mapped_items}
        {result, state2}
    end
  end
  
  defp evaluate_map([fn_arg], state) do
    # Partial application: return a function that takes a list
    {fn_value, final_state} = evaluate_expression(fn_arg, state)
    partial_fn = %{type: :partial_function, name: "map", args: [fn_value], arity: 2}
    {partial_fn, final_state}
  end
  
  defp evaluate_map(_, _), do: raise "map requires 1 or 2 arguments: function [, list]"

  # Built-in function: filter(fn, list)
  defp evaluate_filter([fn_arg, list_arg], state) do
    {fn_value, state1} = evaluate_expression(fn_arg, state)
    {list_value, state2} = evaluate_expression(list_arg, state1)
    
    # Validate arguments
    fn_type = get_type_name(fn_value)
    list_type = get_type_name(list_value)
    
    cond do
      not is_function_value(fn_value) ->
        raise "Unexpected argument: filter(#{fn_type}, #{list_type})"
      not match?(%{type: :list}, list_value) ->
        raise "Unexpected argument: filter(#{fn_type}, #{list_type})"
      true ->
        # Valid arguments, proceed with filtering
        filtered_items = Enum.filter(list_value.items, fn item ->
          # Apply function to item and check if result is truthy
          result = apply_function(fn_value, [item], state2) |> elem(0)
          is_truthy(result)
        end)
        result = %{type: :list, items: filtered_items}
        {result, state2}
    end
  end
  
  defp evaluate_filter([fn_arg], state) do
    # Partial application: return a function that takes a list
    {fn_value, final_state} = evaluate_expression(fn_arg, state)
    partial_fn = %{type: :partial_function, name: "filter", args: [fn_value], arity: 2}
    {partial_fn, final_state}
  end
  
  defp evaluate_filter(_, _), do: raise "filter requires 1 or 2 arguments: function [, list]"

  # Built-in function: fold(init, fn, list)
  defp evaluate_fold([init_arg, fn_arg, list_arg], state) do
    {init_value, state1} = evaluate_expression(init_arg, state)
    {fn_value, state2} = evaluate_expression(fn_arg, state1)
    {list_value, state3} = evaluate_expression(list_arg, state2)
    
    # Validate arguments
    init_type = get_type_name(init_value)
    fn_type = get_type_name(fn_value)
    list_type = get_type_name(list_value)
    
    cond do
      not is_function_value(fn_value) ->
        raise "Unexpected argument: fold(#{init_type}, #{fn_type}, #{list_type})"
      not match?(%{type: :list}, list_value) ->
        raise "Unexpected argument: fold(#{init_type}, #{fn_type}, #{list_type})"
      true ->
        # Valid arguments, proceed with folding
        result = Enum.reduce(list_value.items, init_value, fn item, acc ->
          # Apply function to (acc, item)
          apply_function(fn_value, [acc, item], state3) |> elem(0)
        end)
        {result, state3}
    end
  end
  
  defp evaluate_fold([init_arg, fn_arg], state) do
    # Partial application: return a function that takes a list
    {init_value, state1} = evaluate_expression(init_arg, state)
    {fn_value, state2} = evaluate_expression(fn_arg, state1)
    partial_fn = %{type: :partial_function, name: "fold", args: [init_value, fn_value], arity: 3}
    {partial_fn, state2}
  end
  
  defp evaluate_fold(_, _), do: raise "fold requires 2 or 3 arguments: init, function [, list]"

  # Apply a function value to arguments
  defp apply_function(%{type: :function, params: params, body: body, env: closure_env} = func, args, state) do
    cond do
      length(args) == length(params) ->
        # All parameters provided, execute function
        param_bindings = Enum.zip(params, args) |> Enum.into(%{})
        new_env = Map.merge(closure_env, state.env) |> Map.merge(param_bindings)
        # Use closure's mutable information if available, otherwise use current state
        closure_mutable = Map.get(func, :mutable, state.mutable)
        new_state = %{state | env: new_env, mutable: closure_mutable}
        {result, final_state} = evaluate_expression(body, new_state)
        # Merge closure_refs from function execution back to caller state
        merged_state = %{state | closure_refs: final_state.closure_refs, next_closure_id: final_state.next_closure_id}
        {result, merged_state}
      
      length(args) < length(params) ->
        # Partial application: create a new function with some parameters bound
        remaining_params = Enum.drop(params, length(args))
        param_bindings = Enum.zip(params, args) |> Enum.into(%{})
        new_closure_env = Map.merge(closure_env, param_bindings)
        
        partial_function = %{
          type: :function,
          params: remaining_params,
          body: body,
          env: new_closure_env,
          mutable: Map.get(func, :mutable, state.mutable)
        }
        
        {partial_function, state}
      
      true ->
        raise "Too many arguments provided to function: expected #{length(params)}, got #{length(args)}"
    end
  end
  
  defp apply_function(operator, args, state) when is_binary(operator) do
    # Handle operator functions like "+", "*", etc.
    case {operator, length(args)} do
      {"+", 2} -> {add(Enum.at(args, 0), Enum.at(args, 1)), state}
      {"-", 2} -> {subtract(Enum.at(args, 0), Enum.at(args, 1)), state}
      {"*", 2} -> {multiply(Enum.at(args, 0), Enum.at(args, 1)), state}
      {"/", 2} -> {divide(Enum.at(args, 0), Enum.at(args, 1)), state}
      {"+", 1} -> 
        partial_fn = %{type: :partial_function, name: "+", args: args, arity: 2}
        {partial_fn, state}
      {"-", 1} -> 
        partial_fn = %{type: :partial_function, name: "-", args: args, arity: 2}
        {partial_fn, state}
      {"*", 1} -> 
        partial_fn = %{type: :partial_function, name: "*", args: args, arity: 2}
        {partial_fn, state}
      {"/", 1} -> 
        partial_fn = %{type: :partial_function, name: "/", args: args, arity: 2}
        {partial_fn, state}
      {"size", 1} ->
        [collection] = args
        result = case collection do
          %{type: :list, items: items} -> length(items)
          %{type: :set, items: items} -> length(items)
          %{type: :dict, entries: entries} -> map_size(entries)
          value when is_binary(value) -> byte_size(value)
          _ -> raise "size can only be used with lists, sets, dicts, and strings"
        end
        {result, state}
      {"push", 2} ->
        [item, collection] = args
        result = case collection do
          %{type: :list, items: items} ->
            %{type: :list, items: items ++ [item]}
          %{type: :set, items: items} ->
            new_items = if item in items do
              items
            else
              (items ++ [item]) |> Enum.sort()
            end
            %{type: :set, items: new_items}
          _ ->
            raise "push can only be used with lists and sets"
        end
        {result, state}
      {"push", 1} -> 
        partial_fn = %{type: :partial_function, name: "push", args: args, arity: 2}
        {partial_fn, state}
      {"first", 1} ->
        [collection] = args
        result = case collection do
          %{type: :list, items: []} -> :elf_nil
          %{type: :list, items: [first | _]} -> first
          %{type: :set, items: []} -> :elf_nil
          %{type: :set, items: [first | _]} -> first
          value when is_binary(value) ->
            if String.length(value) > 0 do
              String.first(value)
            else
              :elf_nil
            end
          _ ->
            raise "first can only be used with lists, sets, and strings"
        end
        {result, state}
      {"rest", 1} ->
        [collection] = args
        result = case collection do
          %{type: :list, items: []} -> %{type: :list, items: []}
          %{type: :list, items: [_ | rest]} -> %{type: :list, items: rest}
          %{type: :set, items: []} -> %{type: :set, items: []}
          %{type: :set, items: [_ | rest]} -> %{type: :set, items: rest}
          value when is_binary(value) ->
            if String.length(value) > 0 do
              String.slice(value, 1..-1//1)
            else
              ""
            end
          _ ->
            raise "rest can only be used with lists, sets, and strings"
        end
        {result, state}
      {"assoc", 3} ->
        [key, value, dict] = args
        if match?(%{type: :dict}, key) do
          raise "Unable to use a Dictionary as a Dictionary key"
        end
        result = case dict do
          %{type: :dict, entries: entries} ->
            new_entries = Map.put(entries, key, value)
            %{type: :dict, entries: new_entries}
          _ ->
            raise "assoc can only be used with dictionaries"
        end
        {result, state}
      {"assoc", 1} -> 
        partial_fn = %{type: :partial_function, name: "assoc", args: args, arity: 3}
        {partial_fn, state}
      {"assoc", 2} -> 
        partial_fn = %{type: :partial_function, name: "assoc", args: args, arity: 3}
        {partial_fn, state}
      {"map", 2} ->
        [fn_value, list_value] = args
        result = case list_value do
          %{type: :list, items: items} ->
            mapped_items = Enum.map(items, fn item ->
              apply_function(fn_value, [item], state) |> elem(0)
            end)
            %{type: :list, items: mapped_items}
          _ ->
            raise "map can only be used with lists"
        end
        {result, state}
      {"map", 1} -> 
        partial_fn = %{type: :partial_function, name: "map", args: args, arity: 2}
        {partial_fn, state}
      {"filter", 2} ->
        [fn_value, list_value] = args
        result = case list_value do
          %{type: :list, items: items} ->
            filtered_items = Enum.filter(items, fn item ->
              result = apply_function(fn_value, [item], state) |> elem(0)
              is_truthy(result)
            end)
            %{type: :list, items: filtered_items}
          _ ->
            raise "filter can only be used with lists"
        end
        {result, state}
      {"filter", 1} -> 
        partial_fn = %{type: :partial_function, name: "filter", args: args, arity: 2}
        {partial_fn, state}
      {"fold", 3} ->
        [initial_value, fn_value, list_value] = args
        result = case list_value do
          %{type: :list, items: items} ->
            Enum.reduce(items, initial_value, fn item, acc ->
              apply_function(fn_value, [acc, item], state) |> elem(0)
            end)
          _ ->
            raise "fold can only be used with lists"
        end
        {result, state}
      {"fold", 1} -> 
        partial_fn = %{type: :partial_function, name: "fold", args: args, arity: 3}
        {partial_fn, state}
      {"fold", 2} -> 
        partial_fn = %{type: :partial_function, name: "fold", args: args, arity: 3}
        {partial_fn, state}
      _ -> raise "Unknown function: #{operator}"
    end
  end
  
  defp apply_function(%{type: :partial_function} = partial, args, state) do
    # Handle partial function application
    total_args = partial.args ++ args
    
    cond do
      length(total_args) >= partial.arity ->
        # We have enough arguments, apply the function directly
        case partial.name do
          "map" -> 
            [fn_value, list_value] = Enum.take(total_args, 2)
            result = case list_value do
              %{type: :list, items: items} ->
                mapped_items = Enum.map(items, fn item ->
                  apply_function(fn_value, [item], state) |> elem(0)
                end)
                %{type: :list, items: mapped_items}
              _ ->
                raise "map can only be used with lists"
            end
            {result, state}
          "filter" -> 
            [fn_value, list_value] = Enum.take(total_args, 2)
            result = case list_value do
              %{type: :list, items: items} ->
                filtered_items = Enum.filter(items, fn item ->
                  result = apply_function(fn_value, [item], state) |> elem(0)
                  is_truthy(result)
                end)
                %{type: :list, items: filtered_items}
              _ ->
                raise "filter can only be used with lists"
            end
            {result, state}
          "fold" -> 
            [initial_value, fn_value, list_value] = Enum.take(total_args, 3)
            result = case list_value do
              %{type: :list, items: items} ->
                Enum.reduce(items, initial_value, fn item, acc ->
                  apply_function(fn_value, [acc, item], state) |> elem(0)
                end)
              _ ->
                raise "fold can only be used with lists"
            end
            {result, state}
          "+" ->
            [arg1, arg2] = Enum.take(total_args, 2)
            {add(arg1, arg2), state}
          "-" ->
            [arg1, arg2] = Enum.take(total_args, 2)
            {subtract(arg1, arg2), state}
          "*" ->
            [arg1, arg2] = Enum.take(total_args, 2)
            {multiply(arg1, arg2), state}
          "/" ->
            [arg1, arg2] = Enum.take(total_args, 2)
            {divide(arg1, arg2), state}
          "push" ->
            [item, list] = Enum.take(total_args, 2)
            result = case list do
              %{type: :list, items: items} ->
                %{type: :list, items: items ++ [item]}
              _ ->
                raise "push can only be used with lists"
            end
            {result, state}
          "assoc" ->
            [key, value, dict] = Enum.take(total_args, 3)
            if match?(%{type: :dict}, key) do
              raise "Unable to use a Dictionary as a Dictionary key"
            end
            result = case dict do
              %{type: :dict, entries: entries} ->
                new_entries = Map.put(entries, key, value)
                %{type: :dict, entries: new_entries}
              _ ->
                raise "assoc can only be used with dictionaries"
            end
            {result, state}
          _ -> raise "Unknown partial function: #{partial.name}"
        end
      true ->
        # Still need more arguments, return updated partial function
        updated_partial = %{partial | args: total_args}
        {updated_partial, state}
    end
  end

  defp apply_function(%{type: :composed_function, left: left_fn, right: right_fn}, args, state) do
    # Apply left function first, then right function to the result
    {intermediate_result, state1} = apply_function(left_fn, args, state)
    apply_function(right_fn, [intermediate_result], state1)
  end
  
  # Apply stateful function
  defp apply_function(%{type: :stateful_function, params: params, body: body, closure_id: closure_id}, args, state) do
    cond do
      length(args) == length(params) ->
        # Get the current closure state
        closure_state = Map.get(state.closure_refs, closure_id)
        if closure_state == nil do
          raise "Invalid closure reference: #{closure_id}"
        end
        
        # All parameters provided, execute function
        param_bindings = Enum.zip(params, args) |> Enum.into(%{})
        new_env = Map.merge(closure_state.env, param_bindings)
        new_state = %{state | env: new_env, mutable: closure_state.mutable}
        {result, final_state} = evaluate_expression(body, new_state)
        
        # Update the closure state with any changes to mutable variables
        updated_closure_state = %{closure_state | env: final_state.env}
        # Also update global environment with changes to global mutable variables
        global_env_updates = Map.take(final_state.env, Map.keys(state.env))
        updated_global_env = Map.merge(state.env, global_env_updates)
        # Merge existing closures with new closures created during execution, then update this closure
        merged_closure_refs = Map.merge(state.closure_refs, final_state.closure_refs)
        updated_closure_refs = Map.put(merged_closure_refs, closure_id, updated_closure_state)
        final_global_state = %{state | env: updated_global_env, closure_refs: updated_closure_refs, next_closure_id: final_state.next_closure_id}
        
        {result, final_global_state}
      
      length(args) < length(params) ->
        # Partial application for stateful functions
        remaining_params = Enum.drop(params, length(args))
        param_bindings = Enum.zip(params, args) |> Enum.into(%{})
        
        # Update closure state with bound parameters
        closure_state = Map.get(state.closure_refs, closure_id)
        new_closure_env = Map.merge(closure_state.env, param_bindings)
        updated_closure_state = %{closure_state | env: new_closure_env}
        updated_closure_refs = Map.put(state.closure_refs, closure_id, updated_closure_state)
        
        partial_function = %{
          type: :stateful_function,
          params: remaining_params,
          body: body,
          closure_id: closure_id
        }
        
        new_state = %{state | closure_refs: updated_closure_refs}
        {partial_function, new_state}
      
      true ->
        raise "Too many arguments provided to function: expected #{length(params)}, got #{length(args)}"
    end
  end

  defp apply_function(other, _args, _state) do
    raise "Cannot call non-function value: #{inspect(other)}"
  end

  # Evaluate function literals (lambdas) - creates closures
  defp evaluate_function_literal(expr, state) do
    # Extract parameter names from identifiers
    param_names = Enum.map(expr["parameters"], fn param -> param["name"] end)
    
    # Check if this function captures mutable variables from outer scopes
    # We need to check if any mutable variables are referenced in the function body
    has_mutable_captures = captures_mutable_vars(expr["body"], state)
    
    if has_mutable_captures do
      # Create a stateful closure with a unique ID
      closure_id = state.next_closure_id
      closure_state = %{env: state.env, mutable: state.mutable}
      new_closure_refs = Map.put(state.closure_refs, closure_id, closure_state)
      
      function_value = %{
        type: :stateful_function,
        params: param_names,
        body: expr["body"],
        closure_id: closure_id
      }
      
      new_state = %{state | closure_refs: new_closure_refs, next_closure_id: closure_id + 1}
      {function_value, new_state}
    else
      # Regular function without mutable captures
      function_value = %{
        type: :function,
        params: param_names,
        body: expr["body"],
        env: state.env,  # Capture current environment as closure
        mutable: state.mutable  # Capture current mutable variables
      }
      
      {function_value, state}
    end
  end

  # Evaluate blocks - like programs but return last expression result
  defp evaluate_block(expr, state) do
    statements = expr["statements"] || []
    evaluate_statements(statements, state, nil)
  end

  # Evaluate function composition (>>)
  defp evaluate_function_composition(expr, state) do
    functions = expr["functions"]
    
    # Evaluate all functions in the composition chain
    {evaluated_functions, final_state} = Enum.reduce(functions, {[], state}, fn func, {acc, current_state} ->
      {evaluated_func, new_state} = evaluate_expression(func, current_state)
      {[evaluated_func | acc], new_state}
    end)
    
    # Reverse to get original order and create composed function
    evaluated_functions = Enum.reverse(evaluated_functions)
    
    # Handle composition of any number of functions
    case evaluated_functions do
      [] ->
        raise "Empty function composition"
      [single_fn] ->
        {single_fn, final_state}
      [left_fn, right_fn] ->
        composed_function = %{
          type: :composed_function,
          left: left_fn,
          right: right_fn
        }
        {composed_function, final_state}
      functions ->
        # For multiple functions, compose them right-to-left: f >> g >> h becomes f >> (g >> h)
        [first | rest] = functions
        composed_function = Enum.reduce(rest, first, fn func, acc ->
          %{
            type: :composed_function,
            left: acc,
            right: func
          }
        end)
        {composed_function, final_state}
    end
  end

  # Evaluate function threading (|>)
  defp evaluate_function_thread(expr, state) do
    # Threading is left-associative: a |> f |> g becomes g(f(a))
    # We evaluate from left to right, applying each function to the result
    initial = expr["initial"]
    functions = expr["functions"]
    
    # Start with the initial value
    {result, current_state} = evaluate_expression(initial, state)
    
    # Apply each function in sequence
    Enum.reduce(functions, {result, current_state}, fn func, {current_result, current_state} ->
      {func_value, new_state} = evaluate_expression(func, current_state)
      apply_function(func_value, [current_result], new_state)
    end)
  end

  # Evaluate If expressions
  defp evaluate_if(expr, state) do
    {condition_result, state1} = evaluate_expression(expr["condition"], state)
    
    if is_truthy(condition_result) do
      evaluate_expression(expr["consequence"], state1)
    else
      evaluate_expression(expr["alternative"], state1)
    end
  end

  defp evaluate_arguments([], state, acc), do: {Enum.reverse(acc), state}
  
  defp evaluate_arguments([arg | rest], state, acc) do
    {value, new_state} = evaluate_expression(arg, state)
    evaluate_arguments(rest, new_state, [value | acc])
  end

  defp add(left, right) when is_number(left) and is_number(right), do: left + right
  defp add(left, right) when is_binary(left) and is_binary(right), do: left <> right
  defp add(left, right) when is_binary(left) and is_number(right), do: left <> to_string(right)
  defp add(left, right) when is_number(left) and is_binary(right), do: to_string(left) <> right
  
  # Collection operations
  defp add(%{type: :list, items: items1}, %{type: :list, items: items2}) do
    %{type: :list, items: items1 ++ items2}
  end
  defp add(%{type: :set, items: items1}, %{type: :set, items: items2}) do
    # Set union - combine items and remove duplicates, maintain sorted order
    combined_items = (items1 ++ items2) |> Enum.uniq() |> Enum.sort()
    %{type: :set, items: combined_items}
  end
  defp add(%{type: :dict, entries: entries1}, %{type: :dict, entries: entries2}) do
    # Right-biased merge (second dict wins for duplicate keys)
    merged_entries = Map.merge(entries1, entries2)
    %{type: :dict, entries: merged_entries}
  end
  
  # Error cases for unsupported operations
  defp add(%{type: :list}, right) when is_number(right), do: raise "Unsupported operation: List + Integer"
  defp add(%{type: :set}, right) when is_number(right), do: raise "Unsupported operation: Set + Integer" 
  defp add(left, right), do: raise "Invalid operands for addition: #{inspect(left)} + #{inspect(right)}"

  defp subtract(left, right) when is_number(left) and is_number(right), do: left - right  
  defp subtract(left, right), do: raise "Invalid operands for subtraction: #{inspect(left)} - #{inspect(right)}"

  defp multiply(left, right) when is_number(left) and is_number(right), do: left * right
  defp multiply(left, right) when is_binary(left) and is_integer(right) do
    if right >= 0 do
      String.duplicate(left, right)
    else
      raise "Unsupported operation: String * Integer (< 0)"
    end
  end
  defp multiply(left, right) when is_integer(left) and is_binary(right) do
    if left >= 0 do
      String.duplicate(right, left)
    else
      raise "Unsupported operation: String * Integer (< 0)"
    end
  end
  defp multiply(left, right) when is_binary(left) and is_float(right) do
    raise "Unsupported operation: String * Decimal"
  end
  defp multiply(left, right) when is_float(left) and is_binary(right) do
    raise "Unsupported operation: String * Decimal"
  end
  defp multiply(left, right), do: raise "Invalid operands for multiplication: #{inspect(left)} * #{inspect(right)}"

  defp logical_and(left, right) do
    is_truthy(left) && is_truthy(right)
  end

  defp logical_or(left, right) do
    is_truthy(left) || is_truthy(right)
  end

  defp is_truthy(:elf_nil), do: false
  defp is_truthy(false), do: false
  defp is_truthy(0), do: false
  defp is_truthy(0.0), do: false
  defp is_truthy(""), do: false
  defp is_truthy(%{type: :list, items: []}), do: false
  defp is_truthy(%{type: :set, items: []}), do: false
  defp is_truthy(%{type: :dict, entries: entries}) when map_size(entries) == 0, do: false
  defp is_truthy(_), do: true

  defp divide(_left, 0), do: raise "Division by zero"
  defp divide(_left, right) when is_float(right) and right == 0.0, do: raise "Division by zero"
  defp divide(left, right) when is_number(left) and is_number(right) do
    # Integer division truncates
    if is_integer(left) and is_integer(right) do
      div(left, right)
    else
      left / right
    end
  end
  defp divide(left, right), do: raise "Invalid operands for division: #{inspect(left)} / #{inspect(right)}"

  defp unary_minus(operand) when is_number(operand), do: -operand
  defp unary_minus(operand), do: raise "Invalid operand for unary minus: #{inspect(operand)}"

  defp unary_not(operand), do: !is_truthy(operand)

  defp evaluate_list(expr, state) do
    items = expr["items"] || []
    {evaluated_items, final_state} = evaluate_arguments(items, state, [])
    {%{type: :list, items: evaluated_items}, final_state}
  end

  defp evaluate_set(expr, state) do
    items = expr["items"] || []
    {evaluated_items, final_state} = evaluate_arguments(items, state, [])
    
    # Check if any items are dictionaries - not allowed in sets
    Enum.each(evaluated_items, fn item ->
      if match?(%{type: :dict}, item) do
        raise "Unable to include a Dictionary within a Set"
      end
    end)
    
    # Sets maintain uniqueness and are sorted
    unique_items = evaluated_items |> Enum.uniq() |> Enum.sort()
    {%{type: :set, items: unique_items}, final_state}
  end

  defp evaluate_dict(expr, state) do
    entries = expr["items"] || []
    {evaluated_entries, final_state} = evaluate_dict_entries(entries, state, [])
    # Convert to a map for easier access, maintaining order
    entry_map = Enum.into(evaluated_entries, %{})
    {%{type: :dict, entries: entry_map}, final_state}
  end

  defp evaluate_dict_entries([], state, acc), do: {Enum.reverse(acc), state}
  
  defp evaluate_dict_entries([entry | rest], state, acc) do
    {key, state1} = evaluate_expression(entry["key"], state)
    {value, state2} = evaluate_expression(entry["value"], state1)
    
    # Check if key is a dictionary - not allowed as dictionary keys
    if match?(%{type: :dict}, key) do
      raise "Unable to use a Dictionary as a Dictionary key"
    end
    
    evaluate_dict_entries(rest, state2, [{key, value} | acc])
  end

  defp evaluate_index(expr, state) do
    {collection, state1} = evaluate_expression(expr["left"], state)
    {index, state2} = evaluate_expression(expr["index"], state1)
    
    result = case collection do
      # String indexing
      value when is_binary(value) ->
        if is_integer(index) do
          str_length = String.length(value)
          actual_index = if index < 0, do: str_length + index, else: index
          
          if actual_index >= 0 and actual_index < str_length do
            String.at(value, actual_index)
          else
            nil_value()
          end
        else
          type_name = cond do
            is_float(index) -> "Decimal"
            is_boolean(index) -> "Boolean"
            true -> "#{inspect(index)}"
          end
          raise "Unable to perform index operation, found: String[#{type_name}]"
        end
      
      # List indexing
      %{type: :list, items: items} ->
        if is_integer(index) do
          list_length = length(items)
          actual_index = if index < 0, do: list_length + index, else: index
          
          if actual_index >= 0 and actual_index < list_length do
            Enum.at(items, actual_index)
          else
            nil_value()
          end
        else
          type_name = cond do
            is_float(index) -> "Decimal"
            is_boolean(index) -> "Boolean"
            true -> "#{inspect(index)}"
          end
          raise "Unable to perform index operation, found: List[#{type_name}]"
        end
      
      # Dictionary indexing
      %{type: :dict, entries: entries} ->
        Map.get(entries, index, nil_value())
      
      _ ->
        raise "Cannot index #{inspect(collection)}"
    end
    
    {result, state2}
  end

  # Represent nil as a specific atom to distinguish from Elixir's nil
  defp nil_value(), do: :elf_nil

  def format_value(:elf_nil), do: "nil"
  def format_value(value) when is_integer(value), do: Integer.to_string(value)
  def format_value(value) when is_float(value) do
    # If the float is a whole number, format it as integer
    if value == trunc(value) do
      Integer.to_string(trunc(value))
    else
      Float.to_string(value)
    end
  end
  def format_value(value) when is_binary(value), do: "\"#{value}\""
  def format_value(true), do: "true"
  def format_value(false), do: "false"
  
  # Format collections
  def format_value(%{type: :list, items: items}) do
    formatted_items = items |> Enum.map(&format_value/1) |> Enum.join(", ")
    "[#{formatted_items}]"
  end
  
  def format_value(%{type: :set, items: items}) do
    formatted_items = items |> Enum.map(&format_value/1) |> Enum.join(", ")
    "{#{formatted_items}}"
  end
  
  def format_value(%{type: :dict, entries: entries}) do
    # Format dictionary with keys sorted
    formatted_entries = entries
                       |> Enum.sort_by(fn {key, _} -> key end)
                       |> Enum.map(fn {key, value} -> "#{format_value(key)}: #{format_value(value)}" end)
                       |> Enum.join(", ")
    "#" <> "{" <> formatted_entries <> "}"
  end
  
  def format_value(value), do: inspect(value)

  # Comparison functions
  defp equal(left, right), do: structural_equal(left, right)
  defp greater_than(left, right) when is_number(left) and is_number(right), do: left > right
  defp greater_than(_left, _right), do: raise "Cannot compare non-numeric values"
  defp less_than(left, right) when is_number(left) and is_number(right), do: left < right  
  defp less_than(_left, _right), do: raise "Cannot compare non-numeric values"
  defp greater_than_or_equal(left, right) when is_number(left) and is_number(right), do: left >= right
  defp greater_than_or_equal(_left, _right), do: raise "Cannot compare non-numeric values"
  defp less_than_or_equal(left, right) when is_number(left) and is_number(right), do: left <= right
  defp less_than_or_equal(_left, _right), do: raise "Cannot compare non-numeric values"

  # Function composition: f >> g creates a function that applies f then g
  defp compose_functions(left_fn, right_fn) do
    %{
      type: :composed_function,
      left: left_fn,
      right: right_fn
    }
  end

  # Function threading: value |> function applies function to value
  defp thread_value(value, function, state) do
    # Apply the function to the value
    apply_function(function, [value], state) |> elem(0)
  end

  # Structural equality for elf-lang values
  defp structural_equal(left, right) when is_number(left) and is_number(right), do: left == right
  defp structural_equal(left, right) when is_binary(left) and is_binary(right), do: left == right  
  defp structural_equal(left, right) when is_boolean(left) and is_boolean(right), do: left == right
  defp structural_equal(:elf_nil, :elf_nil), do: true
  defp structural_equal(%{type: :list, items: items1}, %{type: :list, items: items2}) do
    length(items1) == length(items2) and Enum.all?(Enum.zip(items1, items2), fn {a, b} -> structural_equal(a, b) end)
  end
  defp structural_equal(%{type: :set, items: items1}, %{type: :set, items: items2}) do
    MapSet.new(items1, fn x -> to_comparable(x) end) == MapSet.new(items2, fn x -> to_comparable(x) end)
  end
  defp structural_equal(%{type: :dict, entries: entries1}, %{type: :dict, entries: entries2}) do
    map_size(entries1) == map_size(entries2) and 
    Enum.all?(entries1, fn {k, v} -> 
      case Map.get(entries2, k) do
        nil -> false
        other_v -> structural_equal(v, other_v)
      end
    end)
  end
  defp structural_equal(_left, _right), do: false

  # Convert values to comparable forms for set operations
  defp to_comparable(value) when is_number(value) or is_binary(value) or is_boolean(value), do: value
  defp to_comparable(nil), do: :nil
  defp to_comparable(%{type: :list, items: items}), do: {:list, Enum.map(items, &to_comparable/1)}
  defp to_comparable(%{type: :set, items: items}), do: {:set, Enum.map(items, &to_comparable/1) |> Enum.sort()}
  defp to_comparable(%{type: :dict}), do: raise "Cannot use dictionaries in sets"

  # Check if a value is callable as a function
  defp is_function_value(%{type: :function}), do: true
  defp is_function_value(%{type: :stateful_function}), do: true
  defp is_function_value(%{type: :partial_function}), do: true
  defp is_function_value(%{type: :composed_function}), do: true
  defp is_function_value(operator) when operator in ["+", "-", "*", "/", "size", "push", "first", "rest", "assoc", "map", "filter", "fold"], do: true
  defp is_function_value(_), do: false

  # Get the type name for error messages
  defp get_type_name(value) when is_integer(value), do: "Integer"
  defp get_type_name(value) when is_float(value), do: "Decimal"
  defp get_type_name(value) when is_binary(value) and value in ["+", "-", "*", "/", "size", "push", "first", "rest", "assoc", "map", "filter", "fold"], do: "Function"
  defp get_type_name(value) when is_binary(value), do: "String"
  defp get_type_name(true), do: "Boolean"
  defp get_type_name(false), do: "Boolean"
  defp get_type_name(:elf_nil), do: "Nil"
  defp get_type_name(%{type: :list}), do: "List"
  defp get_type_name(%{type: :set}), do: "Set"
  defp get_type_name(%{type: :dict}), do: "Dictionary"
  defp get_type_name(%{type: :function}), do: "Function"
  defp get_type_name(%{type: :stateful_function}), do: "Function"
  defp get_type_name(%{type: :partial_function}), do: "Function"
  defp get_type_name(%{type: :composed_function}), do: "Function"
  defp get_type_name(_), do: "Unknown"

  # Check if an AST node captures mutable variables
  defp captures_mutable_vars(ast_node, state) do
    case ast_node["type"] do
      "Identifier" ->
        var_name = ast_node["name"]
        MapSet.member?(state.mutable, var_name)
      
      "Assignment" ->
        var_name = ast_node["name"]["name"]
        # Check if this assignment is to a mutable variable
        MapSet.member?(state.mutable, var_name) or captures_mutable_vars(ast_node["value"], state)
      
      "Block" ->
        statements = ast_node["statements"] || []
        Enum.any?(statements, fn stmt -> captures_mutable_vars(stmt, state) end)
      
      "Expression" ->
        captures_mutable_vars(ast_node["value"], state)
      
      "Infix" ->
        captures_mutable_vars(ast_node["left"], state) or captures_mutable_vars(ast_node["right"], state)
      
      "Unary" ->
        captures_mutable_vars(ast_node["operand"], state)
      
      "FunctionCall" ->
        arguments = ast_node["arguments"] || []
        Enum.any?(arguments, fn arg -> captures_mutable_vars(arg, state) end)
      
      "Call" ->
        arguments = ast_node["arguments"] || []
        captures_mutable_vars(ast_node["function"], state) or 
        Enum.any?(arguments, fn arg -> captures_mutable_vars(arg, state) end)
      
      "List" ->
        items = ast_node["items"] || []
        Enum.any?(items, fn item -> captures_mutable_vars(item, state) end)
      
      "Set" ->
        items = ast_node["items"] || []
        Enum.any?(items, fn item -> captures_mutable_vars(item, state) end)
      
      "Dictionary" ->
        items = ast_node["items"] || []
        Enum.any?(items, fn entry -> 
          captures_mutable_vars(entry["key"], state) or captures_mutable_vars(entry["value"], state)
        end)
      
      "Function" ->
        # Nested functions - check their bodies
        captures_mutable_vars(ast_node["body"], state)
      
      "If" ->
        captures_mutable_vars(ast_node["condition"], state) or
        captures_mutable_vars(ast_node["consequence"], state) or
        captures_mutable_vars(ast_node["alternative"], state)
      
      # Literals don't capture variables
      "Integer" -> false
      "Decimal" -> false
      "String" -> false
      "Boolean" -> false
      "Nil" -> false
      
      _ -> false
    end
  end

end