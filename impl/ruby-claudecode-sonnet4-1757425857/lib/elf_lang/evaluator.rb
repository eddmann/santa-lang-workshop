module ElfLang
  class EvaluationError < StandardError; end
  
  # Represents a user-defined function
  class UserFunction
    attr_reader :parameters, :body, :closure_env
    
    def initialize(parameters, body, closure_env)
      @parameters = parameters
      @body = body
      @closure_env = closure_env
    end
    
    def arity
      @parameters.length
    end
  end
  
  # Represents a partially applied function
  class PartialFunction
    attr_reader :function, :bound_args
    
    def initialize(function, bound_args)
      @function = function
      @bound_args = bound_args
    end
  end
  
  # Represents a variable that can be shared across environments for mutable capture
  class VariableCell
    attr_accessor :value
    
    def initialize(value)
      @value = value
    end
  end
  
  class Environment
    def initialize(parent = nil)
      @parent = parent
      @variables = {}  # name -> VariableCell
      @mutability = {}
    end
    
    def define(name, value, mutable = false)
      @variables[name] = VariableCell.new(value)
      @mutability[name] = mutable
    end
    
    def get(name)
      if @variables.key?(name)
        @variables[name].value
      elsif @parent
        @parent.get(name)
      else
        raise EvaluationError, "Identifier can not be found: #{name}"
      end
    end
    
    def set(name, value)
      if @variables.key?(name)
        unless @mutability[name]
          raise EvaluationError, "Variable '#{name}' is not mutable"
        end
        @variables[name].value = value
      elsif @parent
        @parent.set(name, value)
      else
        raise EvaluationError, "Identifier can not be found: #{name}"
      end
    end
    
    def is_mutable?(name)
      if @variables.key?(name)
        @mutability[name]
      elsif @parent
        @parent.is_mutable?(name)
      else
        false
      end
    end
    
    # Capture variable cells for closures to maintain shared references
    def capture_mutable_variables
      captured = {}
      @variables.each do |name, cell|
        if @mutability[name]
          captured[name] = cell  # Share the same cell
        end
      end
      
      # Also capture from parent environments
      if @parent
        parent_captured = @parent.capture_mutable_variables
        captured.merge!(parent_captured) { |key, new_val, old_val| new_val }
      end
      
      captured
    end
    
    # Create a new environment with shared mutable variable cells
    def with_captured_mutables(captured_cells)
      new_env = Environment.new(self)
      captured_cells.each do |name, cell|
        new_env.instance_variable_get(:@variables)[name] = cell
        new_env.instance_variable_get(:@mutability)[name] = true
      end
      new_env
    end
  end
  
  class Evaluator
    def initialize
      @global_env = Environment.new
      setup_builtins
    end
    
    def evaluate(ast)
      evaluate_node(ast, @global_env)
    end
    
    private
    
    def setup_builtins
      @global_env.define('puts', :builtin_puts)
      @global_env.define('+', :builtin_add)
      @global_env.define('-', :builtin_subtract)
      @global_env.define('*', :builtin_multiply)
      @global_env.define('/', :builtin_divide)
      @global_env.define('push', :builtin_push)
      @global_env.define('assoc', :builtin_assoc)
      @global_env.define('first', :builtin_first)
      @global_env.define('rest', :builtin_rest)
      @global_env.define('size', :builtin_size)
      @global_env.define('map', :builtin_map)
      @global_env.define('filter', :builtin_filter)
      @global_env.define('fold', :builtin_fold)
    end
    
    def evaluate_node(node, env)
      case node['type']
      when 'Program'
        result = nil
        node['statements'].each do |stmt|
          stmt_result = evaluate_node(stmt, env)
          # Only update result with non-comment results
          unless stmt['type'] == 'Comment' || (stmt['type'] == 'Expression' && stmt['value'] && stmt['value']['type'] == 'Comment')
            result = stmt_result
          end
        end
        result
      
      when 'Expression'
        evaluate_node(node['value'], env)
      
      when 'Integer'
        # Remove underscores and convert to integer
        node['value'].gsub('_', '').to_i
      
      when 'Decimal'
        # Remove underscores and convert to float
        node['value'].gsub('_', '').to_f
      
      when 'String'
        # String value from parser already has quotes removed, just handle escapes
        parse_string_literal(node['value'])
      
      when 'Boolean'
        node['value']
      
      when 'Nil'
        nil
      
      when 'Identifier'
        env.get(node['name'])
      
      when 'Let'
        name = node['name']['name']
        value = evaluate_node(node['value'], env)
        mutable = node['mutable'] == true
        env.define(name, value, mutable)
        nil
      
      when 'MutableLet'
        name = node['name']['name']
        value = evaluate_node(node['value'], env)
        env.define(name, value, true)  # Mutable
        nil
      
      when 'Assignment'
        name = node['name']['name']
        value = evaluate_node(node['value'], env)
        env.set(name, value)
        value
      
      when 'Binary', 'Infix'
        left = evaluate_node(node['left'], env)
        right = evaluate_node(node['right'], env)
        evaluate_binary_operation(node['operator'], left, right)
      
      when 'Unary'
        operand = evaluate_node(node['operand'], env)
        evaluate_unary_operation(node['operator'], operand)
      
      when 'Call'
        function = evaluate_node(node['function'], env)
        args = node['arguments'].map { |arg| evaluate_node(arg, env) }
        
        case function
        when :builtin_puts
          evaluate_puts(args)
        when :builtin_add
          case args.length
          when 1
            # Partial application
            PartialFunction.new(create_operator_function('+'), args)
          when 2
            evaluate_binary_operation('+', args[0], args[1])
          else
            # Extra args ignored in prefix form
            evaluate_binary_operation('+', args[0], args[1])
          end
        when :builtin_subtract
          case args.length
          when 1
            # Partial application
            PartialFunction.new(create_operator_function('-'), args)
          when 2
            evaluate_binary_operation('-', args[0], args[1])
          else
            # Extra args ignored in prefix form
            evaluate_binary_operation('-', args[0], args[1])
          end
        when :builtin_multiply
          case args.length
          when 1
            # Partial application
            PartialFunction.new(create_operator_function('*'), args)
          when 2
            evaluate_binary_operation('*', args[0], args[1])
          else
            # Extra args ignored in prefix form
            evaluate_binary_operation('*', args[0], args[1])
          end
        when :builtin_divide
          case args.length
          when 1
            # Partial application
            PartialFunction.new(create_operator_function('/'), args)
          when 2
            evaluate_binary_operation('/', args[0], args[1])
          else
            # Extra args ignored in prefix form
            evaluate_binary_operation('/', args[0], args[1])
          end
        when :builtin_push
          case args.length
          when 1
            # Partial application
            PartialFunction.new(create_builtin_wrapper(:push), args)
          when 2
            evaluate_push(args[0], args[1])
          else
            # Extra args ignored
            evaluate_push(args[0], args[1])
          end
        when :builtin_assoc
          case args.length
          when 1
            # Partial application with key only
            PartialFunction.new(create_builtin_wrapper(:assoc), args)
          when 2
            # Partial application with key and value
            PartialFunction.new(create_builtin_wrapper(:assoc), args)
          when 3
            evaluate_assoc(args[0], args[1], args[2])
          else
            raise EvaluationError, "Wrong number of arguments for assoc: expected 1-3, got #{args.length}"
          end
        when :builtin_first
          if args.length != 1
            raise EvaluationError, "Wrong number of arguments for first: expected 1, got #{args.length}"
          end
          evaluate_first(args[0])
        when :builtin_rest
          if args.length != 1
            raise EvaluationError, "Wrong number of arguments for rest: expected 1, got #{args.length}"
          end
          evaluate_rest(args[0])
        when :builtin_size
          if args.length != 1
            raise EvaluationError, "Wrong number of arguments for size: expected 1, got #{args.length}"
          end
          evaluate_size(args[0])
        when :builtin_map
          evaluate_map(args)
        when :builtin_filter
          evaluate_filter(args)
        when :builtin_fold
          evaluate_fold(args)
        when UserFunction
          call_user_function(function, args)
        when PartialFunction
          call_partial_function(function, args)
        else
          if is_callable?(function)
            call_function(function, args)
          else
            raise EvaluationError, "Expected a Function, found: #{type_name(function)}"
          end
        end
      
      when 'List'
        node['items'].map { |item| evaluate_node(item, env) }
      
      when 'Set'
        items = node['items'].map { |item| evaluate_node(item, env) }
        # Check if any item is a Dictionary, which is not allowed in sets
        items.each do |item|
          if item.is_a?(Hash) && item[:__type] == :dictionary
            raise EvaluationError, "Unable to include a Dictionary within a Set"
          end
        end
        create_set(items)
      
      when 'Dictionary'
        dict = {}
        node['items'].each do |pair|
          key = evaluate_node(pair['key'], env)
          value = evaluate_node(pair['value'], env)
          
          # Check if key is a Dictionary, which is not allowed as dict keys
          if key.is_a?(Hash) && key[:__type] == :dictionary
            raise EvaluationError, "Unable to use a Dictionary as a Dictionary key"
          end
          
          dict[key] = value
        end
        create_dictionary(dict)
      
      when 'Index'
        target = evaluate_node(node['left'], env)
        index = evaluate_node(node['index'], env)
        evaluate_index_operation(target, index)
      
      when 'Comment'
        # Comments don't evaluate to anything
        nil
      
      when 'Function'
        # Create a user function with closure environment that captures mutable variables by reference
        captured_mutables = env.capture_mutable_variables
        closure_env = env.with_captured_mutables(captured_mutables)
        UserFunction.new(node['parameters'], node['body'], closure_env)
      
      when 'Block'
        result = nil
        node['statements'].each do |stmt|
          result = evaluate_node(stmt, env)
        end
        result
      
      when 'If'
        condition = evaluate_node(node['condition'], env)
        if is_truthy(condition)
          evaluate_node(node['consequence'], env)
        elsif node['alternative']
          evaluate_node(node['alternative'], env)
        else
          nil
        end
      
      when 'FunctionComposition'
        # Create a composed function: f >> g creates x -> g(f(x))
        functions = node['functions'].map { |func_node| evaluate_node(func_node, env) }
        create_composed_function(functions)
      
      when 'FunctionThread'
        # Threading: value |> func1 |> func2 becomes func2(func1(value))
        initial_value = evaluate_node(node['initial'], env)
        functions = node['functions'].map { |func_node| evaluate_node(func_node, env) }
        functions.reduce(initial_value) { |acc, func| call_threaded_function(func, acc) }
      
      else
        raise EvaluationError, "Unknown node type: #{node['type']}"
      end
    end
    
    def parse_string_literal(content)
      # Parser already removed quotes, just handle escape sequences
      content.gsub(/\\(.)/) do |match|
        case $1
        when 'n' then "\n"
        when 't' then "\t"
        when 'r' then "\r"
        when '\\' then "\\"
        when '"' then '"'
        else $1  # Keep other escapes as-is
        end
      end
    end
    
    def evaluate_puts(args)
      output_parts = args.map { |arg| format_value(arg) }
      
      # Join all arguments with spaces and add trailing space
      puts output_parts.join(" ") + " "
      
      nil
    end
    
    def format_value(value)
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
      when Array
        "[#{value.map { |item| format_value(item) }.join(', ')}]"
      when Hash
        if value[:__type] == :set
          # Sets print in ascending order by value
          sorted_items = value[:__items].sort_by { |item| sort_key(item) }
          "{#{sorted_items.map { |item| format_value(item) }.join(', ')}}"
        elsif value[:__type] == :dictionary
          # Dictionaries print in ascending order by key
          sorted_pairs = value[:__pairs].sort_by { |k, v| sort_key(k) }
          pairs_str = sorted_pairs.map { |k, v| "#{format_value(k)}: #{format_value(v)}" }.join(', ')
          "#\{#{pairs_str}}"
        else
          value.to_s
        end
      when UserFunction, PartialFunction
        "Function"
      else
        value.to_s
      end
    end
    
    def evaluate_binary_operation(operator, left, right)
      case operator
      when '+'
        if left.is_a?(String) && right.is_a?(String)
          left + right
        elsif left.is_a?(String) || right.is_a?(String)
          # String concatenation with other types
          left.to_s + right.to_s
        elsif (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          left + right
        elsif left.is_a?(Array) && right.is_a?(Array)
          # List concatenation
          left + right
        elsif left.is_a?(Array) && right.is_a?(Integer)
          # List + Integer is not allowed
          raise EvaluationError, "Unsupported operation: List + Integer"
        elsif is_set?(left) && is_set?(right)
          # Set union
          create_set(left[:__items] + right[:__items])
        elsif is_set?(left) && right.is_a?(Integer)
          # Set + Integer is not allowed  
          raise EvaluationError, "Unsupported operation: Set + Integer"
        elsif is_dictionary?(left) && is_dictionary?(right)
          # Dictionary merge - right-biased
          result_dict = left[:__pairs].dup
          right[:__pairs].each { |k, v| result_dict[k] = v }
          create_dictionary(result_dict)
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} + #{type_name(right)}"
        end
      
      when '-'
        if (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          left - right
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} - #{type_name(right)}"
        end
      
      when '*'
        if left.is_a?(String) && right.is_a?(Integer)
          if right < 0
            raise EvaluationError, "Unsupported operation: String * Integer (< 0)"
          end
          left * right
        elsif left.is_a?(String) && right.is_a?(Float)
          raise EvaluationError, "Unsupported operation: String * Decimal"
        elsif (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          left * right
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} * #{type_name(right)}"
        end
      
      when '/'
        if (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          if right == 0 || right == 0.0
            raise EvaluationError, "Division by zero"
          end
          
          if left.is_a?(Integer) && right.is_a?(Integer)
            # Integer division - truncate towards zero (not floor division)
            (left.to_f / right.to_f).truncate
          else
            # At least one operand is float
            left.to_f / right.to_f
          end
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} / #{type_name(right)}"
        end
      
      when '=='
        structural_equal?(left, right)
      
      when '!='
        !structural_equal?(left, right)
      
      when '>'
        if (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          left > right
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} > #{type_name(right)}"
        end
      
      when '<'
        if (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          left < right
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} < #{type_name(right)}"
        end
      
      when '>='
        if (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          left >= right
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} >= #{type_name(right)}"
        end
      
      when '<='
        if (left.is_a?(Integer) || left.is_a?(Float)) && (right.is_a?(Integer) || right.is_a?(Float))
          left <= right
        else
          raise EvaluationError, "Unsupported operation: #{type_name(left)} <= #{type_name(right)}"
        end
      
      when '&&'
        is_truthy(left) && is_truthy(right)
      
      when '||'
        is_truthy(left) || is_truthy(right)
      
      else
        raise EvaluationError, "Unknown binary operator: #{operator}"
      end
    end
    
    def evaluate_unary_operation(operator, operand)
      case operator
      when '-'
        if operand.is_a?(Integer) || operand.is_a?(Float)
          -operand
        else
          raise EvaluationError, "Unsupported operation: -#{type_name(operand)}"
        end
      else
        raise EvaluationError, "Unknown unary operator: #{operator}"
      end
    end
    
    def is_truthy(value)
      # In elf-lang, only false and nil are falsy
      !(value == false || value.nil?)
    end
    
    def type_name(value)
      case value
      when Integer then "Integer"
      when Float then "Decimal"
      when String then "String"
      when true, false then "Boolean"
      when nil then "Nil"
      when Array then "List"
      when UserFunction, PartialFunction then "Function"
      when Hash
        if value[:__type] == :set
          "Set"
        elsif value[:__type] == :dictionary
          "Dictionary"
        else
          "Hash"
        end
      else "Unknown"
      end
    end
    
    # Collection creation and helper methods
    def create_set(items)
      # Remove duplicates and maintain order
      unique_items = []
      items.each do |item|
        unless unique_items.any? { |existing| structural_equal?(existing, item) }
          unique_items << item
        end
      end
      { __type: :set, __items: unique_items }
    end
    
    def create_dictionary(pairs_hash)
      { __type: :dictionary, __pairs: pairs_hash }
    end
    
    def is_set?(value)
      value.is_a?(Hash) && value[:__type] == :set
    end
    
    def is_dictionary?(value)
      value.is_a?(Hash) && value[:__type] == :dictionary
    end
    
    # Structural equality
    def structural_equal?(left, right)
      return true if left.equal?(right)
      return false if left.class != right.class
      
      case left
      when Array
        return false if left.length != right.length
        left.zip(right).all? { |l, r| structural_equal?(l, r) }
      when Hash
        if is_set?(left) && is_set?(right)
          return false if left[:__items].length != right[:__items].length
          left[:__items].all? do |item1|
            right[:__items].any? { |item2| structural_equal?(item1, item2) }
          end
        elsif is_dictionary?(left) && is_dictionary?(right)
          return false if left[:__pairs].length != right[:__pairs].length
          left[:__pairs].all? do |k, v|
            right[:__pairs].key?(k) && structural_equal?(right[:__pairs][k], v)
          end
        else
          false
        end
      else
        left == right
      end
    end
    
    # Sorting key for deterministic ordering
    def sort_key(value)
      case value
      when Integer then [0, value]
      when Float then [1, value]  
      when String then [2, value]
      when true then [3, 1]
      when false then [3, 0]
      when nil then [4, 0]
      when Array then [5, value.map { |item| sort_key(item) }]
      when Hash
        if is_set?(value)
          [6, value[:__items].map { |item| sort_key(item) }.sort]
        elsif is_dictionary?(value)
          [7, value[:__pairs].map { |k, v| [sort_key(k), sort_key(v)] }.sort]
        else
          [8, value.to_s]
        end
      else
        [9, value.to_s]
      end
    end
    
    # Built-in functions
    def evaluate_push(item, collection)
      if collection.is_a?(Array)
        # Lists: push returns new list with item appended
        collection + [item]
      elsif is_set?(collection)
        # Sets: push returns new set with item added (avoiding duplicates)
        new_items = collection[:__items].dup
        unless new_items.any? { |existing| structural_equal?(existing, item) }
          new_items << item
        end
        create_set(new_items)
      else
        raise EvaluationError, "push can only be used with Lists and Sets"
      end
    end
    
    def evaluate_assoc(key, value, dictionary)
      unless is_dictionary?(dictionary)
        raise EvaluationError, "assoc can only be used with Dictionaries"
      end
      
      new_pairs = dictionary[:__pairs].dup
      new_pairs[key] = value
      create_dictionary(new_pairs)
    end
    
    def evaluate_first(collection)
      case collection
      when Array
        collection.empty? ? nil : collection.first
      when String
        collection.empty? ? nil : collection[0]
      when Hash
        if is_set?(collection)
          collection[:__items].empty? ? nil : collection[:__items].first
        elsif is_dictionary?(collection)
          collection[:__pairs].empty? ? nil : collection[:__pairs].first[1]
        else
          nil
        end
      else
        nil
      end
    end
    
    def evaluate_rest(collection)
      case collection
      when Array
        collection.length <= 1 ? [] : collection[1..-1]
      when String
        collection.length <= 1 ? "" : collection[1..-1]
      when Hash
        if is_set?(collection)
          items = collection[:__items]
          items.length <= 1 ? create_set([]) : create_set(items[1..-1])
        elsif is_dictionary?(collection)
          pairs = collection[:__pairs]
          if pairs.empty?
            create_dictionary({})
          else
            # Remove first key-value pair (in insertion order)
            new_pairs = pairs.dup
            first_key = new_pairs.keys.first
            new_pairs.delete(first_key)
            create_dictionary(new_pairs)
          end
        else
          nil
        end
      else
        nil
      end
    end
    
    def evaluate_size(collection)
      case collection
      when Array
        collection.length
      when String
        # UTF-8 byte count as per spec
        collection.bytesize
      when Hash
        if is_set?(collection)
          collection[:__items].length
        elsif is_dictionary?(collection)
          collection[:__pairs].length
        else
          nil
        end
      else
        nil
      end
    end
    
    def evaluate_index_operation(target, index)
      case target
      when String
        unless index.is_a?(Integer)
          raise EvaluationError, "Unable to perform index operation, found: String[#{type_name(index)}]"
        end
        
        # Handle negative indices
        actual_index = index < 0 ? target.length + index : index
        
        if actual_index >= 0 && actual_index < target.length
          target[actual_index]
        else
          nil
        end
        
      when Array
        unless index.is_a?(Integer)
          raise EvaluationError, "Unable to perform index operation, found: List[#{type_name(index)}]"
        end
        
        # Handle negative indices
        actual_index = index < 0 ? target.length + index : index
        
        if actual_index >= 0 && actual_index < target.length
          target[actual_index]
        else
          nil
        end
        
      when Hash
        if is_dictionary?(target)
          # Dictionary indexing returns value or nil
          target[:__pairs][index]
        else
          raise EvaluationError, "Unable to perform index operation on #{type_name(target)}"
        end
        
      else
        raise EvaluationError, "Unable to perform index operation on #{type_name(target)}"
      end
    end
    
    # Function calling support
    def is_callable?(value)
      value.is_a?(UserFunction) || value.is_a?(PartialFunction) || value.is_a?(Symbol)
    end
    
    def call_function(function, args)
      case function
      when UserFunction
        call_user_function(function, args)
      when PartialFunction
        call_partial_function(function, args)
      else
        raise EvaluationError, "Cannot call #{type_name(function)}"
      end
    end
    
    def call_user_function(func, args)
      # Handle special builtin wrappers
      if func.instance_variable_get(:@operator)
        op = func.instance_variable_get(:@operator)
        return evaluate_binary_operation(op, args[0], args[1]) if args.length >= 2
        return PartialFunction.new(func, args) if args.length == 1
        raise EvaluationError, "Wrong number of arguments for operator #{op}"
      end
      
      if func.instance_variable_get(:@builtin_name)
        name = func.instance_variable_get(:@builtin_name)
        case name
        when :push
          if args.length >= 2
            return evaluate_push(args[0], args[1])
          elsif args.length == 1
            return PartialFunction.new(func, args)
          else
            raise EvaluationError, "Wrong number of arguments for push: expected at least 1, got #{args.length}"
          end
        end
      end
      
      if func.instance_variable_get(:@composed_functions)
        # Handle function composition: f >> g >> h becomes x -> h(g(f(x)))
        functions = func.instance_variable_get(:@composed_functions)
        return PartialFunction.new(func, args) if args.length == 0
        
        # Apply functions in sequence
        result = args[0]
        functions.each do |f|
          result = call_function_like(f, [result])
        end
        return result
      end
      
      # Handle builtin function wrappers for higher-order functions
      if func.instance_variable_get(:@builtin_type)
        builtin_type = func.instance_variable_get(:@builtin_type)
        case builtin_type
        when :map
          return evaluate_map(args)
        when :filter
          return evaluate_filter(args)
        when :fold
          return evaluate_fold(args)
        end
      end
      
      # Handle partial application for regular functions
      if func.parameters && args.length < func.arity
        # Return a partial function
        return PartialFunction.new(func, args)
      elsif func.parameters && args.length > func.arity
        # Extra arguments are ignored (per spec)
        args = args[0...func.arity]
      end
      
      # Create new environment for function execution, inheriting from closure environment
      func_env = Environment.new(func.closure_env)
      
      # Bind parameters to arguments
      func.parameters.each_with_index do |param, index|
        func_env.define(param['name'], args[index])
      end
      
      # Execute function body
      evaluate_node(func.body, func_env)
    end
    
    def call_partial_function(partial_func, args)
      # Combine bound args with new args
      combined_args = partial_func.bound_args + args
      
      # Check if the underlying function is a builtin wrapper  
      func = partial_func.function
      if func.instance_variable_get(:@builtin_type)
        builtin_type = func.instance_variable_get(:@builtin_type)
        case builtin_type
        when :map
          return evaluate_map(combined_args)
        when :filter
          return evaluate_filter(combined_args)
        when :fold
          return evaluate_fold(combined_args)
        end
      end
      
      # Call the underlying function
      call_user_function(func, combined_args)
    end
    
    # Higher-order function implementations
    def evaluate_map(args)
      case args.length
      when 1
        # Partial application: map(fn) -> returns function that takes list
        func = args[0]
        unless is_function_like?(func)
          raise EvaluationError, "Unexpected argument: map(#{type_name(func)})"
        end
        return PartialFunction.new(create_builtin_function(:map, 2), [func])
      when 2
        func, list = args
        unless is_function_like?(func)
          raise EvaluationError, "Unexpected argument: map(#{type_name(func)}, #{type_name(list)})"
        end
        unless list.is_a?(Array)
          raise EvaluationError, "Unexpected argument: map(#{type_name(func)}, #{type_name(list)})"
        end
        
        # Map function over list
        list.map { |item| call_function_like(func, [item]) }
      else
        raise EvaluationError, "Wrong number of arguments for map: expected 1 or 2, got #{args.length}"
      end
    end
    
    def evaluate_filter(args)
      case args.length
      when 1
        # Partial application: filter(fn) -> returns function that takes list
        func = args[0]
        unless is_function_like?(func)
          raise EvaluationError, "Unexpected argument: filter(#{type_name(func)})"
        end
        return PartialFunction.new(create_builtin_function(:filter, 2), [func])
      when 2
        func, list = args
        unless is_function_like?(func)
          raise EvaluationError, "Unexpected argument: filter(#{type_name(func)}, #{type_name(list)})"
        end
        unless list.is_a?(Array)
          raise EvaluationError, "Unexpected argument: filter(#{type_name(func)}, #{type_name(list)})"
        end
        
        # Filter list with function
        list.select { |item| is_truthy(call_function_like(func, [item])) }
      else
        raise EvaluationError, "Wrong number of arguments for filter: expected 1 or 2, got #{args.length}"
      end
    end
    
    def evaluate_fold(args)
      case args.length
      when 2
        # Partial application: fold(init, fn) -> returns function that takes list
        init, func = args
        unless is_function_like?(func)
          raise EvaluationError, "Unexpected argument: fold(#{type_name(init)}, #{type_name(func)})"
        end
        return PartialFunction.new(create_builtin_function(:fold, 3), [init, func])
      when 3
        init, func, list = args
        unless is_function_like?(func)
          raise EvaluationError, "Unexpected argument: fold(#{type_name(init)}, #{type_name(func)}, #{type_name(list)})"
        end
        unless list.is_a?(Array)
          raise EvaluationError, "Unexpected argument: fold(#{type_name(init)}, #{type_name(func)}, #{type_name(list)})"
        end
        
        # Fold list with function
        list.reduce(init) { |acc, item| call_function_like(func, [acc, item]) }
      else
        raise EvaluationError, "Wrong number of arguments for fold: expected 2 or 3, got #{args.length}"
      end
    end
    
    def is_function_like?(value)
      value.is_a?(UserFunction) || value.is_a?(PartialFunction) || operator_function?(value)
    end
    
    def operator_function?(value)
      value == :builtin_add || value == :builtin_subtract || 
      value == :builtin_multiply || value == :builtin_divide
    end
    
    def call_function_like(func, args)
      case func
      when UserFunction
        call_user_function(func, args)
      when PartialFunction
        call_partial_function(func, args)
      when :builtin_add
        if args.length >= 2
          evaluate_binary_operation('+', args[0], args[1])
        else
          raise EvaluationError, "Wrong number of arguments for +"
        end
      when :builtin_subtract
        if args.length >= 2
          evaluate_binary_operation('-', args[0], args[1])
        else
          raise EvaluationError, "Wrong number of arguments for -"
        end
      when :builtin_multiply
        if args.length >= 2
          evaluate_binary_operation('*', args[0], args[1])
        else
          raise EvaluationError, "Wrong number of arguments for *"
        end
      when :builtin_divide
        if args.length >= 2
          evaluate_binary_operation('/', args[0], args[1])
        else
          raise EvaluationError, "Wrong number of arguments for /"
        end
      when :builtin_size
        if args.length >= 1
          evaluate_size(args[0])
        else
          raise EvaluationError, "Wrong number of arguments for size"
        end
      when :builtin_first
        if args.length >= 1
          evaluate_first(args[0])
        else
          raise EvaluationError, "Wrong number of arguments for first"
        end
      when :builtin_rest
        if args.length >= 1
          evaluate_rest(args[0])
        else
          raise EvaluationError, "Wrong number of arguments for rest"
        end
      when :builtin_push
        if args.length >= 2
          evaluate_push(args[0], args[1])
        else
          raise EvaluationError, "Wrong number of arguments for push"
        end
      when :builtin_assoc
        if args.length >= 3
          evaluate_assoc(args[0], args[1], args[2])
        else
          raise EvaluationError, "Wrong number of arguments for assoc"
        end
      else
        raise EvaluationError, "Cannot call #{type_name(func)}"
      end
    end
    
    def create_builtin_function(type, arity)
      # Create a mock function for partial application
      UserFunction.new([], nil, nil).tap do |f|
        f.instance_variable_set(:@builtin_type, type)
        f.instance_variable_set(:@builtin_arity, arity)
      end
    end
    
    def create_operator_function(op)
      # Create a user function that represents an operator
      UserFunction.new([], nil, nil).tap do |f|
        f.instance_variable_set(:@operator, op)
      end
    end
    
    def create_builtin_wrapper(name)
      # Create a user function that represents a builtin
      UserFunction.new([], nil, nil).tap do |f|
        f.instance_variable_set(:@builtin_name, name)
      end
    end
    
    def create_composed_function(functions)
      # Creates a function that applies all functions in sequence: f >> g >> h becomes x -> h(g(f(x)))
      UserFunction.new([], nil, nil).tap do |f|
        f.instance_variable_set(:@composed_functions, functions)
      end
    end
    
    def call_threaded_function(func, value)
      # Threading passes the value to complete partial applications or as first arg for regular functions
      case func
      when PartialFunction
        # For partial functions, add the threaded value as the next argument
        combined_args = func.bound_args + [value]
        
        # Special case for assoc: it expects (key, value, dictionary), so when threading
        # we need to pass the threaded value as the dictionary (3rd arg)
        if func.function.instance_variable_get(:@builtin_name) == :assoc
          # Rearrange args: assoc(key, value) + threaded_dict -> assoc(key, value, dict)
          if func.bound_args.length == 2
            combined_args = func.bound_args + [value]
            return evaluate_assoc(combined_args[0], combined_args[1], combined_args[2])
          end
        end
        
        call_user_function(func.function, combined_args)
      else
        # For regular functions, thread as first argument  
        call_function_like(func, [value])
      end
    end
  end
end