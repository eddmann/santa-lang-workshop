package elflang

import scala.collection.mutable
import scala.collection.mutable.ListBuffer

sealed trait Value
case class IntValue(value: Long) extends Value
case class DecValue(value: Double) extends Value
case class StrValue(value: String) extends Value
case class BoolValue(value: Boolean) extends Value
case object NilValue extends Value
case class ListValue(items: List[Value]) extends Value
case class SetValue(items: Set[Value]) extends Value
case class DictValue(entries: Map[Value, Value]) extends Value
case class FunctionValue(params: List[String], body: ujson.Value, closure: Environment) extends Value
case class BuiltinFunction(name: String, fn: (List[Value], Environment) => Value) extends Value
case class ComposedFunction(functions: List[Value]) extends Value
case class PartialBuiltin(name: String, args: List[Value]) extends Value

class RuntimeError(message: String) extends Exception(message)

class Environment(parent: Option[Environment] = None):
  private val bindings = mutable.HashMap[String, (Value, Boolean)]() // (value, isMutable)

  def define(name: String, value: Value, isMutable: Boolean): Unit =
    bindings(name) = (value, isMutable)

  def get(name: String): Option[Value] =
    bindings.get(name).map(_._1).orElse(parent.flatMap(_.get(name)))

  def set(name: String, value: Value): Unit =
    bindings.get(name) match
      case Some((_, true)) => bindings(name) = (value, true)
      case Some((_, false)) => throw RuntimeError(s"Variable '$name' is not mutable")
      case None => parent match
        case Some(p) => p.set(name, value)
        case None => throw RuntimeError(s"Identifier can not be found: $name")

  def fork(): Environment = Environment(Some(this))

