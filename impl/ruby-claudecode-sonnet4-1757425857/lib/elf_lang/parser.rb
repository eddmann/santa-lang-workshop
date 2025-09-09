module ElfLang
  class Parser
    def initialize(tokens)
      @tokens = tokens
      @pos = 0
    end
    
    def parse
      statements = []
      
      while @pos < @tokens.length
        stmt = parse_statement
        statements << stmt if stmt
      end
      
      {
        "type" => "Program",
        "statements" => statements
      }
    end
    
    private
    
    def current_token
      @pos < @tokens.length ? @tokens[@pos] : nil
    end
    
    def peek_token(offset = 1)
      pos = @pos + offset
      pos < @tokens.length ? @tokens[pos] : nil
    end
    
    def advance
      @pos += 1
    end
    
    def consume(expected_type)
      token = current_token
      if token && token['type'] == expected_type
        advance
        token
      else
        raise "Expected #{expected_type}, got #{token ? token['type'] : 'EOF'}"
      end
    end
    
    def parse_statement
      return nil unless current_token
      
      # Handle comments
      if current_token['type'] == 'CMT'
        comment_value = current_token['value']
        advance
        return {
          "type" => "Comment",
          "value" => comment_value
        }
      end
      
      expr = parse_expression
      
      # Optional semicolon
      if current_token && current_token['type'] == ';'
        advance
      end
      
      {
        "type" => "Expression",
        "value" => expr
      }
    end
    
    def parse_expression
      parse_let_or_expression
    end
    
    def parse_let_or_expression
      if current_token && current_token['type'] == 'LET'
        advance
        
        # Check for 'mut' keyword
        is_mutable = false
        if current_token && current_token['type'] == 'MUT'
          is_mutable = true
          advance
        end
        
        # Get identifier name
        name_token = consume('ID')
        name = {
          "type" => "Identifier",
          "name" => name_token['value']
        }
        
        # Consume '='
        consume('=')
        
        # Parse the value expression
        value = parse_assignment_expression
        
        {
          "type" => is_mutable ? "MutableLet" : "Let",
          "name" => name,
          "value" => value
        }
      else
        parse_assignment_expression
      end
    end
    
    def parse_assignment_expression
      expr = parse_or_expression
      
      # Check for assignment: identifier = value
      if current_token && current_token['type'] == '=' && expr['type'] == 'Identifier'
        advance  # consume '='
        value = parse_assignment_expression  # Right-associative
        
        return {
          "type" => "Assignment",
          "name" => expr,
          "value" => value
        }
      end
      
      expr
    end
    
    def parse_or_expression
      left = parse_and_expression
      
      while current_token && current_token['type'] == '||'
        op = current_token['value']
        advance
        right = parse_and_expression
        left = {
          "type" => "Infix",
          "operator" => op,
          "left" => left,
          "right" => right
        }
      end
      
      left
    end
    
    def parse_and_expression
      left = parse_equality_expression
      
      while current_token && current_token['type'] == '&&'
        op = current_token['value']
        advance
        right = parse_equality_expression
        left = {
          "type" => "Infix",
          "operator" => op,
          "left" => left,
          "right" => right
        }
      end
      
      left
    end
    
    def parse_equality_expression
      left = parse_comparison_expression
      
      while current_token && ['==', '!='].include?(current_token['type'])
        op = current_token['value']
        advance
        right = parse_comparison_expression
        left = {
          "type" => "Infix",
          "operator" => op,
          "left" => left,
          "right" => right
        }
      end
      
      left
    end
    
    def parse_comparison_expression
      left = parse_thread_expression
      
      while current_token && ['>', '<', '>=', '<='].include?(current_token['type'])
        op = current_token['value']
        advance
        right = parse_thread_expression
        left = {
          "type" => "Infix",
          "operator" => op,
          "left" => left,
          "right" => right
        }
      end
      
      left
    end
    
    def parse_thread_expression
      left = parse_compose_expression
      
      if current_token && current_token['type'] == '|>'
        # Collect all threaded functions
        functions = []
        
        while current_token && current_token['type'] == '|>'
          advance
          functions << parse_compose_expression
        end
        
        {
          "type" => "FunctionThread",
          "initial" => left,
          "functions" => functions
        }
      else
        left
      end
    end
    
    def parse_compose_expression
      left = parse_additive_expression
      
      if current_token && current_token['type'] == '>>'
        # Collect all composed functions
        functions = [left]
        
        while current_token && current_token['type'] == '>>'
          advance
          functions << parse_additive_expression
        end
        
        {
          "type" => "FunctionComposition",
          "functions" => functions
        }
      else
        left
      end
    end
    
    def parse_additive_expression
      left = parse_multiplicative_expression
      
      while current_token && ['+', '-'].include?(current_token['type'])
        op = current_token['value']
        advance
        right = parse_multiplicative_expression
        left = {
          "type" => "Infix",
          "operator" => op,
          "left" => left,
          "right" => right
        }
      end
      
      left
    end
    
    def parse_multiplicative_expression
      left = parse_unary_expression
      
      while current_token && ['*', '/'].include?(current_token['type'])
        op = current_token['value']
        advance
        right = parse_unary_expression
        left = {
          "type" => "Infix",
          "operator" => op,
          "left" => left,
          "right" => right
        }
      end
      
      left
    end
    
    def parse_unary_expression
      if current_token && current_token['type'] == '-'
        advance
        expr = parse_unary_expression
        {
          "type" => "Unary",
          "operator" => "-",
          "operand" => expr
        }
      else
        parse_postfix_expression
      end
    end
    
    def parse_postfix_expression
      left = parse_primary_expression
      
      while true
        case current_token&.[]('type')
        when '['
          # Indexing
          advance
          index = parse_expression
          consume(']')
          left = {
            "type" => "Index",
            "left" => left,
            "index" => index
          }
        when '('
          # Function call
          advance
          args = []
          
          unless current_token && current_token['type'] == ')'
            loop do
              args << parse_expression
              
              if current_token && current_token['type'] == ','
                advance
              else
                break
              end
            end
          end
          
          consume(')')
          left = {
            "type" => "Call",
            "function" => left,
            "arguments" => args
          }
        else
          break
        end
      end
      
      left
    end
    
    def parse_primary_expression
      return nil unless current_token
      
      case current_token['type']
      when 'INT'
        value = current_token['value']
        advance
        {
          "type" => "Integer",
          "value" => value
        }
      when 'DEC'
        value = current_token['value']
        advance
        {
          "type" => "Decimal",
          "value" => value
        }
      when 'STR'
        raw_value = current_token['value']
        # Remove surrounding quotes and handle escapes
        content = raw_value[1...-1]  # Remove quotes
        content = content.gsub('\\n', "\n").gsub('\\t', "\t").gsub('\\"', '"').gsub('\\\\', '\\')
        advance
        {
          "type" => "String",
          "value" => content
        }
      when 'TRUE'
        advance
        {
          "type" => "Boolean",
          "value" => true
        }
      when 'FALSE'
        advance
        {
          "type" => "Boolean",
          "value" => false
        }
      when 'NIL'
        advance
        {
          "type" => "Nil"
        }
      when 'ID'
        name = current_token['value']
        advance
        {
          "type" => "Identifier",
          "name" => name
        }
      when '+', '-', '*', '/'
        name = current_token['value']
        advance
        {
          "type" => "Identifier",
          "name" => name
        }
      when '('
        advance
        expr = parse_expression
        consume(')')
        expr
      when '['
        parse_list_literal
      when '{'
        parse_set_literal
      when '#{'
        parse_dict_literal
      when '|'
        parse_function_literal
      when '||'
        parse_empty_function_literal
      when 'IF'
        parse_if_expression
      else
        raise "Unexpected token: #{current_token['type']}"
      end
    end
    
    def parse_list_literal
      advance  # consume '['
      elements = []
      
      unless current_token && current_token['type'] == ']'
        loop do
          elements << parse_expression
          
          if current_token && current_token['type'] == ','
            advance
          else
            break
          end
        end
      end
      
      consume(']')
      
      {
        "type" => "List",
        "items" => elements
      }
    end
    
    def parse_set_literal
      advance  # consume '{'
      elements = []
      
      unless current_token && current_token['type'] == '}'
        loop do
          elements << parse_expression
          
          if current_token && current_token['type'] == ','
            advance
          else
            break
          end
        end
      end
      
      consume('}')
      
      {
        "type" => "Set",
        "items" => elements
      }
    end
    
    def parse_dict_literal
      advance  # consume '#{'
      pairs = []
      
      unless current_token && current_token['type'] == '}'
        loop do
          key = parse_expression
          consume(':')
          value = parse_expression
          
          pairs << {
            "key" => key,
            "value" => value
          }
          
          if current_token && current_token['type'] == ','
            advance
          else
            break
          end
        end
      end
      
      consume('}')
      
      {
        "type" => "Dictionary",
        "items" => pairs
      }
    end
    
    def parse_function_literal
      advance  # consume '|'
      parameters = []
      
      unless current_token && current_token['type'] == '|'
        loop do
          param_token = consume('ID')
          parameters << {
            "type" => "Identifier",
            "name" => param_token['value']
          }
          
          if current_token && current_token['type'] == ','
            advance
          else
            break
          end
        end
      end
      
      consume('|')
      
      # Check if we have a block or a simple expression
      if current_token && current_token['type'] == '{'
        body = parse_block
      else
        # Wrap expression in a Block
        expr = parse_expression
        body = {
          "type" => "Block",
          "statements" => [
            {
              "type" => "Expression",
              "value" => expr
            }
          ]
        }
      end
      
      {
        "type" => "Function",
        "parameters" => parameters,
        "body" => body
      }
    end
    
    def parse_empty_function_literal
      advance  # consume '||'
      parameters = []  # No parameters for ||
      
      # Check if we have a block or a simple expression
      if current_token && current_token['type'] == '{'
        body = parse_block
      else
        # Wrap expression in a Block
        expr = parse_expression
        body = {
          "type" => "Block",
          "statements" => [
            {
              "type" => "Expression",
              "value" => expr
            }
          ]
        }
      end
      
      {
        "type" => "Function",
        "parameters" => parameters,
        "body" => body
      }
    end
    
    def parse_if_expression
      advance  # consume 'IF'
      
      condition = parse_expression
      then_branch = parse_block
      
      else_branch = nil
      if current_token && current_token['type'] == 'ELSE'
        advance
        else_branch = parse_block
      end
      
      {
        "type" => "If",
        "condition" => condition,
        "consequence" => then_branch,
        "alternative" => else_branch
      }
    end
    
    def parse_block
      consume('{')
      statements = []
      
      while current_token && current_token['type'] != '}'
        stmt = parse_statement
        statements << stmt if stmt
      end
      
      consume('}')
      
      {
        "type" => "Block",
        "statements" => statements
      }
    end
  end
end