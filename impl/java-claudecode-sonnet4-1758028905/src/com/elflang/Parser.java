package com.elflang;

import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;

public class Parser {
    private final List<Token> tokens;
    private int position = 0;

    public Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    public Program parse() {
        List<TreeMap<String, Object>> statements = new ArrayList<>();

        while (position < tokens.size()) {
            // Handle comments as statements
            if (current() != null && "CMT".equals(current().getType())) {
                TreeMap<String, Object> comment = new TreeMap<>();
                comment.put("type", "Comment");
                comment.put("value", current().getValue());
                statements.add(comment);
                advance();
                continue;
            }

            TreeMap<String, Object> stmt = parseStatement();
            if (stmt != null) {
                statements.add(stmt);
            }

            // Skip optional semicolons
            if (current() != null && ";".equals(current().getType())) {
                advance();
            }
        }

        return new Program(statements);
    }

    private TreeMap<String, Object> parseStatement() {
        TreeMap<String, Object> expression = parseExpression();
        if (expression == null) {
            return null;
        }

        TreeMap<String, Object> statement = new TreeMap<>();
        statement.put("type", "Expression");
        statement.put("value", expression);
        return statement;
    }

    private TreeMap<String, Object> parseExpression() {
        return parseAssignmentExpression();
    }

    private TreeMap<String, Object> parseAssignmentExpression() {
        TreeMap<String, Object> expr = parseThreadExpression();

        if (current() != null && "=".equals(current().getType()) && position > 0 && !isAfterLet()) {
            advance(); // consume '='
            TreeMap<String, Object> value = parseThreadExpression();

            TreeMap<String, Object> assignment = new TreeMap<>();
            assignment.put("name", expr);
            assignment.put("type", "Assignment");
            assignment.put("value", value);
            return assignment;
        }

        return expr;
    }

    private boolean isAfterLet() {
        // Check if previous non-ID token was LET
        int i = position - 2;
        while (i >= 0 && "ID".equals(tokens.get(i).getType())) {
            i--;
        }
        return i >= 0 && ("LET".equals(tokens.get(i).getType()) || "MUT".equals(tokens.get(i).getType()));
    }

    private TreeMap<String, Object> parseThreadExpression() {
        TreeMap<String, Object> expr = parseCompositionExpression();

        if (current() != null && "|>".equals(current().getType())) {
            TreeMap<String, Object> initial = expr;
            List<TreeMap<String, Object>> functions = new ArrayList<>();

            while (current() != null && "|>".equals(current().getType())) {
                advance(); // consume '|>'
                TreeMap<String, Object> func = parseCompositionExpression();
                functions.add(func);
            }

            TreeMap<String, Object> thread = new TreeMap<>();
            thread.put("initial", initial);
            thread.put("functions", functions);
            thread.put("type", "FunctionThread");
            return thread;
        }

        return expr;
    }

    private TreeMap<String, Object> parseCompositionExpression() {
        TreeMap<String, Object> expr = parseLetExpression();

        if (current() != null && ">>".equals(current().getType())) {
            List<TreeMap<String, Object>> functions = new ArrayList<>();
            functions.add(expr);

            while (current() != null && ">>".equals(current().getType())) {
                advance(); // consume '>>'
                TreeMap<String, Object> func = parseInfixExpression();
                functions.add(func);
            }

            TreeMap<String, Object> composition = new TreeMap<>();
            composition.put("functions", functions);
            composition.put("type", "FunctionComposition");
            return composition;
        }

        return expr;
    }

    private TreeMap<String, Object> parseCompositionExpressionWithoutLet() {
        TreeMap<String, Object> expr = parseInfixExpression();

        if (current() != null && ">>".equals(current().getType())) {
            List<TreeMap<String, Object>> functions = new ArrayList<>();
            functions.add(expr);

            while (current() != null && ">>".equals(current().getType())) {
                advance(); // consume '>>'
                TreeMap<String, Object> func = parseInfixExpression();
                functions.add(func);
            }

            TreeMap<String, Object> composition = new TreeMap<>();
            composition.put("functions", functions);
            composition.put("type", "FunctionComposition");
            return composition;
        }

        return expr;
    }

