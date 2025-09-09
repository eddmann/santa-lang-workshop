require 'json'

module ElfLang
  class CLI
    def self.tokens(file_path)
      source = File.read(file_path, encoding: 'UTF-8')
      lexer = Lexer.new(source)
      tokens = lexer.tokenize
      
      tokens.each do |token|
        puts JSON.generate(token, space: '')
      end
    rescue => e
      puts e.message
      exit 1
    end
    
    def self.ast(file_path)
      source = File.read(file_path, encoding: 'UTF-8')
      lexer = Lexer.new(source)
      tokens = lexer.tokenize
      parser = Parser.new(tokens)
      ast = parser.parse
      
      puts JSON.pretty_generate(sort_keys(ast), indent: '  ')
    rescue => e
      puts e.message
      exit 1
    end
    
    private
    
    def self.sort_keys(obj)
      case obj
      when Hash
        sorted_hash = {}
        obj.keys.sort.each do |key|
          sorted_hash[key] = sort_keys(obj[key])
        end
        sorted_hash
      when Array
        obj.map { |item| sort_keys(item) }
      else
        obj
      end
    end
    
    def self.run(file_path)
      source = File.read(file_path, encoding: 'UTF-8')
      lexer = Lexer.new(source)
      tokens = lexer.tokenize
      parser = Parser.new(tokens)
      ast = parser.parse
      evaluator = Evaluator.new
      result = evaluator.evaluate(ast)
      
      # Print the final result of the program
      puts format_value_for_output(result)
    rescue ElfLang::EvaluationError => e
      puts "[Error] #{e.message}"
      exit 1
    rescue => e
      puts e.message
      exit 1
    end
    
    def self.format_value_for_output(value)
      case value
      when String
        "\"#{value}\""
      when Integer
        value.to_s
      when Float
        # If it's a whole number, display as integer
        if value == value.to_i
          value.to_i.to_s
        else
          value.to_s
        end
      when true
        "true"
      when false
        "false"
      when nil
        "nil"
      else
        value.to_s
      end
    end
  end
end

require_relative 'elf_lang/lexer'
require_relative 'elf_lang/parser'
require_relative 'elf_lang/evaluator'