#!/usr/bin/env python3
"""
Elf-lang evaluator implementation.
"""

from typing import Dict, Any, List, Union, Optional
from parser import Parser
from decimal import Decimal, getcontext

# Set decimal precision for proper arithmetic
getcontext().prec = 50


class ElfValue:
    """Base class for elf-lang values."""
    pass


class ElfInteger(ElfValue):
    def __init__(self, value: int):
        self.value = value
    
    def __repr__(self):
        return str(self.value)


class ElfDecimal(ElfValue):
    def __init__(self, value: Union[float, Decimal, str]):
        if isinstance(value, str):
            # Remove underscores for normalization
            value = value.replace('_', '')
        self.value = Decimal(str(value))
    
    def __repr__(self):
        # If the decimal is a whole number, display as integer
        if self.value % 1 == 0:
            return str(int(self.value))
        # Otherwise, return decimal without trailing zeros
        return str(self.value).rstrip('0').rstrip('.')


class ElfString(ElfValue):
    def __init__(self, value: str):
        # Handle string escapes during parsing - value comes pre-unescaped from parser
        self.value = value
    
    def __repr__(self):
        # Return string with quotes for puts output - don't escape internal quotes
        escaped = self.value.replace('\\', '\\\\').replace('\n', '\\n').replace('\t', '\\t')
        return f'"{escaped}"'


class ElfBoolean(ElfValue):
    def __init__(self, value: bool):
        self.value = value
    
    def __repr__(self):
        return "true" if self.value else "false"


class ElfNil(ElfValue):
    def __repr__(self):
        return "nil"


class ElfList(ElfValue):
    def __init__(self, items: List[ElfValue]):
        self.items = items
    
    def __repr__(self):
        items_repr = [str(item) for item in self.items]
        return f"[{', '.join(items_repr)}]"


class ElfSet(ElfValue):
    def __init__(self, items: List[ElfValue]):
        # Sets should maintain unique values and sort for deterministic output
        self.items = items
    
    def __repr__(self):
        # Sort items for deterministic output (ascending order by value)
        sorted_items = sorted(self.items, key=lambda x: self._sort_key(x))
        items_repr = [str(item) for item in sorted_items]
        return f"{{{', '.join(items_repr)}}}"
    
    def _sort_key(self, value):
        """Get sort key for value."""
        if isinstance(value, ElfInteger):
            return (0, value.value)
        elif isinstance(value, ElfDecimal):
            return (1, float(value.value))
        elif isinstance(value, ElfString):
            return (2, value.value)
        elif isinstance(value, ElfBoolean):
            return (3, value.value)
        elif isinstance(value, ElfNil):
            return (4, 0)
        else:
            return (5, str(value))


class ElfDictionary(ElfValue):
    def __init__(self, items: List[tuple]):
        self.items = items  # List of (key, value) pairs
    
    def __repr__(self):
        # Sort pairs by key for deterministic output (ascending order by key)
        sorted_pairs = sorted(self.items, key=lambda x: self._sort_key(x[0]))
        pairs = [f"{key}: {value}" for key, value in sorted_pairs]
        return f"#{{{', '.join(pairs)}}}"
    
    def _sort_key(self, value):
        """Get sort key for value."""
        if isinstance(value, ElfInteger):
            return (0, value.value)
        elif isinstance(value, ElfDecimal):
            return (1, float(value.value))
        elif isinstance(value, ElfString):
            return (2, value.value)
        elif isinstance(value, ElfBoolean):
            return (3, value.value)
        elif isinstance(value, ElfNil):
            return (4, 0)
        else:
            return (5, str(value))


class RuntimeError(Exception):
    """Runtime errors in elf-lang execution."""
    pass


class Environment:
    """Variable environment with scoping."""
    def __init__(self, parent: Optional['Environment'] = None):
        self.parent = parent
        self.variables: Dict[str, ElfValue] = {}
        self.mutable: Dict[str, bool] = {}
    
    def define(self, name: str, value: ElfValue, mutable: bool = False):
        """Define a new variable."""
        self.variables[name] = value
        self.mutable[name] = mutable
    
    def get(self, name: str) -> ElfValue:
        """Get a variable's value."""
        if name in self.variables:
            return self.variables[name]
        elif self.parent:
            return self.parent.get(name)
        else:
            raise RuntimeError(f"Identifier can not be found: {name}")
    
    def assign(self, name: str, value: ElfValue):
        """Assign to an existing variable."""
        if name in self.variables:
            if not self.mutable[name]:
                raise RuntimeError(f"Variable '{name}' is not mutable")
            self.variables[name] = value
        elif self.parent:
            self.parent.assign(name, value)
        else:
            raise RuntimeError(f"Identifier can not be found: {name}")