    private TreeMap<String, Object> parseLetExpression() {
        if (current() != null && "LET".equals(current().getType())) {
            advance(); // consume 'let'

            boolean isMutable = false;
            if (current() != null && "MUT".equals(current().getType())) {
                isMutable = true;
                advance(); // consume 'mut'
            }

            if (current() == null || !"ID".equals(current().getType())) {
                throw new RuntimeException("Expected identifier after let");
            }

            String varName = current().getValue();
            advance(); // consume identifier

            if (current() == null || !"=".equals(current().getType())) {
                throw new RuntimeException("Expected '=' after identifier");
            }
            advance(); // consume '='

            TreeMap<String, Object> value = parseCompositionExpressionWithoutLet();

            TreeMap<String, Object> identifier = new TreeMap<>();
            identifier.put("name", varName);
            identifier.put("type", "Identifier");

            TreeMap<String, Object> letExpr = new TreeMap<>();
            letExpr.put("name", identifier);
            letExpr.put("type", isMutable ? "MutableLet" : "Let");
            letExpr.put("value", value);

            return letExpr;
        }

        return parseInfixExpression();
    }

    private TreeMap<String, Object> parsePrimaryExpression() {
        Token token = current();
        if (token == null) {
            return null;
        }

        switch (token.getType()) {
            case "IF":
                return parseIfExpression();

            case "|":
                return parseFunctionLiteral();

            case "||":
                return parseZeroParameterFunction();

            case "INT":
                advance();
                TreeMap<String, Object> intLiteral = new TreeMap<>();
                intLiteral.put("type", "Integer");
                intLiteral.put("value", token.getValue());
                return intLiteral;

            case "DEC":
                advance();
                TreeMap<String, Object> decLiteral = new TreeMap<>();
                decLiteral.put("type", "Decimal");
                decLiteral.put("value", token.getValue());
                return decLiteral;

            case "STR":
                advance();
                TreeMap<String, Object> strLiteral = new TreeMap<>();
                strLiteral.put("type", "String");
                // Remove the surrounding quotes for the AST value
                String strValue = token.getValue();
                if (strValue.startsWith("\"") && strValue.endsWith("\"")) {
                    strValue = strValue.substring(1, strValue.length() - 1);
                    // Handle escape sequences
                    strValue = strValue.replace("\\\"", "\"");
                    strValue = strValue.replace("\\n", "\n");
                    strValue = strValue.replace("\\t", "\t");
                    strValue = strValue.replace("\\\\", "\\");
                }
                strLiteral.put("value", strValue);
                return strLiteral;

            case "TRUE":
                advance();
                TreeMap<String, Object> trueLiteral = new TreeMap<>();
                trueLiteral.put("type", "Boolean");
                trueLiteral.put("value", true);
                return trueLiteral;

            case "FALSE":
                advance();
                TreeMap<String, Object> falseLiteral = new TreeMap<>();
                falseLiteral.put("type", "Boolean");
                falseLiteral.put("value", false);
                return falseLiteral;

            case "NIL":
                advance();
                TreeMap<String, Object> nilLiteral = new TreeMap<>();
                nilLiteral.put("type", "Nil");
                return nilLiteral;

            case "[":
                return parseListLiteral();

            case "{":
                return parseSetLiteral();

            case "#{":
                return parseDictLiteral();

            case "ID":
            case "+":
            case "-":
            case "*":
            case "/":
            case "==":
            case "!=":
            case ">":
            case "<":
            case ">=":
            case "<=":
                advance();
                TreeMap<String, Object> identifier = new TreeMap<>();
                identifier.put("name", token.getValue());
                identifier.put("type", "Identifier");
                return identifier;

            default:
                throw new RuntimeException("Unexpected token: " + token.getType());
        }
    }

    private Token current() {
        if (position >= tokens.size()) {
            return null;
        }
        return tokens.get(position);
    }

    private void advance() {
        position++;
    }

    private TreeMap<String, Object> parseInfixExpression() {
        return parseLogical();
    }

    private TreeMap<String, Object> parseInfixExpressionWithoutComposition() {
        // Parse everything except composition and threading (which have the lowest precedence)
        // Start with let expressions, but if it's not a let, parse only up to logical operators
        if (current() != null && "LET".equals(current().getType())) {
            return parseLetExpression();
        } else {
            return parseLogical();
        }
    }

