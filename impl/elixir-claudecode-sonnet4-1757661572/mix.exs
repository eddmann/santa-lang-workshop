defmodule ElfLang.MixProject do
  use Mix.Project

  def project do
    [
      app: :elf_lang,
      version: "0.1.0",
      elixir: "~> 1.18",
      start_permanent: Mix.env() == :prod,
      escript: escript(),
      deps: deps()
    ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp escript do
    [
      main_module: ElfLang.CLI,
      name: "elf_lang"
    ]
  end

  defp deps do
    [
      {:jason, "~> 1.4"}
    ]
  end
end