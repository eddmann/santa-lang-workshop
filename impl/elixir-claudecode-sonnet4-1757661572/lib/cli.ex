defmodule ElfLang.CLI do
  @moduledoc """
  Command-line interface for elf-lang
  """

  def main(args) do
    case args do
      [] ->
        IO.puts("Usage: elf_lang <command> [args]")
        IO.puts("Commands:")
        IO.puts("  <file>        - Run program")
        IO.puts("  tokens <file> - Print tokens")
        IO.puts("  ast <file>    - Print AST")
        System.halt(1)

      ["tokens", file] ->
        handle_tokens(file)

      ["ast", file] ->
        handle_ast(file)

      [file] ->
        handle_run(file)

      _ ->
        IO.puts("Invalid arguments")
        System.halt(1)
    end
  end

  defp handle_tokens(file) do
    case File.read(file) do
      {:ok, content} ->
        tokens = ElfLang.tokenize(content)
        Enum.each(tokens, fn token ->
          IO.puts(Jason.encode!(token))
        end)

      {:error, reason} ->
        IO.puts("Error reading file: #{reason}")
        System.halt(1)
    end
  end

  defp handle_ast(file) do
    case File.read(file) do
      {:ok, content} ->
        tokens = ElfLang.tokenize(content)
        ast = ElfLang.parse(tokens)
        # Pretty print with 2-space indentation and sorted keys
        json_opts = [pretty: true, indent: "  ", sort_keys: true]
        IO.puts(Jason.encode!(ast, json_opts))

      {:error, reason} ->
        IO.puts("Error reading file: #{reason}")
        System.halt(1)
    end
  end

  defp handle_run(file) do
    case File.read(file) do
      {:ok, content} ->
        try do
          tokens = ElfLang.tokenize(content)
          ast = ElfLang.parse(tokens)
          {result, _state} = ElfLang.evaluate(ast)
          # Print the final result of the program
          formatted_result = ElfLang.Evaluator.format_value(result)
          IO.puts(formatted_result)
        rescue
          e in RuntimeError ->
            IO.puts("[Error] #{e.message}")
            System.halt(1)
        end

      {:error, reason} ->
        IO.puts("Error reading file: #{reason}")
        System.halt(1)
    end
  end
end