    private TreeMap<String, Object> parseLogical() {
        TreeMap<String, Object> left = parseComparison();

        while (current() != null && isLogicalOperator(current().getType())) {
            String operator = current().getType();
            advance();
            TreeMap<String, Object> right = parseComparison();

            TreeMap<String, Object> infix = new TreeMap<>();
            infix.put("left", left);
            infix.put("operator", operator);
            infix.put("right", right);
            infix.put("type", "Infix");

            left = infix;
        }

        return left;
    }

    private TreeMap<String, Object> parseComparison() {
        TreeMap<String, Object> left = parseArithmetic();

        while (current() != null && isComparisonOperator(current().getType())) {
            String operator = current().getType();
            advance();
            TreeMap<String, Object> right = parseArithmetic();

            TreeMap<String, Object> infix = new TreeMap<>();
            infix.put("left", left);
            infix.put("operator", operator);
            infix.put("right", right);
            infix.put("type", "Infix");

            left = infix;
        }

        return left;
    }

    private TreeMap<String, Object> parseArithmetic() {
        TreeMap<String, Object> left = parseTerm();

        while (current() != null && ("+".equals(current().getType()) || "-".equals(current().getType()))) {
            String operator = current().getType();
            advance();
            TreeMap<String, Object> right = parseTerm();

            TreeMap<String, Object> infix = new TreeMap<>();
            infix.put("left", left);
            infix.put("operator", operator);
            infix.put("right", right);
            infix.put("type", "Infix");

            left = infix;
        }

        return left;
    }

    private TreeMap<String, Object> parseTerm() {
        TreeMap<String, Object> left = parseFactor();

        while (current() != null && ("*".equals(current().getType()) || "/".equals(current().getType()))) {
            String operator = current().getType();
            advance();
            TreeMap<String, Object> right = parseFactor();

            TreeMap<String, Object> infix = new TreeMap<>();
            infix.put("left", left);
            infix.put("operator", operator);
            infix.put("right", right);
            infix.put("type", "Infix");

            left = infix;
        }

        return left;
    }

    private TreeMap<String, Object> parseFactor() {
        // Handle unary operators, but only when they're truly unary (followed by an operand)
        if (current() != null && ("-".equals(current().getType()) || "+".equals(current().getType()))) {
            // Look ahead to determine if this is a unary operator or an identifier
            if (position + 1 < tokens.size()) {
                String nextType = tokens.get(position + 1).getType();
                // This is a unary operator only if followed by something that can be an operand
                // NOT if followed by (, ), ,, ;, or EOF which would indicate it's used as an identifier
                if ("(".equals(nextType) || ")".equals(nextType) || ",".equals(nextType) || ";".equals(nextType)) {
                    // This is an identifier, not a unary operator
                    return parsePostfixExpression();
                }
            }

            String operator = current().getType();
            advance(); // consume operator
            TreeMap<String, Object> operand = parseFactor();

            TreeMap<String, Object> unary = new TreeMap<>();
            unary.put("operator", operator);
            unary.put("operand", operand);
            unary.put("type", "Unary");
            return unary;
        }

        if (current() != null && "(".equals(current().getType())) {
            advance(); // consume '('
            TreeMap<String, Object> expr = parseInfixExpression();
            if (current() == null || !")".equals(current().getType())) {
                throw new RuntimeException("Expected closing parenthesis");
            }
            advance(); // consume ')'
            return expr;
        }

        return parsePostfixExpression();
    }

