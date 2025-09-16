package com.elflang;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class CLI {
    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("Usage: <bin> <file> | <bin> tokens <file> | <bin> ast <file>");
            System.exit(1);
        }

        try {
            if (args.length == 2 && "tokens".equals(args[0])) {
                handleTokens(args[1]);
            } else if (args.length == 2 && "ast".equals(args[0])) {
                handleAST(args[1]);
            } else if (args.length == 1) {
                handleRun(args[0]);
            } else {
                System.err.println("Usage: <bin> <file> | <bin> tokens <file> | <bin> ast <file>");
                System.exit(1);
            }
        } catch (Exception e) {
            System.out.println("[Error] " + e.getMessage());
            System.exit(1);
        }
    }

    private static void handleTokens(String filename) throws IOException {
        String content = Files.readString(Paths.get(filename));
        Lexer lexer = new Lexer(content);
        List<Token> tokens = lexer.tokenize();

        for (Token token : tokens) {
            System.out.println(token.toJSON());
        }
    }

    private static void handleAST(String filename) throws IOException {
        String content = Files.readString(Paths.get(filename));
        Lexer lexer = new Lexer(content);
        List<Token> tokens = lexer.tokenize();

        Parser parser = new Parser(tokens);
        Program program = parser.parse();

        System.out.println(program.toJSON());
    }

    private static void handleRun(String filename) throws IOException {
        String content = Files.readString(Paths.get(filename));
        Lexer lexer = new Lexer(content);
        List<Token> tokens = lexer.tokenize();

        Parser parser = new Parser(tokens);
        Program program = parser.parse();

        Interpreter interpreter = new Interpreter();
        interpreter.execute(program);
    }
}