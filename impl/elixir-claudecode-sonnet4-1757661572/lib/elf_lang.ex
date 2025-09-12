defmodule ElfLang do
  @moduledoc """
  Elf-lang implementation in Elixir
  """

  def tokenize(source) do
    ElfLang.Lexer.tokenize(source)
  end

  def parse(tokens) do
    ElfLang.Parser.parse(tokens)
  end

  def evaluate(ast) do
    ElfLang.Evaluator.evaluate(ast)
  end
end