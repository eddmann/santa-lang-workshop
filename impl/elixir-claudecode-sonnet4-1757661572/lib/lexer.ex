defmodule ElfLang.Lexer do
  @moduledoc """
  Tokenizer for elf-lang
  """

  def tokenize(source) do
    source
    |> String.graphemes()
    |> scan_tokens([])
    |> Enum.reverse()
  end

  defp scan_tokens([], tokens), do: tokens

  defp scan_tokens([" " | rest], tokens), do: scan_tokens(rest, tokens)
  defp scan_tokens(["\t" | rest], tokens), do: scan_tokens(rest, tokens)
  defp scan_tokens(["\n" | rest], tokens), do: scan_tokens(rest, tokens)
  defp scan_tokens(["\r" | rest], tokens), do: scan_tokens(rest, tokens)

  # Simple tokenization for basic tokens
  defp scan_tokens(["+" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "+", "value" => "+"} | tokens])
  end

  defp scan_tokens(["-" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "-", "value" => "-"} | tokens])
  end

  defp scan_tokens(["*" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "*", "value" => "*"} | tokens])
  end

  defp scan_tokens(["/" | rest], tokens) do
    case rest do
      ["/" | _] ->
        # Comment - scan to end of line
        {comment, remaining} = scan_comment(["/" | rest])
        scan_tokens(remaining, [comment | tokens])

      _ ->
        scan_tokens(rest, [%{"type" => "/", "value" => "/"} | tokens])
    end
  end

  defp scan_tokens(["=" | rest], tokens) do
    case rest do
      ["=" | rest2] ->
        scan_tokens(rest2, [%{"type" => "==", "value" => "=="} | tokens])

      _ ->
        scan_tokens(rest, [%{"type" => "=", "value" => "="} | tokens])
    end
  end

  defp scan_tokens(["!" | rest], tokens) do
    case rest do
      ["=" | rest2] ->
        scan_tokens(rest2, [%{"type" => "!=", "value" => "!="} | tokens])

      _ ->
        raise "Unexpected character: !"
    end
  end

  defp scan_tokens([">" | rest], tokens) do
    case rest do
      ["=" | rest2] ->
        scan_tokens(rest2, [%{"type" => ">=", "value" => ">="} | tokens])

      [">" | rest2] ->
        scan_tokens(rest2, [%{"type" => ">>", "value" => ">>"} | tokens])

      _ ->
        scan_tokens(rest, [%{"type" => ">", "value" => ">"} | tokens])
    end
  end

  defp scan_tokens(["<" | rest], tokens) do
    case rest do
      ["=" | rest2] ->
        scan_tokens(rest2, [%{"type" => "<=", "value" => "<="} | tokens])

      _ ->
        scan_tokens(rest, [%{"type" => "<", "value" => "<"} | tokens])
    end
  end

  defp scan_tokens(["&" | rest], tokens) do
    case rest do
      ["&" | rest2] ->
        scan_tokens(rest2, [%{"type" => "&&", "value" => "&&"} | tokens])

      _ ->
        raise "Unexpected character: &"
    end
  end

  defp scan_tokens(["|" | rest], tokens) do
    case rest do
      ["|" | rest2] ->
        scan_tokens(rest2, [%{"type" => "||", "value" => "||"} | tokens])

      [">" | rest2] ->
        scan_tokens(rest2, [%{"type" => "|>", "value" => "|>"} | tokens])

      _ ->
        # Single pipe for lambda parameters
        scan_tokens(rest, [%{"type" => "|", "value" => "|"} | tokens])
    end
  end

  defp scan_tokens(["#" | rest], tokens) do
    case rest do
      ["{" | rest2] ->
        # Use a safer way to construct the hash-brace token
        token = %{"type" => "#" <> "{", "value" => "#" <> "{"}
        scan_tokens(rest2, [token | tokens])

      _ ->
        raise "Unexpected character: #"
    end
  end

  defp scan_tokens(["{" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "{", "value" => "{"} | tokens])
  end

  defp scan_tokens(["}" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "}", "value" => "}"} | tokens])
  end

  defp scan_tokens(["[" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "[", "value" => "["} | tokens])
  end

  defp scan_tokens(["]" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "]", "value" => "]"} | tokens])
  end

  defp scan_tokens([";" | rest], tokens) do
    scan_tokens(rest, [%{"type" => ";", "value" => ";"} | tokens])
  end

  defp scan_tokens([":" | rest], tokens) do
    scan_tokens(rest, [%{"type" => ":", "value" => ":"} | tokens])
  end

  defp scan_tokens(["," | rest], tokens) do
    scan_tokens(rest, [%{"type" => ",", "value" => ","} | tokens])
  end

  defp scan_tokens(["(" | rest], tokens) do
    scan_tokens(rest, [%{"type" => "(", "value" => "("} | tokens])
  end

  defp scan_tokens([")" | rest], tokens) do
    scan_tokens(rest, [%{"type" => ")", "value" => ")"} | tokens])
  end

  # String literals
  defp scan_tokens(["\"" | rest], tokens) do
    {string_token, remaining} = scan_string(rest, [])
    scan_tokens(remaining, [string_token | tokens])
  end

  # Numbers - digits
  defp scan_tokens([c | rest], tokens) when c >= "0" and c <= "9" do
    {number_token, remaining} = scan_number([c | rest], [])
    scan_tokens(remaining, [number_token | tokens])
  end

  # Identifiers and keywords - letters and underscore
  defp scan_tokens([c | rest], tokens)
      when (c >= "a" and c <= "z") or (c >= "A" and c <= "Z") or c == "_" do
    {id_token, remaining} = scan_identifier([c | rest], [])
    scan_tokens(remaining, [id_token | tokens])
  end

  # Fallback for unexpected characters
  defp scan_tokens([c | _], _tokens) do
    raise "Unexpected character: #{c}"
  end

  # Scan comment
  defp scan_comment(chars) do
    {comment_chars, rest} = take_until_newline(chars)
    comment_text = Enum.join(comment_chars, "")
    token = %{"type" => "CMT", "value" => comment_text}
    {token, rest}
  end

  defp take_until_newline(chars) do
    take_until_newline(chars, [])
  end

  defp take_until_newline([], acc) do
    {Enum.reverse(acc), []}
  end

  defp take_until_newline(["\n" | rest], acc) do
    {Enum.reverse(acc), ["\n" | rest]}
  end

  defp take_until_newline([c | rest], acc) do
    take_until_newline(rest, [c | acc])
  end

  # Scan string
  defp scan_string(chars, acc) do
    scan_string_chars(chars, acc)
  end

  defp scan_string_chars([], _acc) do
    raise "Unterminated string literal"
  end

  defp scan_string_chars(["\"" | rest], acc) do
    string_content = Enum.join(Enum.reverse(acc), "")
    string_value = "\"" <> string_content <> "\""
    token = %{"type" => "STR", "value" => string_value}
    {token, rest}
  end

  defp scan_string_chars(["\\" | rest], acc) do
    case rest do
      ["n" | rest2] -> scan_string_chars(rest2, ["\\n" | acc])
      ["t" | rest2] -> scan_string_chars(rest2, ["\\t" | acc])
      ["\"" | rest2] -> scan_string_chars(rest2, ["\\\"" | acc])
      ["\\" | rest2] -> scan_string_chars(rest2, ["\\\\" | acc])
      [c | rest2] -> scan_string_chars(rest2, ["\\" <> c | acc])
      [] -> raise "Unterminated string literal"
    end
  end

  defp scan_string_chars([c | rest], acc) do
    scan_string_chars(rest, [c | acc])
  end

  # Scan number
  defp scan_number(chars, acc) do
    scan_number_digits(chars, acc, false)
  end

  defp scan_number_digits([], acc, has_decimal) do
    number_str = Enum.join(Enum.reverse(acc), "")
    token_type = if has_decimal, do: "DEC", else: "INT"
    token = %{"type" => token_type, "value" => number_str}
    {token, []}
  end

  defp scan_number_digits([c | rest] = chars, acc, has_decimal) do
    cond do
      c >= "0" and c <= "9" ->
        scan_number_digits(rest, [c | acc], has_decimal)

      c == "_" ->
        # Only allow underscore between digits
        case {acc, rest} do
          {[d | _], [d2 | _]} when d >= "0" and d <= "9" and d2 >= "0" and d2 <= "9" ->
            scan_number_digits(rest, [c | acc], has_decimal)

          _ ->
            raise "Invalid underscore in number"
        end

      c == "." and not has_decimal ->
        case rest do
          [d | _] when d >= "0" and d <= "9" ->
            scan_number_digits(rest, [c | acc], true)

          _ ->
            # Not a decimal number, return what we have so far
            number_str = Enum.join(Enum.reverse(acc), "")
            token = %{"type" => "INT", "value" => number_str}
            {token, chars}
        end

      true ->
        # End of number
        number_str = Enum.join(Enum.reverse(acc), "")
        token_type = if has_decimal, do: "DEC", else: "INT"
        token = %{"type" => token_type, "value" => number_str}
        {token, chars}
    end
  end

  # Scan identifier
  defp scan_identifier(chars, acc) do
    scan_identifier_chars(chars, acc)
  end

  defp scan_identifier_chars([], acc) do
    id_str = Enum.join(Enum.reverse(acc), "")
    token = make_identifier_token(id_str)
    {token, []}
  end

  defp scan_identifier_chars([c | rest] = chars, acc) do
    if (c >= "a" and c <= "z") or (c >= "A" and c <= "Z") or (c >= "0" and c <= "9") or
         c == "_" do
      scan_identifier_chars(rest, [c | acc])
    else
      id_str = Enum.join(Enum.reverse(acc), "")
      token = make_identifier_token(id_str)
      {token, chars}
    end
  end

  # Convert identifier string to appropriate token
  defp make_identifier_token("let"), do: %{"type" => "LET", "value" => "let"}
  defp make_identifier_token("mut"), do: %{"type" => "MUT", "value" => "mut"}
  defp make_identifier_token("if"), do: %{"type" => "IF", "value" => "if"}
  defp make_identifier_token("else"), do: %{"type" => "ELSE", "value" => "else"}
  defp make_identifier_token("true"), do: %{"type" => "TRUE", "value" => "true"}
  defp make_identifier_token("false"), do: %{"type" => "FALSE", "value" => "false"}
  defp make_identifier_token("nil"), do: %{"type" => "NIL", "value" => "nil"}
  defp make_identifier_token(id), do: %{"type" => "ID", "value" => id}
end