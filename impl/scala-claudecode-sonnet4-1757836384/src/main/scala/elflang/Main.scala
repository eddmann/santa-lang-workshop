package elflang

object Main:
  def main(args: Array[String]): Unit =
    args.toList match
      case Nil =>
        System.err.println("Usage: <bin> <file> | <bin> ast <file> | <bin> tokens <file>")
        System.exit(1)

      case "tokens" :: file :: Nil =>
        try
          val source = scala.io.Source.fromFile(file).mkString
          val tokens = Lexer.tokenize(source)
          tokens.foreach(token => println(ujson.write(ujson.Obj("type" -> token.tokenType, "value" -> token.value))))
        catch
          case e: Exception =>
            System.err.println(s"Error: ${e.getMessage}")
            System.exit(1)

      case "ast" :: file :: Nil =>
        try
          val source = scala.io.Source.fromFile(file).mkString
          val tokens = Lexer.tokenize(source)
          val ast = Parser.parse(tokens)
          println(ujson.write(ast, indent = 2))
        catch
          case e: Exception =>
            System.err.println(s"Error: ${e.getMessage}")
            System.exit(1)

      case file :: Nil =>
        try
          val source = scala.io.Source.fromFile(file).mkString
          val tokens = Lexer.tokenize(source)
          val ast = Parser.parse(tokens)
          val result = Interpreter.interpret(ast)
          print(result)
        catch
          case e: RuntimeError =>
            println(s"[Error] ${e.getMessage}")
            System.exit(1)
          case e: Exception =>
            println(s"[Error] ${e.getMessage}")
            System.exit(1)

      case _ =>
        System.err.println("Usage: <bin> <file> | <bin> ast <file> | <bin> tokens <file>")
        System.exit(1)