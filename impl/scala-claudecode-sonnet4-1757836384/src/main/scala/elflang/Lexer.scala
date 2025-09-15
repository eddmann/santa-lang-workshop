package elflang

import scala.collection.mutable.ListBuffer
import scala.util.matching.Regex

object Lexer:
  def tokenize(source: String): List[Token] =
    val tokens = ListBuffer[Token]()
    var pos = 0
    val length = source.length

    while pos < length do
      val ch = source(pos)

      // Skip whitespace
      if ch.isWhitespace then
        pos += 1

      // Comments
      else if pos < length - 1 && source.substring(pos, pos + 2) == "//" then
        val start = pos
        while pos < length && source(pos) != '\n' do
          pos += 1
        tokens += Token("CMT", source.substring(start, pos))

      // Multi-character operators (must come before single-char operators)
      else if pos < length - 1 && source.substring(pos, pos + 2) == "#{"  then
        tokens += Token("#{", "#{")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == "==" then
        tokens += Token("==", "==")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == "!=" then
        tokens += Token("!=", "!=")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == ">=" then
        tokens += Token(">=", ">=")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == "<=" then
        tokens += Token("<=", "<=")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == "&&" then
        tokens += Token("&&", "&&")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == "||" then
        tokens += Token("||", "||")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == "|>" then
        tokens += Token("|>", "|>")
        pos += 2
      else if pos < length - 1 && source.substring(pos, pos + 2) == ">>" then
        tokens += Token(">>", ">>")
        pos += 2

      // Single-character operators
      else if "+-*/={}[]><;(),:|".contains(ch) then
        tokens += Token(ch.toString, ch.toString)
        pos += 1

      // String literals
      else if ch == '"' then
        val start = pos
        pos += 1 // Skip opening quote
        val sb = new StringBuilder("\"")

        while pos < length && source(pos) != '"' do
          if source(pos) == '\\' && pos < length - 1 then
            // Handle escape sequences
            pos += 1
            source(pos) match
              case 'n' => sb.append("\\n")
              case 't' => sb.append("\\t")
              case '"' => sb.append("\\\"")
              case '\\' => sb.append("\\\\")
              case c => sb.append("\\").append(c)
            pos += 1
          else
            sb.append(source(pos))
            pos += 1

        if pos < length then
          pos += 1 // Skip closing quote
          sb.append('"')

        tokens += Token("STR", sb.toString)

      // Numbers (integers and decimals)
      else if ch.isDigit then
        val start = pos

        // Read digits with underscores
        while pos < length && (source(pos).isDigit || source(pos) == '_') do
          pos += 1

        // Check if it's a decimal
        if pos < length && source(pos) == '.' then
          pos += 1 // Skip dot
          while pos < length && (source(pos).isDigit || source(pos) == '_') do
            pos += 1
          tokens += Token("DEC", source.substring(start, pos))
        else
          tokens += Token("INT", source.substring(start, pos))

      // Identifiers and keywords
      else if ch.isLetter || ch == '_' then
        val start = pos
        while pos < length && (source(pos).isLetterOrDigit || source(pos) == '_') do
          pos += 1

        val word = source.substring(start, pos)
        val tokenType = word match
          case "let" => "LET"
          case "mut" => "MUT"
          case "if" => "IF"
          case "else" => "ELSE"
          case "true" => "TRUE"
          case "false" => "FALSE"
          case "nil" => "NIL"
          case _ => "ID"

        tokens += Token(tokenType, word)

      else
        // Skip unknown characters
        pos += 1

    tokens.toList