    private TreeMap<String, Object> parsePostfixExpression() {
        TreeMap<String, Object> expr = parsePrimaryExpression();

        while (current() != null && ("[".equals(current().getType()) || "(".equals(current().getType()))) {
            if ("[".equals(current().getType())) {
                advance(); // consume '['
                TreeMap<String, Object> index = parseInfixExpression();
                if (current() == null || !"]".equals(current().getType())) {
                    throw new RuntimeException("Expected closing ']' in index");
                }
                advance(); // consume ']'

                TreeMap<String, Object> indexExpr = new TreeMap<>();
                indexExpr.put("index", index);
                indexExpr.put("left", expr);
                indexExpr.put("type", "Index");

                expr = indexExpr;
            } else if ("(".equals(current().getType())) {
                advance(); // consume '('
                List<TreeMap<String, Object>> arguments = new ArrayList<>();

                while (current() != null && !")".equals(current().getType())) {
                    TreeMap<String, Object> arg = parseThreadExpression();
                    arguments.add(arg);

                    if (current() != null && ",".equals(current().getType())) {
                        advance(); // consume ','
                    } else if (current() != null && !")".equals(current().getType())) {
                        throw new RuntimeException("Expected ',' or ')' in function call");
                    }
                }

                if (current() == null || !")".equals(current().getType())) {
                    throw new RuntimeException("Expected closing ')' in function call");
                }
                advance(); // consume ')'

                TreeMap<String, Object> call = new TreeMap<>();
                call.put("arguments", arguments);
                call.put("function", expr);
                call.put("type", "Call");

                expr = call;
            }
        }

        return expr;
    }

    private boolean isComparisonOperator(String type) {
        return "==".equals(type) || "!=".equals(type) || ">".equals(type) ||
               "<".equals(type) || ">=".equals(type) || "<=".equals(type);
    }

    private boolean isLogicalOperator(String type) {
        return "&&".equals(type) || "||".equals(type);
    }

    private TreeMap<String, Object> parseListLiteral() {
        advance(); // consume '['

        List<TreeMap<String, Object>> items = new ArrayList<>();

        while (current() != null && !"]".equals(current().getType())) {
            TreeMap<String, Object> item = parseInfixExpression();
            items.add(item);

            if (current() != null && ",".equals(current().getType())) {
                advance(); // consume ','
            } else if (current() != null && !"]".equals(current().getType())) {
                throw new RuntimeException("Expected ',' or ']' in list literal");
            }
        }

        if (current() == null || !"]".equals(current().getType())) {
            throw new RuntimeException("Expected closing ']' in list literal");
        }
        advance(); // consume ']'

        TreeMap<String, Object> list = new TreeMap<>();
        list.put("items", items);
        list.put("type", "List");
        return list;
    }

    private TreeMap<String, Object> parseSetLiteral() {
        advance(); // consume '{'

        List<TreeMap<String, Object>> items = new ArrayList<>();

        while (current() != null && !"}".equals(current().getType())) {
            TreeMap<String, Object> item = parseInfixExpression();
            items.add(item);

            if (current() != null && ",".equals(current().getType())) {
                advance(); // consume ','
            } else if (current() != null && !"}".equals(current().getType())) {
                throw new RuntimeException("Expected ',' or '}' in set literal");
            }
        }

        if (current() == null || !"}".equals(current().getType())) {
            throw new RuntimeException("Expected closing '}' in set literal");
        }
        advance(); // consume '}'

        TreeMap<String, Object> set = new TreeMap<>();
        set.put("items", items);
        set.put("type", "Set");
        return set;
    }

    private TreeMap<String, Object> parseDictLiteral() {
        advance(); // consume '#{'

        List<TreeMap<String, Object>> entries = new ArrayList<>();

        while (current() != null && !"}".equals(current().getType())) {
            TreeMap<String, Object> key = parseInfixExpression();

            if (current() == null || !":".equals(current().getType())) {
                throw new RuntimeException("Expected ':' after dictionary key");
            }
            advance(); // consume ':'

            TreeMap<String, Object> value = parseInfixExpression();

            TreeMap<String, Object> entry = new TreeMap<>();
            entry.put("key", key);
            entry.put("value", value);
            entries.add(entry);

            if (current() != null && ",".equals(current().getType())) {
                advance(); // consume ','
            } else if (current() != null && !"}".equals(current().getType())) {
                throw new RuntimeException("Expected ',' or '}' in dictionary literal");
            }
        }

        if (current() == null || !"}".equals(current().getType())) {
            throw new RuntimeException("Expected closing '}' in dictionary literal");
        }
        advance(); // consume '}'

        TreeMap<String, Object> dict = new TreeMap<>();
        dict.put("items", entries);
        dict.put("type", "Dictionary");
        return dict;
    }