class ElfFunction(ElfValue):
    def __init__(self, parameters: List[str], body: Dict[str, Any], closure_env: Union[Dict[str, ElfValue], Environment]):
        self.parameters = parameters
        self.body = body
        self.closure_env = closure_env
    
    def __repr__(self):
        return f"|{', '.join(self.parameters)}| {{ [closure] }}"


class Evaluator:
    """Elf-lang evaluator."""
    
    def __init__(self):
        self.global_env = Environment()
        self.output_buffer = []
        
        # Built-in functions
        self.global_env.define("puts", ElfBuiltinFunction("puts", self.builtin_puts, arity=-1))  # -1 indicates variadic
        
        # Collection functions
        self.global_env.define("push", ElfBuiltinFunction("push", self.builtin_push))
        self.global_env.define("assoc", ElfBuiltinFunction("assoc", self.builtin_assoc))
        self.global_env.define("first", ElfBuiltinFunction("first", self.builtin_first))
        self.global_env.define("rest", ElfBuiltinFunction("rest", self.builtin_rest))
        self.global_env.define("size", ElfBuiltinFunction("size", self.builtin_size))
        
        # Higher-order functions
        self.global_env.define("map", ElfBuiltinFunction("map", self.builtin_map))
        self.global_env.define("filter", ElfBuiltinFunction("filter", self.builtin_filter))
        self.global_env.define("fold", ElfBuiltinFunction("fold", self.builtin_fold))
        
        # Operator functions
        self.global_env.define("+", ElfBuiltinFunction("+", self.builtin_add))
        self.global_env.define("-", ElfBuiltinFunction("-", self.builtin_subtract))
        self.global_env.define("*", ElfBuiltinFunction("*", self.builtin_multiply))
        self.global_env.define("/", ElfBuiltinFunction("/", self.builtin_divide))
    
    def builtin_puts(self, *args: ElfValue) -> ElfValue:
        """Built-in puts function."""
        for arg in args:
            self.output_buffer.append(f"{arg} ")
        self.output_buffer.append("\n")
        return ElfNil()
    
    def builtin_add(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Built-in + function."""
        return self.add_values(left, right)
    
    def builtin_subtract(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Built-in - function."""
        return self.subtract_values(left, right)
    
    def builtin_multiply(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Built-in * function."""
        return self.multiply_values(left, right)
    
    def builtin_divide(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Built-in / function."""
        return self.divide_values(left, right)
    
    def builtin_push(self, item: ElfValue, collection: ElfValue) -> ElfValue:
        """Built-in push function - returns new collection with item added."""
        if isinstance(collection, ElfList):
            # Return new list with item appended
            new_items = collection.items + [item]
            return ElfList(new_items)
        elif isinstance(collection, ElfSet):
            # Return new set with item added (avoiding duplicates)
            new_items = collection.items[:]
            # Check if item already exists (structural equality)
            for existing in new_items:
                if self.values_equal(existing, item):
                    return ElfSet(new_items)  # Item already exists, return unchanged
            new_items.append(item)
            return ElfSet(new_items)
        else:
            collection_type = type(collection).__name__[3:]  # Remove 'Elf' prefix
            raise RuntimeError(f"Cannot push to {collection_type}")
    
    def builtin_assoc(self, key: ElfValue, value: ElfValue, dictionary: ElfValue) -> ElfValue:
        """Built-in assoc function - returns new dictionary with key/value set."""
        if not isinstance(dictionary, ElfDictionary):
            dict_type = type(dictionary).__name__[3:]  # Remove 'Elf' prefix
            raise RuntimeError(f"Cannot assoc to {dict_type}")
        
        # Create new dictionary with key/value updated
        new_pairs = []
        key_found = False
        
        for existing_key, existing_value in dictionary.items:
            if self.values_equal(existing_key, key):
                new_pairs.append((key, value))  # Update existing key
                key_found = True
            else:
                new_pairs.append((existing_key, existing_value))
        
        if not key_found:
            new_pairs.append((key, value))  # Add new key
        
        return ElfDictionary(new_pairs)
    
    def builtin_first(self, collection: ElfValue) -> ElfValue:
        """Built-in first function - returns first element or nil."""
        if isinstance(collection, ElfList):
            return collection.items[0] if collection.items else ElfNil()
        elif isinstance(collection, ElfString):
            return ElfString(collection.value[0]) if collection.value else ElfNil()
        elif isinstance(collection, ElfSet):
            return collection.items[0] if collection.items else ElfNil()
        elif isinstance(collection, ElfDictionary):
            if collection.items:
                key, value = collection.items[0]
                return key
            return ElfNil()
        else:
            return ElfNil()
    
    def builtin_rest(self, collection: ElfValue) -> ElfValue:
        """Built-in rest function - returns collection without first element."""
        if isinstance(collection, ElfList):
            return ElfList(collection.items[1:]) if collection.items else ElfList([])
        elif isinstance(collection, ElfString):
            return ElfString(collection.value[1:]) if collection.value else ElfString("")
        elif isinstance(collection, ElfSet):
            return ElfSet(collection.items[1:]) if collection.items else ElfSet([])
        elif isinstance(collection, ElfDictionary):
            return ElfDictionary(collection.items[1:]) if collection.items else ElfDictionary([])
        else:
            return ElfNil()
    
    def builtin_size(self, collection: ElfValue) -> ElfValue:
        """Built-in size function - returns size of collection."""
        if isinstance(collection, (ElfList, ElfSet, ElfDictionary)):
            return ElfInteger(len(collection.items))
        elif isinstance(collection, ElfString):
            # Count UTF-8 bytes for strings
            return ElfInteger(len(collection.value.encode('utf-8')))
        else:
            return ElfNil()
    
    def builtin_map(self, func: ElfValue, lst: ElfValue) -> ElfValue:
        """Built-in map function - applies function to each element."""
        if not isinstance(lst, ElfList):
            raise RuntimeError(f"Unexpected argument: map({type(func).__name__.replace('Elf', '')}, {type(lst).__name__.replace('Elf', '')})")
        
        if not isinstance(func, (ElfFunction, ElfBuiltinFunction)):
            raise RuntimeError(f"Unexpected argument: map({type(func).__name__.replace('Elf', '')}, List)")
        
        # Apply function to each element
        result_items = []
        for item in lst.items:
            if isinstance(func, ElfBuiltinFunction):
                mapped_value = func.call(item)
            else:
                mapped_value = self.call_user_function(func, [item], self.global_env)
            result_items.append(mapped_value)
        
        return ElfList(result_items)
    
    def builtin_filter(self, predicate: ElfValue, lst: ElfValue) -> ElfValue:
        """Built-in filter function - keeps elements where predicate returns true."""
        if not isinstance(lst, ElfList):
            raise RuntimeError(f"Unexpected argument: filter({type(predicate).__name__.replace('Elf', '')}, {type(lst).__name__.replace('Elf', '')})")
        
        if not isinstance(predicate, (ElfFunction, ElfBuiltinFunction)):
            raise RuntimeError(f"Unexpected argument: filter({type(predicate).__name__.replace('Elf', '')}, List)")
        
        # Filter elements based on predicate
        result_items = []
        for item in lst.items:
            if isinstance(predicate, ElfBuiltinFunction):
                result = predicate.call(item)
            else:
                result = self.call_user_function(predicate, [item], self.global_env)
            
            # Check if result is truthy
            if self.is_truthy(result):
                result_items.append(item)
        
        return ElfList(result_items)
    
    def builtin_fold(self, initial: ElfValue, func: ElfValue, lst: ElfValue) -> ElfValue:
        """Built-in fold function - reduces list to single value."""
        if not isinstance(lst, ElfList):
            raise RuntimeError(f"Unexpected argument: fold({type(initial).__name__.replace('Elf', '')}, {type(func).__name__.replace('Elf', '')}, {type(lst).__name__.replace('Elf', '')})")
        
        if not isinstance(func, (ElfFunction, ElfBuiltinFunction)):
            raise RuntimeError(f"Unexpected argument: fold({type(initial).__name__.replace('Elf', '')}, {type(func).__name__.replace('Elf', '')}, List)")
        
        # Fold over the list
        accumulator = initial
        for item in lst.items:
            if isinstance(func, ElfBuiltinFunction):
                accumulator = func.call(accumulator, item)
            else:
                accumulator = self.call_user_function(func, [accumulator, item], self.global_env)
        
        return accumulator
    
    def is_truthy(self, value: ElfValue) -> bool:
        """Check if a value is truthy."""
        if isinstance(value, ElfBoolean):
            return value.value
        elif isinstance(value, ElfNil):
            return False
        elif isinstance(value, ElfInteger):
            return value.value != 0
        elif isinstance(value, ElfDecimal):
            return float(value.value) != 0.0
        elif isinstance(value, ElfString):
            return len(value.value) > 0
        elif isinstance(value, (ElfList, ElfSet, ElfDictionary)):
            return len(value.items) > 0
        else:
            return True
    
    def compose_functions(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Compose two functions: (f >> g)(x) = g(f(x))."""
        if not isinstance(left, (ElfFunction, ElfBuiltinFunction)):
            raise RuntimeError(f"Cannot compose non-function: {type(left).__name__.replace('Elf', '')}")
        if not isinstance(right, (ElfFunction, ElfBuiltinFunction)):
            raise RuntimeError(f"Cannot compose non-function: {type(right).__name__.replace('Elf', '')}")
        
        # Create a new function that composes left and right
        def composed_call(*args):
            # Apply left function first
            if isinstance(left, ElfBuiltinFunction):
                intermediate = left.call(*args)
            else:
                intermediate = self.call_user_function(left, list(args), self.global_env)
            
            # Apply right function to the result
            if isinstance(right, ElfBuiltinFunction):
                return right.call(intermediate)
            else:
                return self.call_user_function(right, [intermediate], self.global_env)
        
        # Return as a built-in function for simplicity
        return ElfBuiltinFunction(f"({left}>>{right})", composed_call, arity=1)
    
    def thread_value(self, value: ElfValue, func: ElfValue) -> ElfValue:
        """Thread a value through a function: value |> func = func(value)."""
        if not isinstance(func, (ElfFunction, ElfBuiltinFunction)):
            raise RuntimeError(f"Cannot thread into non-function: {type(func).__name__.replace('Elf', '')}")
        
        # Apply the function to the value
        if isinstance(func, ElfBuiltinFunction):
            return func.call(value)
        else:
            return self.call_user_function(func, [value], self.global_env)
    
    def evaluate_program(self, ast: Dict[str, Any]) -> str:
        """Evaluate a complete program."""
        self.output_buffer = []
        last_result = ElfNil()
        
        try:
            for statement in ast["statements"]:
                result = self.evaluate_statement(statement, self.global_env)
                # Only update last_result if it's not a comment
                if statement["type"] != "Comment":
                    last_result = result
            
            # Add the final result to output
            self.output_buffer.append(str(last_result))
            
        except RuntimeError as e:
            self.output_buffer.append(f"[Error] {str(e)}")
            return ''.join(self.output_buffer)
        
        return ''.join(self.output_buffer)
    
    def evaluate_statement(self, stmt: Dict[str, Any], env: Environment) -> ElfValue:
        """Evaluate a statement."""
        if stmt["type"] == "Expression":
            return self.evaluate_expression(stmt["value"], env)
        elif stmt["type"] == "Comment":
            # Comments are ignored during evaluation
            return ElfNil()
        else:
            raise RuntimeError(f"Unknown statement type: {stmt['type']}")
    
    def evaluate_expression(self, expr: Dict[str, Any], env: Environment) -> ElfValue:
        """Evaluate an expression."""
        expr_type = expr["type"]
        
        # Literals
        if expr_type == "Integer":
            # Remove underscores for normalization
            value_str = expr["value"].replace('_', '')
            return ElfInteger(int(value_str))
        
        elif expr_type == "Decimal":
            return ElfDecimal(expr["value"])
        
        elif expr_type == "String":
            return ElfString(expr["value"])
        
        elif expr_type == "Boolean":
            return ElfBoolean(expr["value"])
        
        elif expr_type == "Nil":
            return ElfNil()
        
        # Identifiers
        elif expr_type == "Identifier":
            return env.get(expr["name"])
        
        # Variable declarations
        elif expr_type == "Let":
            value = self.evaluate_expression(expr["value"], env)
            env.define(expr["name"]["name"], value, mutable=False)
            return value
        
        elif expr_type == "MutableLet":
            value = self.evaluate_expression(expr["value"], env)
            env.define(expr["name"]["name"], value, mutable=True)
            return value
        
        # Function calls
        elif expr_type == "Call":
            func = self.evaluate_expression(expr["function"], env)
            args = [self.evaluate_expression(arg, env) for arg in expr["arguments"]]
            
            if isinstance(func, ElfBuiltinFunction):
                return func.call(*args)
            elif isinstance(func, ElfFunction):
                return self.call_user_function(func, args, env)
            else:
                raise RuntimeError(f"Expected a Function, found: {type(func).__name__.replace('Elf', '')}")
        
        # Infix operations
        elif expr_type == "Infix":
            return self.evaluate_infix(expr, env)
        
        # Prefix/unary operations
        elif expr_type == "Prefix":
            return self.evaluate_prefix(expr, env)
        
        # Assignment operations
        elif expr_type == "Assignment":
            value = self.evaluate_expression(expr["value"], env)
            env.assign(expr["name"]["name"], value)
            return value
        
        # Collection literals
        elif expr_type == "List":
            items = [self.evaluate_expression(item, env) for item in expr["items"]]
            return ElfList(items)
        
        elif expr_type == "Set":
            items = [self.evaluate_expression(item, env) for item in expr["items"]]
            # Check for dictionary in set (error case)
            for item in items:
                if isinstance(item, ElfDictionary):
                    raise RuntimeError("Unable to include a Dictionary within a Set")
            return ElfSet(items)
        
        elif expr_type == "Dictionary":
            pairs = []
            for pair in expr["items"]:
                key = self.evaluate_expression(pair["key"], env)
                value = self.evaluate_expression(pair["value"], env)
                # Check for dictionary as key (error case)
                if isinstance(key, ElfDictionary):
                    raise RuntimeError("Unable to use a Dictionary as a Dictionary key")
                pairs.append((key, value))
            return ElfDictionary(pairs)
        
        # Function literals
        elif expr_type == "Function":
            # Extract parameter names from AST
            param_names = [param["name"] for param in expr["parameters"]]
            # Store reference to the environment where function was defined (for closure)
            return ElfFunction(param_names, expr["body"], env)
        
        # If expressions
        elif expr_type == "If":
            condition = self.evaluate_expression(expr["condition"], env)
            if self.is_truthy(condition):
                return self.evaluate_block(expr["consequence"], env)
            elif expr["alternative"]:
                return self.evaluate_block(expr["alternative"], env)
            else:
                return ElfNil()
        
        # Indexing operations
        elif expr_type == "Index":
            target = self.evaluate_expression(expr["left"], env)
            index = self.evaluate_expression(expr["index"], env)
            return self.index_value(target, index)
        
        # Function composition and threading
        elif expr_type == "FunctionComposition":
            # Compose all functions: f >> g >> h becomes h(g(f(x)))
            functions = [self.evaluate_expression(func, env) for func in expr["functions"]]
            if len(functions) < 2:
                raise RuntimeError("Function composition requires at least 2 functions")
            
            # Start with the first function and compose with the rest
            result = functions[0]
            for func in functions[1:]:
                result = self.compose_functions(result, func)
            return result
        
        elif expr_type == "FunctionThread":
            # Thread initial value through all functions: x |> f |> g becomes g(f(x))
            value = self.evaluate_expression(expr["initial"], env)
            functions = [self.evaluate_expression(func, env) for func in expr["functions"]]
            
            for func in functions:
                value = self.thread_value(value, func)
            
            return value
        
        else:
            raise RuntimeError(f"Unknown expression type: {expr_type}")
    
    def evaluate_infix(self, expr: Dict[str, Any], env: Environment) -> ElfValue:
        """Evaluate infix operations."""
        operator = expr["operator"]
        left = self.evaluate_expression(expr["left"], env)
        right = self.evaluate_expression(expr["right"], env)
        
        # Arithmetic operations
        if operator == "+":
            return self.add_values(left, right)
        elif operator == "-":
            return self.subtract_values(left, right)
        elif operator == "*":
            return self.multiply_values(left, right)
        elif operator == "/":
            return self.divide_values(left, right)
        
        # Comparison operations
        elif operator == "==":
            return ElfBoolean(self.values_equal(left, right))
        elif operator == "!=":
            return ElfBoolean(not self.values_equal(left, right))
        elif operator == ">":
            return ElfBoolean(self.compare_values(left, right) > 0)
        elif operator == "<":
            return ElfBoolean(self.compare_values(left, right) < 0)
        elif operator == ">=":
            return ElfBoolean(self.compare_values(left, right) >= 0)
        elif operator == "<=":
            return ElfBoolean(self.compare_values(left, right) <= 0)
        
        # Logical operations
        elif operator == "&&":
            return ElfBoolean(self.is_truthy(left) and self.is_truthy(right))
        elif operator == "||":
            return ElfBoolean(self.is_truthy(left) or self.is_truthy(right))
        
        # Function composition and threading
        elif operator == ">>":
            return self.compose_functions(left, right)
        elif operator == "|>":
            return self.thread_value(left, right)
        
        else:
            raise RuntimeError(f"Unknown operator: {operator}")
    
    def evaluate_prefix(self, expr: Dict[str, Any], env: Environment) -> ElfValue:
        """Evaluate prefix operations."""
        operator = expr["operator"]
        operand = self.evaluate_expression(expr["operand"], env)
        
        if operator == "-":
            if isinstance(operand, ElfInteger):
                return ElfInteger(-operand.value)
            elif isinstance(operand, ElfDecimal):
                return ElfDecimal(-operand.value)
            else:
                raise RuntimeError(f"Unsupported operation: -{type(operand).__name__[3:]}")
        else:
            raise RuntimeError(f"Unknown prefix operator: {operator}")
    
    def add_values(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Add two values."""
        if isinstance(left, ElfInteger) and isinstance(right, ElfInteger):
            return ElfInteger(left.value + right.value)
        elif isinstance(left, (ElfInteger, ElfDecimal)) and isinstance(right, (ElfInteger, ElfDecimal)):
            # Use float arithmetic to match expected precision behavior
            left_val = float(left.value)
            right_val = float(right.value)
            return ElfDecimal(left_val + right_val)
        elif isinstance(left, ElfString) and isinstance(right, ElfString):
            return ElfString(left.value + right.value)
        elif isinstance(left, ElfString):
            # String concatenation with other types
            if isinstance(right, ElfInteger):
                return ElfString(left.value + str(right.value))
            elif isinstance(right, ElfDecimal):
                return ElfString(left.value + str(right.value))
            else:
                return ElfString(left.value + str(right))
        elif isinstance(right, ElfString):
            # Other types concatenated with string
            if isinstance(left, ElfInteger):
                return ElfString(str(left.value) + right.value)
            elif isinstance(left, ElfDecimal):
                return ElfString(str(left.value) + right.value)
            else:
                return ElfString(str(left) + right.value)
        # Collection operations
        elif isinstance(left, ElfList) and isinstance(right, ElfList):
            # List concatenation
            return ElfList(left.items + right.items)
        elif isinstance(left, ElfList) and isinstance(right, ElfInteger):
            # List + Integer is an error
            raise RuntimeError("Unsupported operation: List + Integer")
        elif isinstance(left, ElfSet) and isinstance(right, ElfSet):
            # Set union
            result_items = left.items[:]
            for item in right.items:
                # Add if not already present (structural equality)
                found = False
                for existing in result_items:
                    if self.values_equal(existing, item):
                        found = True
                        break
                if not found:
                    result_items.append(item)
            return ElfSet(result_items)
        elif isinstance(left, ElfSet) and isinstance(right, ElfInteger):
            # Set + Integer is an error
            raise RuntimeError("Unsupported operation: Set + Integer")
        elif isinstance(left, ElfDictionary) and isinstance(right, ElfDictionary):
            # Dictionary merge (right-biased)
            result_pairs = left.items[:]
            for right_key, right_value in right.items:
                # Update existing key or add new key
                found = False
                for i, (left_key, left_value) in enumerate(result_pairs):
                    if self.values_equal(left_key, right_key):
                        result_pairs[i] = (right_key, right_value)  # Right-biased update
                        found = True
                        break
                if not found:
                    result_pairs.append((right_key, right_value))
            return ElfDictionary(result_pairs)
        else:
            raise RuntimeError(f"Unsupported operation: {type(left).__name__[3:]} + {type(right).__name__[3:]}")
    
    def subtract_values(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Subtract two values."""
        if isinstance(left, ElfInteger) and isinstance(right, ElfInteger):
            return ElfInteger(left.value - right.value)
        elif isinstance(left, (ElfInteger, ElfDecimal)) and isinstance(right, (ElfInteger, ElfDecimal)):
            # Use float arithmetic to match expected precision behavior
            left_val = float(left.value)
            right_val = float(right.value)
            return ElfDecimal(left_val - right_val)
        else:
            raise RuntimeError(f"Unsupported operation: {type(left).__name__[3:]} - {type(right).__name__[3:]}")
    
    def multiply_values(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Multiply two values."""
        if isinstance(left, ElfInteger) and isinstance(right, ElfInteger):
            return ElfInteger(left.value * right.value)
        elif isinstance(left, (ElfInteger, ElfDecimal)) and isinstance(right, (ElfInteger, ElfDecimal)):
            # Use float arithmetic to match expected precision behavior
            left_val = float(left.value)
            right_val = float(right.value)
            return ElfDecimal(left_val * right_val)
        elif isinstance(left, ElfString) and isinstance(right, ElfInteger):
            if right.value < 0:
                raise RuntimeError("Unsupported operation: String * Integer (< 0)")
            return ElfString(left.value * right.value)
        elif isinstance(left, ElfString) and isinstance(right, ElfDecimal):
            raise RuntimeError("Unsupported operation: String * Decimal")
        else:
            raise RuntimeError(f"Unsupported operation: {type(left).__name__[3:]} * {type(right).__name__[3:]}")
    
    def divide_values(self, left: ElfValue, right: ElfValue) -> ElfValue:
        """Divide two values."""
        if isinstance(left, ElfInteger) and isinstance(right, ElfInteger):
            if right.value == 0:
                raise RuntimeError("Division by zero")
            # Integer division truncates toward zero (not floor division)
            result = int(left.value / right.value)
            return ElfInteger(result)
        elif isinstance(left, (ElfInteger, ElfDecimal)) and isinstance(right, (ElfInteger, ElfDecimal)):
            # Use float arithmetic to match expected precision behavior
            left_val = float(left.value)
            right_val = float(right.value)
            if right_val == 0:
                raise RuntimeError("Division by zero")
            return ElfDecimal(left_val / right_val)
        else:
            raise RuntimeError(f"Unsupported operation: {type(left).__name__[3:]} / {type(right).__name__[3:]}")
    
    def values_equal(self, left: ElfValue, right: ElfValue) -> bool:
        """Check if two values are equal with structural equality."""
        if type(left) != type(right):
            return False
        
        if isinstance(left, (ElfInteger, ElfDecimal, ElfString, ElfBoolean)):
            return left.value == right.value
        elif isinstance(left, ElfNil):
            return True  # All nil values are equal
        elif isinstance(left, ElfList):
            if len(left.items) != len(right.items):
                return False
            return all(self.values_equal(l, r) for l, r in zip(left.items, right.items))
        elif isinstance(left, ElfSet):
            if len(left.items) != len(right.items):
                return False
            # Sets should be equal regardless of order
            for left_item in left.items:
                found = False
                for right_item in right.items:
                    if self.values_equal(left_item, right_item):
                        found = True
                        break
                if not found:
                    return False
            return True
        elif isinstance(left, ElfDictionary):
            if len(left.items) != len(right.items):
                return False
            # Dictionaries should be equal regardless of order
            for left_key, left_value in left.items:
                found = False
                for right_key, right_value in right.items:
                    if self.values_equal(left_key, right_key) and self.values_equal(left_value, right_value):
                        found = True
                        break
                if not found:
                    return False
            return True
        else:
            return False  # Functions and other complex types
    
    def compare_values(self, left: ElfValue, right: ElfValue) -> int:
        """Compare two values. Returns -1, 0, or 1."""
        if isinstance(left, ElfInteger) and isinstance(right, ElfInteger):
            return (left.value > right.value) - (left.value < right.value)
        elif isinstance(left, (ElfInteger, ElfDecimal)) and isinstance(right, (ElfInteger, ElfDecimal)):
            left_val = Decimal(left.value) if isinstance(left, ElfInteger) else left.value
            right_val = Decimal(right.value) if isinstance(right, ElfInteger) else right.value
            return (left_val > right_val) - (left_val < right_val)
        elif isinstance(left, ElfString) and isinstance(right, ElfString):
            return (left.value > right.value) - (left.value < right.value)
        else:
            raise RuntimeError(f"Cannot compare {type(left).__name__[3:]} with {type(right).__name__[3:]}")
    
    def is_truthy(self, value: ElfValue) -> bool:
        """Check if a value is truthy."""
        if isinstance(value, ElfBoolean):
            return value.value
        elif isinstance(value, ElfNil):
            return False
        elif isinstance(value, ElfInteger):
            return value.value != 0
        elif isinstance(value, ElfDecimal):
            return value.value != 0
        elif isinstance(value, ElfString):
            return value.value != ""
        elif isinstance(value, (ElfList, ElfSet, ElfDictionary)):
            return len(value.items) > 0
        else:
            return True  # Functions and other values are truthy
    
    def index_value(self, target: ElfValue, index: ElfValue) -> ElfValue:
        """Index into a string, list, or dictionary."""
        if isinstance(target, ElfString):
            if not isinstance(index, ElfInteger):
                index_type = type(index).__name__[3:]  # Remove 'Elf' prefix
                raise RuntimeError(f"Unable to perform index operation, found: String[{index_type}]")
            
            string_val = target.value
            idx = index.value
            
            # Handle negative indices
            if idx < 0:
                idx = len(string_val) + idx
            
            # Return character or nil if out of bounds
            if 0 <= idx < len(string_val):
                return ElfString(string_val[idx])
            else:
                return ElfNil()
        
        elif isinstance(target, ElfList):
            if not isinstance(index, ElfInteger):
                index_type = type(index).__name__[3:]  # Remove 'Elf' prefix
                raise RuntimeError(f"Unable to perform index operation, found: List[{index_type}]")
            
            items = target.items
            idx = index.value
            
            # Handle negative indices
            if idx < 0:
                idx = len(items) + idx
            
            # Return item or nil if out of bounds
            if 0 <= idx < len(items):
                return items[idx]
            else:
                return ElfNil()
        
        elif isinstance(target, ElfDictionary):
            # Dictionary indexing - find matching key
            for key, value in target.items:
                if self.values_equal(key, index):
                    return value
            return ElfNil()
        
        else:
            target_type = type(target).__name__[3:]  # Remove 'Elf' prefix
            raise RuntimeError(f"Cannot index into {target_type}")
    
    def call_user_function(self, func: ElfFunction, args: List[ElfValue], env: Environment) -> ElfValue:
        """Call a user-defined function."""
        # Support partial application - if not enough args, return partially applied function
        if len(args) < len(func.parameters):
            # Create a new function with remaining parameters
            remaining_params = func.parameters[len(args):]
            # For partial application, we need to create a closure with bound arguments
            if isinstance(func.closure_env, Environment):
                # TODO: Handle partial application with Environment closure
                partial_closure = {}
                for i, arg in enumerate(args):
                    partial_closure[func.parameters[i]] = arg
                return ElfFunction(remaining_params, func.body, partial_closure)
            else:
                closure_env = func.closure_env.copy()
                for i, arg in enumerate(args):
                    closure_env[func.parameters[i]] = arg
                return ElfFunction(remaining_params, func.body, closure_env)
        
        # Create new environment for function execution
        if isinstance(func.closure_env, Environment):
            # Use the closure environment as parent (lexical scoping)
            func_env = Environment(parent=func.closure_env)
        else:
            # Legacy dict-based closure - use current env as parent
            func_env = Environment(parent=env)
            # Add closure variables first (they have priority)
            for name, value in func.closure_env.items():
                func_env.define(name, value, mutable=False)
        
        # Bind parameters to arguments (take only what we need, ignore extras)
        for i, param_name in enumerate(func.parameters):
            if i < len(args):
                func_env.define(param_name, args[i], mutable=False)
        
        # Execute function body (which is a Block)
        return self.evaluate_block(func.body, func_env)
    
    def evaluate_block(self, block: Dict[str, Any], env: Environment) -> ElfValue:
        """Evaluate a block, returning the last expression's value."""
        result = ElfNil()
        for stmt in block["statements"]:
            result = self.evaluate_statement(stmt, env)
        return result


class ElfBuiltinFunction(ElfValue):
    """Built-in function wrapper."""
    def __init__(self, name: str, func, arity: int = None, partial_args: List[ElfValue] = None):
        self.name = name
        self.func = func
        self.arity = arity or func.__code__.co_argcount - 1  # -1 for 'self' parameter
        self.partial_args = partial_args or []
    
    def call(self, *args: ElfValue) -> ElfValue:
        # Combine partial args with new args
        all_args = self.partial_args + list(args)
        
        # Handle variadic functions (arity = -1)
        if self.arity == -1:
            return self.func(*all_args)
        
        if len(all_args) < self.arity:
            # Return partially applied function
            return ElfBuiltinFunction(self.name, self.func, self.arity, all_args)
        elif len(all_args) >= self.arity:
            # Call function with required arguments (ignore extras)
            return self.func(*all_args[:self.arity])
        
        return self.func(*all_args)
    
    def __repr__(self):
        if self.partial_args:
            return f"<partial {self.name}({len(self.partial_args)}/{self.arity})>"
        return f"<builtin {self.name}>"


def evaluate_program(source: str) -> str:
    """Evaluate a program from source code."""
    try:
        parser = Parser(source)
        ast = parser.parse()
        evaluator = Evaluator()
        return evaluator.evaluate_program(ast)
    except Exception as e:
        return str(e)