object Interpreter:
  def interpret(ast: ujson.Value): String =
    val env = createGlobalEnvironment()
    val output = ListBuffer[String]()

    // Don't override puts - let it print directly to stdout
    // This ensures output appears even if an error occurs later

    val result = eval(ast, env, output)

    // Return the final result (puts will have already printed its outputs)
    formatValue(result)

  private def createGlobalEnvironment(): Environment =
    val env = Environment()

    // Built-in functions
    env.define("puts", BuiltinFunction("puts", (args, _) => {
      val formatted = args.map(formatValue).mkString(" ")
      println(formatted + " ")
      NilValue
    }), false)

    // Arithmetic operators as functions
    env.define("+", BuiltinFunction("+", (args, _) => args match {
      case List(IntValue(a), IntValue(b)) => IntValue(a + b)
      case List(DecValue(a), DecValue(b)) => DecValue(a + b)
      case List(IntValue(a), DecValue(b)) => DecValue(a + b)
      case List(DecValue(a), IntValue(b)) => DecValue(a + b)
      case List(StrValue(a), StrValue(b)) => StrValue(a + b)
      // String concatenation with automatic conversion
      case List(StrValue(a), IntValue(b)) => StrValue(a + b.toString)
      case List(IntValue(a), StrValue(b)) => StrValue(a.toString + b)
      case List(StrValue(a), DecValue(b)) => StrValue(a + b.toString)
      case List(DecValue(a), StrValue(b)) => StrValue(a.toString + b)
      case List(StrValue(a), BoolValue(b)) => StrValue(a + b.toString)
      case List(BoolValue(a), StrValue(b)) => StrValue(a.toString + b)
      case List(StrValue(a), NilValue) => StrValue(a + "nil")
      case List(NilValue, StrValue(b)) => StrValue("nil" + b)
      case List(ListValue(a), ListValue(b)) => ListValue(a ++ b)
      case List(SetValue(a), SetValue(b)) => SetValue(a ++ b)
      case List(DictValue(a), DictValue(b)) => DictValue(a ++ b)
      case List(a, b) => throw RuntimeError(s"Unsupported operation: ${typeOf(a)} + ${typeOf(b)}")
      case _ => throw RuntimeError("+ requires exactly 2 arguments")
    }), false)

    env.define("-", BuiltinFunction("-", (args, _) => args match {
      case List(IntValue(a), IntValue(b)) => IntValue(a - b)
      case List(DecValue(a), DecValue(b)) => DecValue(a - b)
      case List(IntValue(a), DecValue(b)) => DecValue(a - b)
      case List(DecValue(a), IntValue(b)) => DecValue(a - b)
      case List(a, b) => throw RuntimeError(s"Unsupported operation: ${typeOf(a)} - ${typeOf(b)}")
      case _ => throw RuntimeError("- requires exactly 2 arguments")
    }), false)

    env.define("*", BuiltinFunction("*", (args, _) => args match {
      case List(IntValue(a), IntValue(b)) => IntValue(a * b)
      case List(DecValue(a), DecValue(b)) => DecValue(a * b)
      case List(IntValue(a), DecValue(b)) => DecValue(a * b)
      case List(DecValue(a), IntValue(b)) => DecValue(a * b)
      case List(StrValue(s), IntValue(n)) if n >= 0 => StrValue(s * n.toInt)
      case List(IntValue(n), StrValue(s)) if n >= 0 => StrValue(s * n.toInt)
      case List(StrValue(_), IntValue(n)) if n < 0 => throw RuntimeError("Unsupported operation: String * Integer (< 0)")
      case List(IntValue(n), StrValue(_)) if n < 0 => throw RuntimeError("Unsupported operation: String * Integer (< 0)")
      case List(StrValue(_), DecValue(_)) => throw RuntimeError("Unsupported operation: String * Decimal")
      case List(DecValue(_), StrValue(_)) => throw RuntimeError("Unsupported operation: String * Decimal")
      case List(a, b) => throw RuntimeError(s"Unsupported operation: ${typeOf(a)} * ${typeOf(b)}")
      case _ => throw RuntimeError("* requires exactly 2 arguments")
    }), false)

    env.define("/", BuiltinFunction("/", (args, _) => args match {
      case List(_, IntValue(0)) => throw RuntimeError("Division by zero")
      case List(_, DecValue(0.0)) => throw RuntimeError("Division by zero")
      case List(IntValue(a), IntValue(b)) => IntValue(a / b) // Truncating division
      case List(DecValue(a), DecValue(b)) => DecValue(a / b)
      case List(IntValue(a), DecValue(b)) => DecValue(a / b)
      case List(DecValue(a), IntValue(b)) => DecValue(a / b)
      case List(a, b) => throw RuntimeError(s"Unsupported operation: ${typeOf(a)} / ${typeOf(b)}")
      case _ => throw RuntimeError("/ requires exactly 2 arguments")
    }), false)

    // Collection functions
    env.define("push", BuiltinFunction("push", (args, _) => args match {
      case List(value, ListValue(items)) => ListValue(items :+ value)
      case List(value, SetValue(items)) => SetValue(items + value)
      case List(_, coll) => throw RuntimeError(s"push(...): invalid argument type, expected List or Set, found ${typeOf(coll)}")
      case _ => throw RuntimeError("push requires exactly 2 arguments")
    }), false)

    env.define("assoc", BuiltinFunction("assoc", (args, _) => args match {
      case List(key, value, DictValue(entries)) =>
        // Check if key is a dictionary (not allowed)
        key match
          case DictValue(_) => throw RuntimeError("Unable to use a Dictionary as a Dictionary key")
          case _ => DictValue(entries + (key -> value))
      case List(_, _, dict) => throw RuntimeError(s"assoc(...): invalid argument type, expected Dictionary, found ${typeOf(dict)}")
      case _ => throw RuntimeError("assoc requires exactly 3 arguments")
    }), false)

    env.define("first", BuiltinFunction("first", (args, _) => args match {
      case List(ListValue(items)) => if items.nonEmpty then items.head else NilValue
      case List(StrValue(s)) => if s.nonEmpty then StrValue(s.head.toString) else NilValue
      case List(coll) => throw RuntimeError(s"first(...): invalid argument type, expected List or String, found ${typeOf(coll)}")
      case _ => throw RuntimeError("first requires exactly 1 argument")
    }), false)

    env.define("rest", BuiltinFunction("rest", (args, _) => args match {
      case List(ListValue(items)) => if items.nonEmpty then ListValue(items.tail) else ListValue(List())
      case List(StrValue(s)) => if s.nonEmpty then StrValue(s.tail) else StrValue("")
      case List(coll) => throw RuntimeError(s"rest(...): invalid argument type, expected List or String, found ${typeOf(coll)}")
      case _ => throw RuntimeError("rest requires exactly 1 argument")
    }), false)

    env.define("size", BuiltinFunction("size", (args, _) => args match {
      case List(ListValue(items)) => IntValue(items.length)
      case List(SetValue(items)) => IntValue(items.size)
      case List(DictValue(entries)) => IntValue(entries.size)
      case List(StrValue(s)) => IntValue(s.getBytes("UTF-8").length)
      case List(coll) => throw RuntimeError(s"size(...): invalid argument type, expected List, Set, Dictionary, or String, found ${typeOf(coll)}")
      case _ => throw RuntimeError("size requires exactly 1 argument")
    }), false)

    // Higher-order functions
    env.define("map", BuiltinFunction("map", (args, env) => args match {
      case List(fn, ListValue(items)) if isCallable(fn) =>
        val mapped = items.map { item =>
          evalCall(fn, List(item), env, ListBuffer())
        }
        ListValue(mapped)
      case List(fn, ListValue(_)) if !isCallable(fn) =>
        throw RuntimeError(s"Unexpected argument: map(${typeOf(fn)}, List)")
      case List(fn, coll) if isCallable(fn) =>
        throw RuntimeError(s"Unexpected argument: map(Function, ${typeOf(coll)})")
      case List(fn, coll) =>
        throw RuntimeError(s"Unexpected argument: map(${typeOf(fn)}, ${typeOf(coll)})")
      case _ => throw RuntimeError("map requires exactly 2 arguments")
    }), false)

    env.define("filter", BuiltinFunction("filter", (args, env) => args match {
      case List(fn, ListValue(items)) if isCallable(fn) =>
        val filtered = items.filter { item =>
          val result = evalCall(fn, List(item), env, ListBuffer())
          isTruthy(result)
        }
        ListValue(filtered)
      case List(fn, ListValue(_)) if !isCallable(fn) =>
        throw RuntimeError(s"Unexpected argument: filter(${typeOf(fn)}, List)")
      case List(fn, coll) if isCallable(fn) =>
        throw RuntimeError(s"Unexpected argument: filter(Function, ${typeOf(coll)})")
      case List(fn, coll) =>
        throw RuntimeError(s"Unexpected argument: filter(${typeOf(fn)}, ${typeOf(coll)})")
      case _ => throw RuntimeError("filter requires exactly 2 arguments")
    }), false)

    env.define("fold", BuiltinFunction("fold", (args, env) => args match {
      case List(init, fn, ListValue(items)) if isCallable(fn) =>
        items.foldLeft(init) { (acc, item) =>
          evalCall(fn, List(acc, item), env, ListBuffer())
        }
      case List(init, fn, ListValue(_)) if !isCallable(fn) =>
        throw RuntimeError(s"Unexpected argument: fold(${typeOf(init)}, ${typeOf(fn)}, List)")
      case List(init, fn, coll) if isCallable(fn) =>
        throw RuntimeError(s"Unexpected argument: fold(${typeOf(init)}, Function, ${typeOf(coll)})")
      case List(init, fn, coll) =>
        throw RuntimeError(s"Unexpected argument: fold(${typeOf(init)}, ${typeOf(fn)}, ${typeOf(coll)})")
      case _ => throw RuntimeError("fold requires exactly 3 arguments")
    }), false)

    env

  private def eval(node: ujson.Value, env: Environment, output: ListBuffer[String]): Value =
    node match
      case ujson.Obj(fields) =>
        fields.get("type").map(_.str) match
          case Some("Program") =>
            val statements = fields("statements").arr
            var lastValue: Value = NilValue
            for stmt <- statements do
              stmt.obj.get("type").map(_.str) match
                case Some("Comment") => // Skip comments
                case _ =>
                  lastValue = eval(stmt.obj("value"), env, output)
            lastValue

          case Some("Integer") =>
            IntValue(fields("value").str.replace("_", "").toLong)

          case Some("Decimal") =>
            DecValue(fields("value").str.replace("_", "").toDouble)

          case Some("String") =>
            StrValue(fields("value").str)

          case Some("Boolean") =>
            BoolValue(fields("value").bool)

          case Some("Nil") =>
            NilValue

          case Some("Identifier") =>
            val name = fields("name").str
            env.get(name).getOrElse(throw RuntimeError(s"Identifier can not be found: $name"))

          case Some("Let") | Some("MutableLet") =>
            val isMutable = fields("type").str == "MutableLet"
            val name = fields("name").obj("name").str
            val value = eval(fields("value"), env, output)
            env.define(name, value, isMutable)
            value

          case Some("Assignment") =>
            val name = fields("name").obj("name").str
            val value = eval(fields("value"), env, output)
            env.set(name, value)
            value

          case Some("Infix") =>
            val op = fields("operator").str
            // Handle short-circuit evaluation for logical operators
            if op == "&&" then
              val left = eval(fields("left"), env, output)
              if !isTruthy(left) then
                BoolValue(false)
              else
                val right = eval(fields("right"), env, output)
                BoolValue(isTruthy(right))
            else if op == "||" then
              val left = eval(fields("left"), env, output)
              if isTruthy(left) then
                BoolValue(true)
              else
                val right = eval(fields("right"), env, output)
                BoolValue(isTruthy(right))
            else
              val left = eval(fields("left"), env, output)
              val right = eval(fields("right"), env, output)
              evalInfix(op, left, right, env)

          case Some("Prefix") =>
            val op = fields("operator").str
            val operand = eval(fields("operand"), env, output)
            evalPrefix(op, operand)

          case Some("List") =>
            val items = fields("items").arr.map(item => eval(item, env, output)).toList
            ListValue(items)

          case Some("Set") =>
            val items = fields("items").arr.map(item => eval(item, env, output))
            // Check if any item is a dictionary (not allowed in set literals)
            if items.exists(_.isInstanceOf[DictValue]) then
              throw RuntimeError("Unable to include a Dictionary within a Set")
            SetValue(items.toSet)

          case Some("Dictionary") =>
            val entries = fields("items").arr.map { item =>
              val key = eval(item.obj("key"), env, output)
              // Check if key is a dictionary (not allowed)
              key match
                case DictValue(_) => throw RuntimeError("Unable to use a Dictionary as a Dictionary key")
                case _ =>
              val value = eval(item.obj("value"), env, output)
              (key, value)
            }.toMap
            DictValue(entries)

          case Some("If") =>
            val condition = eval(fields("condition"), env, output)
            if isTruthy(condition) then
              eval(fields("consequence"), env, output)
            else
              eval(fields("alternative"), env, output)

          case Some("Block") =>
            val blockEnv = env.fork()
            val statements = fields("statements").arr
            var lastValue: Value = NilValue
            for stmt <- statements do
              lastValue = eval(stmt.obj("value"), env, output)
            lastValue

          case Some("Function") =>
            val params = fields("parameters").arr.map(_.obj("name").str).toList
            val body = fields("body")
            FunctionValue(params, body, env)

          case Some("Call") =>
            val function = eval(fields("function"), env, output)
            val args = fields("arguments").arr.map(arg => eval(arg, env, output)).toList
            evalCall(function, args, env, output)

          case Some("Index") =>
            val left = eval(fields("left"), env, output)
            val index = eval(fields("index"), env, output)
            evalIndex(left, index)

          case Some("FunctionComposition") =>
            val functions = fields("functions").arr.map(f => eval(f, env, output)).toList
            composeFunctions(functions, env)

          case Some("FunctionThread") =>
            val initial = eval(fields("initial"), env, output)
            val functions = fields("functions").arr.map(f => eval(f, env, output)).toList
            threadValue(initial, functions, env, output)

          case other =>
            throw RuntimeError(s"Unknown AST node type: $other")

      case _ =>
        throw RuntimeError(s"Invalid AST node: $node")

  private def evalPrefix(op: String, operand: Value): Value =
    op match
      case "-" =>
        operand match
          case IntValue(n) => IntValue(-n)
          case DecValue(n) => DecValue(-n)
          case _ => throw RuntimeError(s"Unsupported operation: -${typeOf(operand)}")
      case _ => throw RuntimeError(s"Unknown prefix operator: $op")

  private def evalInfix(op: String, left: Value, right: Value, env: Environment): Value =
    op match
      case "+" | "-" | "*" | "/" =>
        val opFn = env.get(op).get.asInstanceOf[BuiltinFunction]
        opFn.fn(List(left, right), env)

      case "==" => BoolValue(left == right)
      case "!=" => BoolValue(left != right)

      case ">" =>
        (left, right) match
          case (IntValue(a), IntValue(b)) => BoolValue(a > b)
          case (DecValue(a), DecValue(b)) => BoolValue(a > b)
          case (IntValue(a), DecValue(b)) => BoolValue(a > b)
          case (DecValue(a), IntValue(b)) => BoolValue(a > b)
          case (StrValue(a), StrValue(b)) => BoolValue(a > b)
          case _ => throw RuntimeError(s"Unsupported operation: ${typeOf(left)} > ${typeOf(right)}")

      case "<" =>
        (left, right) match
          case (IntValue(a), IntValue(b)) => BoolValue(a < b)
          case (DecValue(a), DecValue(b)) => BoolValue(a < b)
          case (IntValue(a), DecValue(b)) => BoolValue(a < b)
          case (DecValue(a), IntValue(b)) => BoolValue(a < b)
          case (StrValue(a), StrValue(b)) => BoolValue(a < b)
          case _ => throw RuntimeError(s"Unsupported operation: ${typeOf(left)} < ${typeOf(right)}")

      case ">=" =>
        (left, right) match
          case (IntValue(a), IntValue(b)) => BoolValue(a >= b)
          case (DecValue(a), DecValue(b)) => BoolValue(a >= b)
          case (IntValue(a), DecValue(b)) => BoolValue(a >= b)
          case (DecValue(a), IntValue(b)) => BoolValue(a >= b)
          case (StrValue(a), StrValue(b)) => BoolValue(a >= b)
          case _ => throw RuntimeError(s"Unsupported operation: ${typeOf(left)} >= ${typeOf(right)}")

      case "<=" =>
        (left, right) match
          case (IntValue(a), IntValue(b)) => BoolValue(a <= b)
          case (DecValue(a), DecValue(b)) => BoolValue(a <= b)
          case (IntValue(a), DecValue(b)) => BoolValue(a <= b)
          case (DecValue(a), IntValue(b)) => BoolValue(a <= b)
          case (StrValue(a), StrValue(b)) => BoolValue(a <= b)
          case _ => throw RuntimeError(s"Unsupported operation: ${typeOf(left)} <= ${typeOf(right)}")

      case _ => throw RuntimeError(s"Unknown operator: $op")

  private def evalCall(function: Value, args: List[Value], env: Environment, output: ListBuffer[String]): Value =
    function match
      case BuiltinFunction(name, fn) =>
        // Check if this is a binary operator or push that supports partial application
        val partialOps = Set("+", "-", "*", "/")
        val higherOrderPartials = Set("map", "filter")
        if partialOps.contains(name) && args.length == 1 then
          // Return a partial application
          PartialBuiltin(name, args)
        else if (name == "push" || higherOrderPartials.contains(name)) && args.length == 1 then
          // push/map/filter with partial application
          PartialBuiltin(name, args)
        else if name == "fold" && args.length < 3 then
          // fold with partial application
          PartialBuiltin(name, args)
        else if name == "assoc" && args.length < 3 then
          // assoc with partial application (3-arg function)
          PartialBuiltin(name, args)
        else
          fn(args, env)
      case PartialBuiltin(name, prevArgs) =>
        // Complete the partial application
        val allArgs = prevArgs ++ args
        env.get(name) match
          case Some(BuiltinFunction(_, fn)) => fn(allArgs, env)
          case _ => throw RuntimeError(s"Cannot complete partial application of $name")
      case FunctionValue(params, body, closure) =>
        if args.length < params.length then
          // Partial application - return a new function with remaining parameters
          val boundEnv = closure.fork()
          for (param, arg) <- params.zip(args) do
            boundEnv.define(param, arg, false)
          val remainingParams = params.drop(args.length)
          FunctionValue(remainingParams, body, boundEnv)
        else
          // Full application (or extra args which are ignored)
          val callEnv = closure.fork()
          for (param, arg) <- params.zip(args) do
            callEnv.define(param, arg, false)
          eval(body, callEnv, output)
      case ComposedFunction(functions) =>
        // Apply functions in sequence: f >> g means g(f(args))
        functions.foldLeft(args.head) { (acc, fn) =>
          evalCall(fn, List(acc), env, output)
        }
      case _ => throw RuntimeError(s"Expected a Function, found: ${typeOf(function)}")

  private def evalIndex(collection: Value, index: Value): Value =
    (collection, index) match
      case (ListValue(items), IntValue(i)) =>
        val idx = if i < 0 then items.length + i else i
        if idx >= 0 && idx < items.length then items(idx.toInt) else NilValue
      case (ListValue(_), idx) =>
        throw RuntimeError(s"Unable to perform index operation, found: List[${typeOf(idx)}]")

      case (StrValue(s), IntValue(i)) =>
        val idx = if i < 0 then s.length + i else i
        if idx >= 0 && idx < s.length then StrValue(s.charAt(idx.toInt).toString) else NilValue
      case (StrValue(_), idx) =>
        throw RuntimeError(s"Unable to perform index operation, found: String[${typeOf(idx)}]")

      case (DictValue(entries), key) =>
        entries.getOrElse(key, NilValue)

      case _ =>
        throw RuntimeError(s"Unable to perform index operation on ${typeOf(collection)}")

  private def composeFunctions(functions: List[Value], env: Environment): Value =
    // Function composition: f >> g means g(f(x))
    // Create a ComposedFunction that captures the functions
    ComposedFunction(functions)

  private def threadValue(initial: Value, functions: List[Value], env: Environment, output: ListBuffer[String]): Value =
    functions.foldLeft(initial) { (acc, fn) =>
      evalCall(fn, List(acc), env, output)
    }

  private def isCallable(value: Value): Boolean =
    value match
      case _: FunctionValue => true
      case _: BuiltinFunction => true
      case _: ComposedFunction => true
      case _: PartialBuiltin => true
      case _ => false

  private def isTruthy(value: Value): Boolean =
    value match
      case BoolValue(b) => b
      case IntValue(0) => false
      case IntValue(_) => true
      case DecValue(0.0) => false
      case DecValue(_) => true
      case StrValue("") => false
      case StrValue(_) => true
      case ListValue(Nil) => false
      case ListValue(_) => true
      case SetValue(s) if s.isEmpty => false
      case SetValue(_) => true
      case DictValue(d) if d.isEmpty => false
      case DictValue(_) => true
      case NilValue => false
      case _ => true

  private def formatValue(value: Value): String =
    value match
      case IntValue(i) => i.toString
      case DecValue(d) =>
        val s = d.toString
        // Remove trailing zeros after decimal point
        if s.contains(".") && !s.contains("E") then
          s.reverse.dropWhile(_ == '0').dropWhile(_ == '.').reverse
        else s
      case StrValue(s) => "\"" + s + "\""
      case BoolValue(b) => b.toString
      case NilValue => "nil"
      case ListValue(items) =>
        "[" + items.map(formatValue).mkString(", ") + "]"
      case SetValue(items) =>
        val sorted = items.toList.sortBy(formatValue)
        "{" + sorted.map(formatValue).mkString(", ") + "}"
      case DictValue(entries) =>
        val sorted = entries.toList.sortBy(e => formatValue(e._1))
        val formatted = sorted.map { case (k, v) =>
          formatValue(k) + ": " + formatValue(v)
        }
        "#{" + formatted.mkString(", ") + "}"
      case FunctionValue(_, _, _) => "[closure]"
      case BuiltinFunction(name, _) => s"[builtin: $name]"
      case ComposedFunction(_) => "[composed]"
      case PartialBuiltin(name, _) => s"[partial: $name]"

  private def escapeString(s: String): String =
    s.flatMap {
      case '"' => "\\\""
      case '\\' => "\\\\"
      case '\n' => "\\n"
      case '\t' => "\\t"
      case c => c.toString
    }

  private def typeOf(value: Value): String =
    value match
      case IntValue(_) => "Integer"
      case DecValue(_) => "Decimal"
      case StrValue(_) => "String"
      case BoolValue(_) => "Boolean"
      case NilValue => "Nil"
      case ListValue(_) => "List"
      case SetValue(_) => "Set"
      case DictValue(_) => "Dictionary"
      case FunctionValue(_, _, _) => "Function"
      case BuiltinFunction(_, _) => "Function"
      case ComposedFunction(_) => "Function"
      case PartialBuiltin(_, _) => "Function"