    private TreeMap<String, Object> parseIfExpression() {
        advance(); // consume 'if'

        TreeMap<String, Object> condition = parseInfixExpression();

        TreeMap<String, Object> consequence = parseBlock();

        TreeMap<String, Object> alternative = null;
        if (current() != null && "ELSE".equals(current().getType())) {
            advance(); // consume 'else'
            alternative = parseBlock();
        }

        TreeMap<String, Object> ifExpr = new TreeMap<>();
        ifExpr.put("condition", condition);
        ifExpr.put("consequence", consequence);
        if (alternative != null) {
            ifExpr.put("alternative", alternative);
        }
        ifExpr.put("type", "If");
        return ifExpr;
    }

    private TreeMap<String, Object> parseBlock() {
        if (current() == null || !"{".equals(current().getType())) {
            throw new RuntimeException("Expected opening '{'");
        }
        advance(); // consume '{'

        List<TreeMap<String, Object>> statements = new ArrayList<>();

        while (current() != null && !"}".equals(current().getType())) {
            // Skip comments
            if ("CMT".equals(current().getType())) {
                advance();
                continue;
            }

            TreeMap<String, Object> stmt = parseStatement();
            if (stmt != null) {
                statements.add(stmt);
            }

            // Skip optional semicolons
            if (current() != null && ";".equals(current().getType())) {
                advance();
            }
        }

        if (current() == null || !"}".equals(current().getType())) {
            throw new RuntimeException("Expected closing '}'");
        }
        advance(); // consume '}'

        TreeMap<String, Object> block = new TreeMap<>();
        block.put("statements", statements);
        block.put("type", "Block");
        return block;
    }

    private TreeMap<String, Object> parseFunctionLiteral() {
        advance(); // consume '|'

        List<TreeMap<String, Object>> parameters = new ArrayList<>();

        // Parse parameters
        while (current() != null && !"|".equals(current().getType())) {
            if (!"ID".equals(current().getType())) {
                throw new RuntimeException("Expected parameter name");
            }

            TreeMap<String, Object> param = new TreeMap<>();
            param.put("name", current().getValue());
            param.put("type", "Identifier");
            parameters.add(param);
            advance();

            if (current() != null && ",".equals(current().getType())) {
                advance(); // consume ','
            } else if (current() != null && !"|".equals(current().getType())) {
                throw new RuntimeException("Expected ',' or '|' in parameter list");
            }
        }

        if (current() == null || !"|".equals(current().getType())) {
            throw new RuntimeException("Expected closing '|'");
        }
        advance(); // consume '|'

        TreeMap<String, Object> body;
        if (current() != null && "{".equals(current().getType())) {
            body = parseBlock();
        } else {
            // Single expression body
            TreeMap<String, Object> expr = parseInfixExpression();
            List<TreeMap<String, Object>> statements = new ArrayList<>();
            TreeMap<String, Object> statement = new TreeMap<>();
            statement.put("type", "Expression");
            statement.put("value", expr);
            statements.add(statement);

            body = new TreeMap<>();
            body.put("statements", statements);
            body.put("type", "Block");
        }

        TreeMap<String, Object> function = new TreeMap<>();
        function.put("parameters", parameters);
        function.put("body", body);
        function.put("type", "Function");
        return function;
    }

    private TreeMap<String, Object> parseZeroParameterFunction() {
        advance(); // consume '||'

        // No parameters for zero-parameter function
        List<TreeMap<String, Object>> parameters = new ArrayList<>();

        TreeMap<String, Object> body;
        if (current() != null && "{".equals(current().getType())) {
            body = parseBlock();
        } else {
            // Single expression body
            TreeMap<String, Object> expr = parseInfixExpression();
            List<TreeMap<String, Object>> statements = new ArrayList<>();
            TreeMap<String, Object> statement = new TreeMap<>();
            statement.put("type", "Expression");
            statement.put("value", expr);
            statements.add(statement);

            body = new TreeMap<>();
            body.put("statements", statements);
            body.put("type", "Block");
        }

        TreeMap<String, Object> function = new TreeMap<>();
        function.put("parameters", parameters);
        function.put("body", body);
        function.put("type", "Function");
        return function;
    }
}