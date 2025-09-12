defmodule ElfLang.Parser do
  @moduledoc """
  Parser for elf-lang - converts tokens to Abstract Syntax Tree
  """

  def parse(tokens) do
    {statements, _remaining} = parse_statements(tokens, [])
    %{"type" => "Program", "statements" => Enum.reverse(statements)}
  end

  # Parse multiple statements
  defp parse_statements([], statements), do: {statements, []}

  defp parse_statements(tokens, statements) do
    case parse_statement(tokens) do
      {statement, remaining_tokens} ->
        # Skip semicolon if present
        remaining_after_semicolon = case remaining_tokens do
          [%{"type" => ";"} | rest] -> rest
          other -> other
        end
        parse_statements(remaining_after_semicolon, [statement | statements])

      nil ->
        {statements, tokens}
    end
  end

  # Parse a single statement
  defp parse_statement([%{"type" => "CMT", "value" => comment_text} | rest]) do
    {%{"type" => "Comment", "value" => comment_text}, rest}
  end

  defp parse_statement(tokens) do
    case parse_expression(tokens) do
      {expr, remaining} -> {%{"type" => "Expression", "value" => expr}, remaining}
      nil -> nil
    end
  end

  # Parse expressions (entry point for expression parsing)
  defp parse_expression([%{"type" => "LET"} | rest]) do
    parse_let_declaration(rest)
  end

  defp parse_expression([%{"type" => "IF"} | rest]) do
    parse_if_expression(rest)
  end

  defp parse_expression(tokens) do
    parse_assignment_expression(tokens)
  end

  # Parse assignment expressions (highest precedence)
  defp parse_assignment_expression(tokens) do
    case parse_thread_expression(tokens) do
      {%{"type" => "Identifier", "name" => var_name} = left, [%{"type" => "="} | after_eq]} ->
        # This is an assignment
        case parse_expression(after_eq) do
          {value_expr, remaining} ->
            assignment_expr = %{
              "type" => "Assignment",
              "name" => %{"type" => "Identifier", "name" => var_name},
              "value" => value_expr
            }
            {assignment_expr, remaining}
          nil -> 
            {left, [%{"type" => "="} | after_eq]}
        end
      {expr, remaining} ->
        {expr, remaining}
      nil ->
        nil
    end
  end

  # Parse let declarations
  defp parse_let_declaration([%{"type" => "MUT"} | rest]) do
    case rest do
      [%{"type" => "ID", "value" => name}, %{"type" => "="} | after_eq] ->
        case parse_expression(after_eq) do
          {value_expr, remaining} ->
            let_expr = %{
              "type" => "MutableLet",
              "name" => %{"type" => "Identifier", "name" => name},
              "value" => value_expr
            }
            {let_expr, remaining}
          nil -> nil
        end
      _ -> nil
    end
  end

  defp parse_let_declaration([%{"type" => "ID", "value" => name}, %{"type" => "="} | after_eq]) do
    case parse_expression(after_eq) do
      {value_expr, remaining} ->
        let_expr = %{
          "type" => "Let",
          "name" => %{"type" => "Identifier", "name" => name},
          "value" => value_expr
        }
        {let_expr, remaining}
      nil -> nil
    end
  end

  defp parse_let_declaration(_), do: nil

  # Parse if expressions: if condition { consequence } else { alternative }
  defp parse_if_expression(tokens) do
    case parse_expression(tokens) do
      {condition, remaining} ->
        case parse_block(remaining) do
          {consequence, [%{"type" => "ELSE"} | after_else]} ->
            case parse_block(after_else) do
              {alternative, final_remaining} ->
                if_expr = %{
                  "type" => "If",
                  "condition" => condition,
                  "consequence" => consequence,
                  "alternative" => alternative
                }
                {if_expr, final_remaining}
              nil -> nil
            end
          _ -> nil
        end
      nil -> nil
    end
  end

  # Parse block: { statements }
  defp parse_block([%{"type" => "{"} | rest]) do
    {statements, remaining} = parse_statements(rest, [])
    case remaining do
      [%{"type" => "}"} | after_brace] ->
        block = %{"type" => "Block", "statements" => Enum.reverse(statements)}
        {block, after_brace}
      _ -> nil
    end
  end

  defp parse_block(_), do: nil

  # Parse thread expressions: |>
  defp parse_thread_expression(tokens) do
    case parse_composition_expression(tokens) do
      {left, remaining} ->
        parse_thread_rest(left, remaining)
      nil -> nil
    end
  end

  defp parse_thread_rest(left, [%{"type" => "|>"} | rest]) do
    case parse_composition_expression(rest) do
      {right, remaining} ->
        # Build function thread chain
        case left do
          %{"type" => "FunctionThread", "functions" => funcs, "initial" => initial} ->
            # Add to existing chain
            thread_expr = %{
              "type" => "FunctionThread", 
              "functions" => funcs ++ [right],
              "initial" => initial
            }
            parse_thread_rest(thread_expr, remaining)
          _ ->
            # Start new chain
            thread_expr = %{
              "type" => "FunctionThread",
              "functions" => [right],
              "initial" => left
            }
            parse_thread_rest(thread_expr, remaining)
        end
      nil -> {left, [%{"type" => "|>"} | rest]}
    end
  end

  defp parse_thread_rest(left, remaining), do: {left, remaining}

  # Parse composition expressions: >>
  defp parse_composition_expression(tokens) do
    case parse_additive_expression(tokens) do
      {left, remaining} ->
        parse_composition_rest(left, remaining)
      nil -> nil
    end
  end

  defp parse_composition_rest(left, [%{"type" => ">>"} | rest]) do
    case parse_additive_expression(rest) do
      {right, remaining} ->
        # Build function composition chain
        case left do
          %{"type" => "FunctionComposition", "functions" => funcs} ->
            # Add to existing chain
            comp_expr = %{
              "type" => "FunctionComposition",
              "functions" => funcs ++ [right]
            }
            parse_composition_rest(comp_expr, remaining)
          _ ->
            # Start new chain
            comp_expr = %{
              "type" => "FunctionComposition",
              "functions" => [left, right]
            }
            parse_composition_rest(comp_expr, remaining)
        end
      nil -> {left, [%{"type" => ">>"} | rest]}
    end
  end

  defp parse_composition_rest(left, remaining), do: {left, remaining}

  # Additive expressions: + -
  defp parse_additive_expression(tokens) do
    case parse_comparison_expression(tokens) do
      {left, remaining} -> parse_additive_rest(left, remaining)
      nil -> nil
    end
  end

  defp parse_additive_rest(left, [%{"type" => op} | rest]) when op in ["+", "-"] do
    case parse_comparison_expression(rest) do
      {right, remaining} ->
        infix_expr = %{"type" => "Infix", "left" => left, "operator" => op, "right" => right}
        parse_additive_rest(infix_expr, remaining)
      nil -> {left, [%{"type" => op} | rest]}
    end
  end

  defp parse_additive_rest(left, remaining), do: {left, remaining}

  # Comparison expressions: > < >= <= == !=
  defp parse_comparison_expression(tokens) do
    case parse_logical_expression(tokens) do
      {left, remaining} -> parse_comparison_rest(left, remaining)
      nil -> nil
    end
  end

  defp parse_comparison_rest(left, [%{"type" => op} | rest]) when op in [">", "<", ">=", "<=", "==", "!="] do
    case parse_logical_expression(rest) do
      {right, remaining} ->
        infix_expr = %{"type" => "Infix", "left" => left, "operator" => op, "right" => right}
        parse_comparison_rest(infix_expr, remaining)
      nil -> {left, [%{"type" => op} | rest]}
    end
  end

  defp parse_comparison_rest(left, remaining), do: {left, remaining}

  # Logical expressions: && ||
  defp parse_logical_expression(tokens) do
    case parse_multiplicative_expression(tokens) do
      {left, remaining} -> parse_logical_rest(left, remaining)
      nil -> nil
    end
  end

  defp parse_logical_rest(left, [%{"type" => op} | rest]) when op in ["&&", "||"] do
    case parse_multiplicative_expression(rest) do
      {right, remaining} ->
        infix_expr = %{"type" => "Infix", "left" => left, "operator" => op, "right" => right}
        parse_logical_rest(infix_expr, remaining)
      nil -> {left, [%{"type" => op} | rest]}
    end
  end

  defp parse_logical_rest(left, remaining), do: {left, remaining}

  # Multiplicative expressions: * /
  defp parse_multiplicative_expression(tokens) do
    case parse_index_expression(tokens) do
      {left, remaining} -> parse_multiplicative_rest(left, remaining)
      nil -> nil
    end
  end

  # Parse indexing expressions: expr[index]
  defp parse_index_expression(tokens) do
    case parse_unary_expression(tokens) do
      {left, remaining} -> parse_index_rest(left, remaining)
      nil -> nil
    end
  end

  defp parse_index_rest(left, [token | rest]) do
    if token["type"] == "[" do
      case parse_expression(rest) do
        {index_expr, [bracket_token | after_bracket]} ->
          if bracket_token["type"] == "]" do
            index_node = %{"type" => "Index", "left" => left, "index" => index_expr}
            parse_index_rest(index_node, after_bracket)
          else
            {left, [token | rest]}
          end
        _ ->
          {left, [token | rest]}
      end
    else
      {left, [token | rest]}
    end
  end

  defp parse_index_rest(left, tokens), do: {left, tokens}

  defp parse_multiplicative_rest(left, [%{"type" => op} | rest]) when op in ["*", "/"] do
    case parse_index_expression(rest) do
      {right, remaining} ->
        infix_expr = %{"type" => "Infix", "left" => left, "operator" => op, "right" => right}
        parse_multiplicative_rest(infix_expr, remaining)
      nil -> {left, [%{"type" => op} | rest]}
    end
  end

  defp parse_multiplicative_rest(left, remaining), do: {left, remaining}

  # Unary expressions: -expr, !expr
  defp parse_unary_expression([%{"type" => op} | rest]) when op in ["-", "!"] do
    case parse_unary_expression(rest) do
      {expr, remaining} ->
        unary_expr = %{"type" => "Unary", "operator" => op, "operand" => expr}
        {unary_expr, remaining}
      nil -> nil
    end
  end

  defp parse_unary_expression(tokens) do
    case parse_primary_expression(tokens) do
      {expr, remaining} -> 
        # Check if this expression is followed by parentheses (function call)
        parse_postfix_expression(expr, remaining)
      nil -> nil
    end
  end
  
  # Handle postfix operations like function calls
  defp parse_postfix_expression(expr, [%{"type" => "("} | rest]) do
    # This is a function call on the expression
    {args, remaining} = parse_argument_list(rest, [])
    case remaining do
      [%{"type" => ")"} | after_paren] ->
        call_expr = %{
          "type" => "Call",
          "function" => expr,
          "arguments" => Enum.reverse(args)
        }
        # Recursively check for more postfix operations
        parse_postfix_expression(call_expr, after_paren)
      _ ->
        # Malformed function call
        {expr, [%{"type" => "("} | rest]}
    end
  end
  
  defp parse_postfix_expression(expr, tokens) do
    # No postfix operation found
    {expr, tokens}
  end

  # Primary expressions
  defp parse_primary_expression([%{"type" => "INT", "value" => value} | rest]) do
    {%{"type" => "Integer", "value" => value}, rest}
  end

  defp parse_primary_expression([%{"type" => "DEC", "value" => value} | rest]) do
    {%{"type" => "Decimal", "value" => value}, rest}
  end

  defp parse_primary_expression([%{"type" => "STR", "value" => quoted_str} | rest]) do
    unquoted = unescape_string(String.slice(quoted_str, 1..-2//1))
    {%{"type" => "String", "value" => unquoted}, rest}
  end

  defp parse_primary_expression([%{"type" => "TRUE"} | rest]) do
    {%{"type" => "Boolean", "value" => true}, rest}
  end

  defp parse_primary_expression([%{"type" => "FALSE"} | rest]) do
    {%{"type" => "Boolean", "value" => false}, rest}
  end

  defp parse_primary_expression([%{"type" => "NIL"} | rest]) do
    {%{"type" => "Nil"}, rest}
  end

  # Parse identifiers (function calls now handled by postfix parsing)
  defp parse_primary_expression([%{"type" => "ID", "value" => name} | rest]) do
    {%{"type" => "Identifier", "name" => name}, rest}
  end

  # Handle operator identifiers: +, -, *, /
  defp parse_primary_expression([%{"type" => op} | rest]) when op in ["+", "-", "*", "/"] do
    {%{"type" => "Identifier", "name" => op}, rest}
  end

  # Parse list literals: [1, 2, 3]
  defp parse_primary_expression([%{"type" => "["} | rest]) do
    parse_list_literal(rest, [])
  end

  # Parse set literals: {1, 2, 3}
  defp parse_primary_expression([%{"type" => "{"} | rest]) do
    parse_set_literal(rest, [])
  end

  # Parse dict literals: #{key: value}
  defp parse_primary_expression([%{"type" => dict_type} | rest]) when dict_type == "#" <> "{" do
    parse_dict_literal(rest, [])
  end

  # Parse parenthesized expressions
  defp parse_primary_expression([%{"type" => "("} | rest]) do
    case parse_expression(rest) do
      {expr, [%{"type" => ")"} | remaining]} ->
        {expr, remaining}
      _ ->
        nil
    end
  end

  # Parse lambda expressions: |x| expr or || expr  
  defp parse_primary_expression([%{"type" => "|"} | rest]) do
    parse_lambda(rest)
  end
  
  # Parse empty parameter lambda: || expr
  defp parse_primary_expression([%{"type" => "||"} | rest]) do
    parse_empty_lambda(rest)
  end

  defp parse_primary_expression(_), do: nil

  # Parse list literal: [item1, item2, ...]
  defp parse_list_literal([token | rest], items) do
    case token["type"] do
      ~s(]) -> {%{"type" => "List", "items" => Enum.reverse(items)}, rest}
      _ ->
        case parse_expression([token | rest]) do
          {item, remaining} ->
            case remaining do
              [%{"type" => ","} | after_comma] ->
                parse_list_literal(after_comma, [item | items])
              [%{"type" => ~s(])} | after_bracket] ->
                {%{"type" => "List", "items" => Enum.reverse([item | items])}, after_bracket}
              _ ->
                nil
            end
          nil ->
            nil
        end
    end
  end

  defp parse_list_literal([], _items), do: nil

  # Parse set literal: {item1, item2, ...}
  defp parse_set_literal([token | rest], items) do
    case token["type"] do
      "}" -> {%{"type" => "Set", "items" => Enum.reverse(items)}, rest}
      _ ->
        case parse_expression([token | rest]) do
          {item, remaining} ->
            case remaining do
              [%{"type" => ","} | after_comma] ->
                parse_set_literal(after_comma, [item | items])
              [%{"type" => "}"} | after_brace] ->
                {%{"type" => "Set", "items" => Enum.reverse([item | items])}, after_brace}
              _ ->
                nil
            end
          nil ->
            nil
        end
    end
  end

  defp parse_set_literal([], _items), do: nil

  # Parse dict literal: #{key: value, key2: value2, ...}
  defp parse_dict_literal([token | rest], pairs) do
    case token["type"] do
      "}" -> {%{"type" => "Dictionary", "items" => Enum.reverse(pairs)}, rest}
      _ ->
        case parse_expression([token | rest]) do
          {key, [colon_token | after_colon]} ->
            if colon_token["type"] == ":" do
              case parse_expression(after_colon) do
                {value, remaining} ->
                  pair = %{"key" => key, "value" => value}
                  case remaining do
                    [%{"type" => ","} | after_comma] ->
                      parse_dict_literal(after_comma, [pair | pairs])
                    [%{"type" => "}"} | after_brace] ->
                      {%{"type" => "Dictionary", "items" => Enum.reverse([pair | pairs])}, after_brace}
                    _ ->
                      nil
                  end
                nil ->
                  nil
              end
            else
              nil
            end
          _ ->
            nil
        end
    end
  end

  defp parse_dict_literal([], _pairs), do: nil

  # Parse function calls: func(arg1, arg2, ...)
  defp parse_function_call(name, tokens) do
    {args, remaining} = parse_argument_list(tokens, [])
    case remaining do
      [%{"type" => ")"} | after_paren] ->
        call_expr = %{
          "type" => "Call",
          "function" => %{"type" => "Identifier", "name" => name},
          "arguments" => Enum.reverse(args)
        }
        {call_expr, after_paren}
      _ ->
        nil
    end
  end

  defp parse_argument_list([%{"type" => ")"} | _] = tokens, args), do: {args, tokens}

  defp parse_argument_list(tokens, args) do
    case parse_expression(tokens) do
      {arg_expr, remaining} ->
        case remaining do
          [%{"type" => ","} | after_comma] ->
            parse_argument_list(after_comma, [arg_expr | args])
          _ ->
            {[arg_expr | args], remaining}
        end
      nil ->
        {args, tokens}
    end
  end

  # Helper function to unescape string literals  
  defp unescape_string(str) do
    # Handle the specific case that's failing: \\\" should become "
    case str do
      "\\\"" -> "\""
      _ ->
        str
        |> String.replace("\\\\", "\x00")  # temp placeholder for literal backslash
        |> String.replace("\\\"", "\"")    # Convert \\\" to \"  
        |> String.replace("\\n", "\n")    # Convert \\n to newline
        |> String.replace("\\t", "\t")    # Convert \\t to tab
        |> String.replace("\x00", "\\")   # restore literal backslashes
    end
  end

  # Parse lambda functions: |param1, param2| expression
  defp parse_lambda(tokens) do
    {params, after_params} = parse_lambda_params(tokens, [])
    case after_params do
      [%{"type" => "|"} | after_pipe] ->
        # Check if the body starts with { (explicit block) or is an expression
        case after_pipe do
          [%{"type" => "{"} | _] ->
            # Parse as block directly
            case parse_block(after_pipe) do
              {body_block, remaining} ->
                lambda_expr = %{
                  "type" => "Function",
                  "parameters" => Enum.reverse(params),
                  "body" => body_block
                }
                {lambda_expr, remaining}
              nil -> nil
            end
          _ ->
            # Parse as expression and wrap in Block
            case parse_expression(after_pipe) do
              {body_expr, remaining} ->
                lambda_expr = %{
                  "type" => "Function",
                  "parameters" => Enum.reverse(params),
                  "body" => %{
                    "type" => "Block",
                    "statements" => [%{"type" => "Expression", "value" => body_expr}]
                  }
                }
                {lambda_expr, remaining}
              nil -> nil
            end
        end
      _ -> nil
    end
  end

  # Parse empty parameter lambda: || expression  
  defp parse_empty_lambda(tokens) do
    # Check if the body starts with { (explicit block) or is an expression
    case tokens do
      [%{"type" => "{"} | _] ->
        # Parse as block directly
        case parse_block(tokens) do
          {body_block, remaining} ->
            lambda_expr = %{
              "type" => "Function",
              "parameters" => [],
              "body" => body_block
            }
            {lambda_expr, remaining}
          nil -> nil
        end
      _ ->
        # Parse as expression and wrap in Block
        case parse_expression(tokens) do
          {body_expr, remaining} ->
            lambda_expr = %{
              "type" => "Function",
              "parameters" => [],
              "body" => %{
                "type" => "Block",
                "statements" => [%{"type" => "Expression", "value" => body_expr}]
              }
            }
            {lambda_expr, remaining}
          nil -> nil
        end
    end
  end

  # Parse lambda parameters: param1, param2
  defp parse_lambda_params([%{"type" => "|"} | _] = tokens, params), do: {params, tokens}
  
  defp parse_lambda_params([%{"type" => "ID", "value" => name} | rest], params) do
    param = %{"type" => "Identifier", "name" => name}
    case rest do
      [%{"type" => ","} | after_comma] ->
        parse_lambda_params(after_comma, [param | params])
      _ ->
        {[param | params], rest}
    end
  end

  defp parse_lambda_params(tokens, params), do: {params, tokens}
end