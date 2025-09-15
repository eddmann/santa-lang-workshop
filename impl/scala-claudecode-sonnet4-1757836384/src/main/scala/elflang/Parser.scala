package elflang

import scala.collection.mutable.ListBuffer

class ParseError(message: String) extends Exception(message)

object Parser:
  def parse(tokens: List[Token]): ujson.Value =
    val parser = new Parser(tokens)
    parser.parseProgram()

class Parser(tokens: List[Token]):
  private var pos = 0
  private val length = tokens.length

  private def current: Option[Token] =
    if pos < length then Some(tokens(pos)) else None

  private def peek(offset: Int = 1): Option[Token] =
    val newPos = pos + offset
    if newPos < length then Some(tokens(newPos)) else None

  private def advance(): Option[Token] =
    val token = current
    pos += 1
    token

  private def expect(tokenType: String): Token =
    current match
      case Some(token) if token.tokenType == tokenType =>
        advance()
        token
      case Some(token) =>
        throw ParseError(s"Expected $tokenType, got ${token.tokenType}")
      case None =>
        throw ParseError(s"Expected $tokenType, got end of input")

  private def match_(tokenTypes: String*): Boolean =
    current.exists(token => tokenTypes.contains(token.tokenType))

  def parseProgram(): ujson.Value =
    val statements = ListBuffer[ujson.Value]()

    while pos < length do
      // Include comments as statements
      if match_("CMT") then
        val comment = advance().get
        statements += ujson.Obj(
          "type" -> "Comment",
          "value" -> comment.value
        )
      else
        statements += parseStatement()

    ujson.Obj(
      "statements" -> ujson.Arr(statements.toSeq*),
      "type" -> "Program"
    )

  private def parseStatement(): ujson.Value =
    // Check for assignment statements (mutable variable updates)
    if match_("ID") then
      val peekNext = peek()
      peekNext match
        case Some(Token("=", _)) =>
          val name = advance().get
          advance() // consume '='
          val value = parseExpression()
          // Optional semicolon
          if match_(";") then
            advance()
          return ujson.Obj(
            "type" -> "Expression",
            "value" -> ujson.Obj(
              "name" -> ujson.Obj("name" -> name.value, "type" -> "Identifier"),
              "type" -> "Assignment",
              "value" -> value
            )
          )
        case _ => // Continue with normal expression

    val expr = parseExpression()

    // Optional semicolon
    if match_(";") then
      advance()

    ujson.Obj(
      "type" -> "Expression",
      "value" -> expr
    )

  private def parseExpression(): ujson.Value =
    parseFunctionThread()

  // Precedence levels (lowest to highest):
  // Function thread: |>
  // Function composition: >>
  // Logical OR: ||
  // Logical AND: &&
  // Comparison: ==, !=, >, <, >=, <=
  // Addition/Subtraction: + -
  // Multiplication/Division: * /
  // Primary: literals, parentheses, identifiers, let expressions

  private def parseFunctionThread(): ujson.Value =
    var left = parseFunctionComposition()

    while match_("|>") do
      advance() // consume '|>'
      val right = parseFunctionComposition()
      // Build up the thread
      left match
        case thread @ ujson.Obj(pairs) if pairs.value.get("type").contains(ujson.Str("FunctionThread")) =>
          // Already a thread, add to it
          val functions = thread("functions").arr.toSeq
          left = ujson.Obj(
            "functions" -> ujson.Arr((functions :+ right)*),
            "initial" -> thread("initial"),
            "type" -> "FunctionThread"
          )
        case _ =>
          // Start a new thread
          left = ujson.Obj(
            "functions" -> ujson.Arr(right),
            "initial" -> left,
            "type" -> "FunctionThread"
          )

    left

  private def parseFunctionComposition(): ujson.Value =
    var functions = ListBuffer[ujson.Value]()
    functions += parseLogicalOr()

    while match_(">>") do
      advance() // consume '>>'
      functions += parseLogicalOr()

    if functions.length > 1 then
      ujson.Obj(
        "functions" -> ujson.Arr(functions.toSeq*),
        "type" -> "FunctionComposition"
      )
    else
      functions.head

  private def parseLogicalOr(): ujson.Value =
    var left = parseLogicalAnd()

    while match_("||") do
      val op = advance().get.value
      val right = parseLogicalAnd()
      left = ujson.Obj(
        "left" -> left,
        "operator" -> op,
        "right" -> right,
        "type" -> "Infix"
      )

    left

  private def parseLogicalAnd(): ujson.Value =
    var left = parseComparison()

    while match_("&&") do
      val op = advance().get.value
      val right = parseComparison()
      left = ujson.Obj(
        "left" -> left,
        "operator" -> op,
        "right" -> right,
        "type" -> "Infix"
      )

    left

  private def parseComparison(): ujson.Value =
    var left = parseAddition()

    while match_("==", "!=", ">", "<", ">=", "<=") do
      val op = advance().get.value
      val right = parseAddition()
      left = ujson.Obj(
        "left" -> left,
        "operator" -> op,
        "right" -> right,
        "type" -> "Infix"
      )

    left

  private def parseAddition(): ujson.Value =
    var left = parseMultiplication()

    while match_("+", "-") do
      val op = advance().get.value
      val right = parseMultiplication()
      left = ujson.Obj(
        "left" -> left,
        "operator" -> op,
        "right" -> right,
        "type" -> "Infix"
      )

    left

  private def parseMultiplication(): ujson.Value =
    var left = parsePostfix()

    while match_("*", "/") do
      val op = advance().get.value
      val right = parsePostfix()
      left = ujson.Obj(
        "left" -> left,
        "operator" -> op,
        "right" -> right,
        "type" -> "Infix"
      )

    left

  private def parsePostfix(): ujson.Value =
    var left = parsePrimary()

    // Handle indexing and function calls (higher precedence)
    while match_("[", "(") do
      current match
        case Some(Token("[", _)) =>
          advance() // consume '['
          val index = parseExpression()
          expect("]")
          left = ujson.Obj(
            "index" -> index,
            "left" -> left,
            "type" -> "Index"
          )
        case Some(Token("(", _)) =>
          advance() // consume '('
          val arguments = ListBuffer[ujson.Value]()

          if !match_(")") then
            arguments += parseExpression()
            while match_(",") do
              advance() // consume ','
              if match_(")") then
                // Allow trailing comma
                ()
              else
                arguments += parseExpression()

          expect(")")
          left = ujson.Obj(
            "arguments" -> ujson.Arr(arguments.toSeq*),
            "function" -> left,
            "type" -> "Call"
          )
        case _ =>
          return left

    left

  private def parsePrimary(): ujson.Value =
    current match
      // Handle unary minus
      case Some(Token("-", _)) =>
        advance() // consume '-'
        val operand = parsePrimary()
        ujson.Obj(
          "operand" -> operand,
          "operator" -> "-",
          "type" -> "Prefix"
        )

      case Some(Token("INT", value)) =>
        advance()
        // Keep underscores in the AST representation
        ujson.Obj("type" -> "Integer", "value" -> value)

      case Some(Token("DEC", value)) =>
        advance()
        // Keep underscores in the AST representation
        ujson.Obj("type" -> "Decimal", "value" -> value)

      case Some(Token("STR", value)) =>
        advance()
        // Remove quotes and unescape
        val content = unescapeString(value.substring(1, value.length - 1))
        ujson.Obj("type" -> "String", "value" -> content)

      case Some(Token("TRUE", _)) =>
        advance()
        ujson.Obj("type" -> "Boolean", "value" -> true)

      case Some(Token("FALSE", _)) =>
        advance()
        ujson.Obj("type" -> "Boolean", "value" -> false)

      case Some(Token("NIL", _)) =>
        advance()
        ujson.Obj("type" -> "Nil")

      case Some(Token("ID", value)) =>
        advance()
        ujson.Obj("name" -> value, "type" -> "Identifier")

      // Handle operators as identifiers in certain contexts (but not unary minus)
      case Some(Token(op, value)) if "+*/<>=".contains(op) && op.length == 1 =>
        advance()
        ujson.Obj("name" -> value, "type" -> "Identifier")

      case Some(Token("LET", _)) =>
        parseLet()

      case Some(Token("IF", _)) =>
        parseIf()

      case Some(Token("(", _)) =>
        advance() // consume '('
        val expr = parseExpression()
        expect(")")
        expr

      case Some(Token("[", _)) =>
        parseList()

      case Some(Token("{", _)) =>
        parseSet()

      case Some(Token("#{", _)) =>
        parseDictionary()

      case Some(Token("|", _)) =>
        parseFunction()

      case Some(Token("||", _)) =>
        parseZeroArgFunction()

      case Some(token) =>
        throw ParseError(s"Unexpected token: ${token.tokenType}")

      case None =>
        throw ParseError("Unexpected end of input")

  private def parseLet(): ujson.Value =
    advance() // consume 'let'

    val isMutable = if match_("MUT") then
      advance()
      true
    else
      false

    val name = expect("ID")
    expect("=")
    val value = parseExpression()

    ujson.Obj(
      "name" -> ujson.Obj("name" -> name.value, "type" -> "Identifier"),
      "type" -> (if isMutable then "MutableLet" else "Let"),
      "value" -> value
    )

  private def parseList(): ujson.Value =
    advance() // consume '['
    val items = ListBuffer[ujson.Value]()

    if !match_("]") then
      items += parseExpression()
      while match_(",") do
        advance() // consume ','
        if match_("]") then
          // Allow trailing comma but don't add another element
          ()
        else
          items += parseExpression()

    expect("]")
    ujson.Obj(
      "items" -> ujson.Arr(items.toSeq*),
      "type" -> "List"
    )

  private def parseSet(): ujson.Value =
    advance() // consume '{'
    val items = ListBuffer[ujson.Value]()

    if !match_("}") then
      items += parseExpression()
      while match_(",") do
        advance() // consume ','
        if match_("}") then
          // Allow trailing comma but don't add another element
          ()
        else
          items += parseExpression()

    expect("}")
    ujson.Obj(
      "items" -> ujson.Arr(items.toSeq*),
      "type" -> "Set"
    )

  private def parseDictionary(): ujson.Value =
    advance() // consume '#{'
    val items = ListBuffer[ujson.Value]()

    if !match_("}") then
      // Parse key-value pair
      val key = parseExpression()
      expect(":")
      val value = parseExpression()
      items += ujson.Obj("key" -> key, "value" -> value)

      while match_(",") do
        advance() // consume ','
        if match_("}") then
          // Allow trailing comma but don't add another element
          ()
        else
          val nextKey = parseExpression()
          expect(":")
          val nextValue = parseExpression()
          items += ujson.Obj("key" -> nextKey, "value" -> nextValue)

    expect("}")
    ujson.Obj(
      "items" -> ujson.Arr(items.toSeq*),
      "type" -> "Dictionary"
    )

  private def parseIf(): ujson.Value =
    advance() // consume 'if'
    val condition = parseExpression()
    val consequence = parseBlock()
    expect("ELSE")
    val alternative = parseBlock()

    ujson.Obj(
      "alternative" -> alternative,
      "condition" -> condition,
      "consequence" -> consequence,
      "type" -> "If"
    )

  private def parseFunction(): ujson.Value =
    advance() // consume '|'
    val parameters = ListBuffer[ujson.Value]()

    // Parse parameter list
    if !match_("|") then
      parameters += ujson.Obj("name" -> expect("ID").value, "type" -> "Identifier")
      while match_(",") do
        advance() // consume ','
        parameters += ujson.Obj("name" -> expect("ID").value, "type" -> "Identifier")

    expect("|") // closing |

    // Parse body - either a block or a single expression wrapped in a block
    val body = if match_("{") then
      parseBlock()
    else
      // Single expression - wrap in a block
      val expr = parseExpression()
      ujson.Obj(
        "statements" -> ujson.Arr(ujson.Obj(
          "type" -> "Expression",
          "value" -> expr
        )),
        "type" -> "Block"
      )

    ujson.Obj(
      "body" -> body,
      "parameters" -> ujson.Arr(parameters.toSeq*),
      "type" -> "Function"
    )

  private def parseZeroArgFunction(): ujson.Value =
    advance() // consume '||'

    // Parse body - either a block or a single expression wrapped in a block
    val body = if match_("{") then
      parseBlock()
    else
      // Single expression - wrap in a block
      val expr = parseExpression()
      ujson.Obj(
        "statements" -> ujson.Arr(ujson.Obj(
          "type" -> "Expression",
          "value" -> expr
        )),
        "type" -> "Block"
      )

    ujson.Obj(
      "body" -> body,
      "parameters" -> ujson.Arr(), // Empty parameters for zero-arg function
      "type" -> "Function"
    )

  private def parseBlock(): ujson.Value =
    expect("{")
    val statements = ListBuffer[ujson.Value]()

    while !match_("}") do
      // Skip comments at block level
      if match_("CMT") then
        advance()
      else
        statements += parseBlockStatement()

    expect("}")
    ujson.Obj(
      "statements" -> ujson.Arr(statements.toSeq*),
      "type" -> "Block"
    )

  private def parseBlockStatement(): ujson.Value =
    // Check for assignment statements (mutable variable updates)
    if match_("ID") then
      val peekNext = peek()
      peekNext match
        case Some(Token("=", _)) =>
          val name = advance().get
          advance() // consume '='
          val value = parseExpression()
          // Optional semicolon
          if match_(";") then
            advance()
          return ujson.Obj(
            "type" -> "Expression",
            "value" -> ujson.Obj(
              "name" -> ujson.Obj("name" -> name.value, "type" -> "Identifier"),
              "type" -> "Assignment",
              "value" -> value
            )
          )
        case _ => // Continue with normal expression

    val expr = parseExpression()

    // Optional semicolon
    if match_(";") then
      advance()

    ujson.Obj(
      "type" -> "Expression",
      "value" -> expr
    )

  private def unescapeString(s: String): String =
    val sb = new StringBuilder()
    var i = 0
    while i < s.length do
      if s(i) == '\\' && i < s.length - 1 then
        s(i + 1) match
          case 'n' => sb.append('\n')
          case 't' => sb.append('\t')
          case '"' => sb.append('"')
          case '\\' => sb.append('\\')
          case c => sb.append('\\').append(c)
        i += 2
      else
        sb.append(s(i))
        i += 1
    sb.toString