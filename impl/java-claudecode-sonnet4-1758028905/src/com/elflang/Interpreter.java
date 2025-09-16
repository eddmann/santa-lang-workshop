package com.elflang;

import java.util.*;
import java.util.stream.Collectors;

public class Interpreter {
    private final Map<String, Object> variables = new HashMap<>();
    private final Map<String, Boolean> mutableVariables = new HashMap<>();

    public void execute(Program program) {
        List<TreeMap<String, Object>> statements = program.getStatements();
        Object lastResult = null;
        Object lastStandaloneResult = null;

        for (TreeMap<String, Object> statement : statements) {
            Object result = evaluateStatement(statement);

            // Track the last standalone expression result
            if ("Expression".equals(statement.get("type")) && result != null && !wasPutsCall && isStandaloneExpression(statement)) {
                lastStandaloneResult = result;
            }
            wasPutsCall = false; // Reset flag
            lastResult = result;
        }

        // Print the last standalone expression result, or nil if program ends with statements
        if (lastStandaloneResult != null) {
            System.out.println(formatValue(lastStandaloneResult));
        } else if (statements.size() > 0 && lastResult == null) {
            System.out.println("nil");
        }
    }

    private boolean isStandaloneExpression(TreeMap<String, Object> statement) {
        TreeMap<String, Object> expr = (TreeMap<String, Object>) statement.get("value");
        String type = (String) expr.get("type");

        // Only function calls that aren't puts should be printed as standalone expressions
        return "Call".equals(type) && !wasPutsCall;
    }

    private boolean wasPutsCall = false;

    private String formatValue(Object value) {
        if (value == null) {
            return "nil";
        } else if (value instanceof String) {
            return "\"" + value + "\"";
        } else if (value instanceof Boolean) {
            return value.toString();
        } else if (value instanceof Long) {
            return value.toString();
        } else if (value instanceof Double) {
            // Format decimals without unnecessary trailing zeros
            double d = (Double) value;
            if (d == Math.floor(d) && !Double.isInfinite(d)) {
                return String.valueOf((long) d);
            } else {
                return value.toString();
            }
        } else if (value instanceof PersistentVector) {
            PersistentVector<?> list = (PersistentVector<?>) value;
            StringBuilder sb = new StringBuilder("[");
            boolean first = true;
            for (Object item : list) {
                if (!first) sb.append(", ");
                sb.append(formatValue(item));
                first = false;
            }
            sb.append("]");
            return sb.toString();
        } else if (value instanceof PersistentSet) {
            PersistentSet<?> set = (PersistentSet<?>) value;
            StringBuilder sb = new StringBuilder("{");
            boolean first = true;
            // Use sorted elements for deterministic output
            for (Object item : set.getSortedElements()) {
                if (!first) sb.append(", ");
                sb.append(formatValue(item));
                first = false;
            }
            sb.append("}");
            return sb.toString();
        } else if (value instanceof PersistentMap) {
            PersistentMap<?, ?> dict = (PersistentMap<?, ?>) value;
            StringBuilder sb = new StringBuilder("#{");
            boolean first = true;
            // Use sorted entries for deterministic output
            for (Map.Entry<?, ?> entry : dict.getSortedEntries()) {
                if (!first) sb.append(", ");
                sb.append(formatValue(entry.getKey()));
                sb.append(": ");
                sb.append(formatValue(entry.getValue()));
                first = false;
            }
            sb.append("}");
            return sb.toString();
        } else {
            return value.toString();
        }
    }

    private Object evaluateStatement(TreeMap<String, Object> statement) {
        String type = (String) statement.get("type");

        switch (type) {
            case "Expression":
                return evaluateExpression((TreeMap<String, Object>) statement.get("value"));
            case "Comment":
                return null; // Comments don't produce values
            default:
                throw new RuntimeException("Unknown statement type: " + type);
        }
    }

    private Object evaluateExpression(TreeMap<String, Object> expression) {
        String type = (String) expression.get("type");

        switch (type) {
            case "Integer":
                String intValue = (String) expression.get("value");
                // Remove underscores from numeric literals
                intValue = intValue.replace("_", "");
                return Long.parseLong(intValue);

            case "Decimal":
                String decValue = (String) expression.get("value");
                // Remove underscores from numeric literals
                decValue = decValue.replace("_", "");
                return Double.parseDouble(decValue);

            case "String":
                return (String) expression.get("value");

            case "Boolean":
                return (Boolean) expression.get("value");

            case "Nil":
                return null;

            case "Identifier":
                String name = (String) expression.get("name");
                if ("puts".equals(name)) {
                    return puts;
                }
                // Check for built-in functions
                switch (name) {
                    case "push": return new PushFunction();
                    case "assoc": return new AssocFunction();
                    case "first": return new FirstFunction();
                    case "rest": return new RestFunction();
                    case "size": return new SizeFunction();
                    case "map": return new MapFunction();
                    case "filter": return new FilterFunction();
                    case "fold": return new FoldFunction();
                }
                // Check if it's an operator used as a function
                switch (name) {
                    case "+": return new OperatorFunction("+");
                    case "-": return new OperatorFunction("-");
                    case "*": return new OperatorFunction("*");
                    case "/": return new OperatorFunction("/");
                    case ">": return new OperatorFunction(">");
                    case "<": return new OperatorFunction("<");
                    case ">=": return new OperatorFunction(">=");
                    case "<=": return new OperatorFunction("<=");
                    case "==": return new OperatorFunction("==");
                    case "!=": return new OperatorFunction("!=");
                }
                if (!variables.containsKey(name)) {
                    throw new RuntimeException("Identifier can not be found: " + name);
                }
                return variables.get(name);

            case "Call":
                return evaluateCall(expression);

            case "Let":
            case "MutableLet":
                return evaluateLet(expression);

            case "Assignment":
                return evaluateAssignment(expression);

            case "Infix":
                return evaluateInfix(expression);

            case "Unary":
                return evaluateUnary(expression);

            case "List":
                return evaluateList(expression);

            case "Set":
                return evaluateSet(expression);

            case "Dictionary":
                return evaluateDictionary(expression);

            case "Index":
                return evaluateIndex(expression);

            case "Function":
                return evaluateFunction(expression);

            case "Block":
                return evaluateBlock(expression);

            case "If":
                return evaluateIf(expression);

            case "FunctionComposition":
                return evaluateFunctionComposition(expression);

            case "FunctionThread":
                return evaluateFunctionThread(expression);

            default:
                throw new RuntimeException("Unknown expression type: " + type);
        }
    }

    private Object evaluateCall(TreeMap<String, Object> call) {
        TreeMap<String, Object> functionExpr = (TreeMap<String, Object>) call.get("function");
        List<TreeMap<String, Object>> argumentExprs = (List<TreeMap<String, Object>>) call.get("arguments");

        Object function = evaluateExpression(functionExpr);
        Object[] arguments = new Object[argumentExprs.size()];

        for (int i = 0; i < argumentExprs.size(); i++) {
            arguments[i] = evaluateExpression(argumentExprs.get(i));
        }

        if (function instanceof PutsFunction) {
            wasPutsCall = true;
        }

        return callFunction(function, arguments);
    }

    private Object evaluateLet(TreeMap<String, Object> let) {
        TreeMap<String, Object> nameExpr = (TreeMap<String, Object>) let.get("name");
        String varName = (String) nameExpr.get("name");
        Object value = evaluateExpression((TreeMap<String, Object>) let.get("value"));

        variables.put(varName, value);

        // Track mutability - "Let" is immutable, "MutableLet" is mutable
        String letType = (String) let.get("type");
        mutableVariables.put(varName, "MutableLet".equals(letType));

        return value;
    }

    private Object evaluateAssignment(TreeMap<String, Object> assignment) {
        TreeMap<String, Object> nameExpr = (TreeMap<String, Object>) assignment.get("name");
        String varName = (String) nameExpr.get("name");
        Object value = evaluateExpression((TreeMap<String, Object>) assignment.get("value"));

        if (!variables.containsKey(varName)) {
            throw new RuntimeException("Cannot assign to undefined variable: " + varName);
        }

        // Check if variable is mutable
        if (!mutableVariables.getOrDefault(varName, false)) {
            throw new RuntimeException("Variable '" + varName + "' is not mutable");
        }

        variables.put(varName, value);
        return value;
    }

    private Object evaluateInfix(TreeMap<String, Object> infix) {
        Object left = evaluateExpression((TreeMap<String, Object>) infix.get("left"));
        String operator = (String) infix.get("operator");
        Object right = evaluateExpression((TreeMap<String, Object>) infix.get("right"));

        switch (operator) {
            case "+":
                if (left instanceof Long && right instanceof Long) {
                    return (Long) left + (Long) right;
                } else if (left instanceof Number && right instanceof Number) {
                    return ((Number) left).doubleValue() + ((Number) right).doubleValue();
                } else if (left instanceof String || right instanceof String) {
                    // String concatenation - convert any type to string
                    return convertToString(left) + convertToString(right);
                } else if (left instanceof PersistentVector && right instanceof PersistentVector) {
                    @SuppressWarnings("unchecked")
                    PersistentVector<Object> leftList = (PersistentVector<Object>) left;
                    @SuppressWarnings("unchecked")
                    PersistentVector<Object> rightList = (PersistentVector<Object>) right;
                    return leftList.concat(rightList);
                } else if (left instanceof PersistentSet && right instanceof PersistentSet) {
                    @SuppressWarnings("unchecked")
                    PersistentSet<Object> leftSet = (PersistentSet<Object>) left;
                    @SuppressWarnings("unchecked")
                    PersistentSet<Object> rightSet = (PersistentSet<Object>) right;
                    return leftSet.union(rightSet);
                } else if (left instanceof PersistentMap && right instanceof PersistentMap) {
                    @SuppressWarnings("unchecked")
                    PersistentMap<Object, Object> leftMap = (PersistentMap<Object, Object>) left;
                    @SuppressWarnings("unchecked")
                    PersistentMap<Object, Object> rightMap = (PersistentMap<Object, Object>) right;
                    return leftMap.merge(rightMap);
                } else if (left instanceof PersistentVector && right instanceof Number) {
                    throw new RuntimeException("Unsupported operation: List + Integer");
                } else if (left instanceof PersistentSet && right instanceof Number) {
                    throw new RuntimeException("Unsupported operation: Set + Integer");
                }
                break;

            case "-":
                if (left instanceof Long && right instanceof Long) {
                    return (Long) left - (Long) right;
                } else if (left instanceof Number && right instanceof Number) {
                    return ((Number) left).doubleValue() - ((Number) right).doubleValue();
                }
                break;

            case "*":
                if (left instanceof Long && right instanceof Long) {
                    return (Long) left * (Long) right;
                } else if (left instanceof Number && right instanceof Number) {
                    return ((Number) left).doubleValue() * ((Number) right).doubleValue();
                } else if (left instanceof String && right instanceof Number) {
                    // String repetition
                    return repeatString((String) left, (Number) right);
                } else if (left instanceof Number && right instanceof String) {
                    // String repetition (reverse order)
                    return repeatString((String) right, (Number) left);
                }
                // More specific error message for string * non-number
                if (left instanceof String) {
                    throw new RuntimeException("Unsupported operation: String * " +
                        (right != null ? right.getClass().getSimpleName() : "null"));
                } else if (right instanceof String) {
                    throw new RuntimeException("Unsupported operation: " +
                        (left != null ? left.getClass().getSimpleName() : "null") + " * String");
                }
                break;

            case "/":
                if (left instanceof Long && right instanceof Long) {
                    if ((Long) right == 0) {
                        throw new RuntimeException("Division by zero");
                    }
                    return (Long) left / (Long) right; // Integer division truncates
                } else if (left instanceof Number && right instanceof Number) {
                    if (((Number) right).doubleValue() == 0.0) {
                        throw new RuntimeException("Division by zero");
                    }
                    return ((Number) left).doubleValue() / ((Number) right).doubleValue();
                }
                break;

            case ">":
                if (left instanceof Number && right instanceof Number) {
                    return ((Number) left).doubleValue() > ((Number) right).doubleValue();
                }
                break;

            case "<":
                if (left instanceof Number && right instanceof Number) {
                    return ((Number) left).doubleValue() < ((Number) right).doubleValue();
                }
                break;

            case ">=":
                if (left instanceof Number && right instanceof Number) {
                    return ((Number) left).doubleValue() >= ((Number) right).doubleValue();
                }
                break;

            case "<=":
                if (left instanceof Number && right instanceof Number) {
                    return ((Number) left).doubleValue() <= ((Number) right).doubleValue();
                }
                break;

            case "==":
                return structuralEquals(left, right);

            case "!=":
                return !structuralEquals(left, right);

            case "&&":
                // Logical AND - return boolean result
                return isTruthy(left) && isTruthy(right);

            case "||":
                // Logical OR - return boolean result
                return isTruthy(left) || isTruthy(right);
        }

        throw new RuntimeException("Unsupported operation: " + operator + " on " +
                                   (left != null ? left.getClass().getSimpleName() : "null") + " and " +
                                   (right != null ? right.getClass().getSimpleName() : "null"));
    }

    private Object evaluateUnary(TreeMap<String, Object> unary) {
        String operator = (String) unary.get("operator");
        Object operand = evaluateExpression((TreeMap<String, Object>) unary.get("operand"));

        switch (operator) {
            case "-":
                if (operand instanceof Long) {
                    return -(Long) operand;
                } else if (operand instanceof Double) {
                    return -(Double) operand;
                }
                throw new RuntimeException("Cannot negate non-numeric value");

            case "+":
                if (operand instanceof Long || operand instanceof Double) {
                    return operand; // unary + is no-op for numbers
                }
                throw new RuntimeException("Cannot apply unary + to non-numeric value");

            default:
                throw new RuntimeException("Unknown unary operator: " + operator);
        }
    }

    private String convertToString(Object value) {
        if (value == null) {
            return "nil";
        } else if (value instanceof String) {
            return (String) value;
        } else if (value instanceof Boolean) {
            return value.toString();
        } else if (value instanceof Long) {
            return value.toString();
        } else if (value instanceof Double) {
            double d = (Double) value;
            if (d == Math.floor(d) && !Double.isInfinite(d)) {
                return String.valueOf((long) d);
            } else {
                return value.toString();
            }
        } else {
            return value.toString();
        }
    }

    private String repeatString(String str, Number times) {
        if (times instanceof Double) {
            throw new RuntimeException("Unsupported operation: String * Decimal");
        }

        long count = times.longValue();
        if (count < 0) {
            throw new RuntimeException("Unsupported operation: String * Integer (< 0)");
        }

        if (count == 0) {
            return "";
        }

        StringBuilder result = new StringBuilder();
        for (int i = 0; i < count; i++) {
            result.append(str);
        }
        return result.toString();
    }

    private boolean isTruthy(Object value) {
        if (value == null) return false;
        if (value instanceof Boolean) return (Boolean) value;
        if (value instanceof Long) return (Long) value != 0L;
        if (value instanceof Double) return (Double) value != 0.0;
        if (value instanceof String) return !((String) value).isEmpty();
        return true;
    }

    private Object evaluateList(TreeMap<String, Object> list) {
        List<TreeMap<String, Object>> items = (List<TreeMap<String, Object>>) list.get("items");
        List<Object> evaluatedElements = new ArrayList<>();

        for (TreeMap<String, Object> item : items) {
            evaluatedElements.add(evaluateExpression(item));
        }

        return PersistentVector.of(evaluatedElements);
    }

    private Object evaluateSet(TreeMap<String, Object> set) {
        List<TreeMap<String, Object>> items = (List<TreeMap<String, Object>>) set.get("items");
        Set<Object> evaluatedElements = new LinkedHashSet<>();

        for (TreeMap<String, Object> item : items) {
            Object value = evaluateExpression(item);
            // Check if trying to include a Dictionary in a Set literal
            if (value instanceof PersistentMap) {
                throw new RuntimeException("Unable to include a Dictionary within a Set");
            }
            evaluatedElements.add(value);
        }

        return PersistentSet.of(evaluatedElements);
    }

    private Object evaluateDictionary(TreeMap<String, Object> dict) {
        List<TreeMap<String, Object>> items = (List<TreeMap<String, Object>>) dict.get("items");
        Map<Object, Object> evaluatedPairs = new LinkedHashMap<>();

        for (TreeMap<String, Object> item : items) {
            Object key = evaluateExpression((TreeMap<String, Object>) item.get("key"));
            Object value = evaluateExpression((TreeMap<String, Object>) item.get("value"));

            // Check if key is a PersistentMap (not allowed as dictionary key)
            if (key instanceof PersistentMap) {
                throw new RuntimeException("Unable to use a Dictionary as a Dictionary key");
            }

            evaluatedPairs.put(key, value);
        }

        return PersistentMap.of(evaluatedPairs);
    }

    private Object evaluateIndex(TreeMap<String, Object> index) {
        Object target = evaluateExpression((TreeMap<String, Object>) index.get("left"));
        Object indexValue = evaluateExpression((TreeMap<String, Object>) index.get("index"));

        if (target instanceof PersistentVector) {
            PersistentVector<?> list = (PersistentVector<?>) target;
            if (!(indexValue instanceof Long)) {
                String indexType = indexValue == null ? "null" :
                    (indexValue instanceof Double ? "Decimal" :
                     indexValue instanceof Boolean ? "Boolean" :
                     indexValue instanceof String ? "String" : indexValue.getClass().getSimpleName());
                throw new RuntimeException("Unable to perform index operation, found: List[" + indexType + "]");
            }
            return list.get(((Long) indexValue).intValue());
        } else if (target instanceof String) {
            String str = (String) target;
            if (!(indexValue instanceof Long)) {
                String indexType = indexValue == null ? "null" :
                    (indexValue instanceof Double ? "Decimal" :
                     indexValue instanceof Boolean ? "Boolean" :
                     indexValue instanceof String ? "String" : indexValue.getClass().getSimpleName());
                throw new RuntimeException("Unable to perform index operation, found: String[" + indexType + "]");
            }
            int idx = ((Long) indexValue).intValue();
            if (idx < 0) {
                idx = str.length() + idx;
            }
            if (idx < 0 || idx >= str.length()) {
                return null;
            }
            return String.valueOf(str.charAt(idx));
        } else if (target instanceof PersistentMap) {
            @SuppressWarnings("unchecked")
            PersistentMap<Object, Object> dict = (PersistentMap<Object, Object>) target;
            return dict.get(indexValue);
        } else {
            throw new RuntimeException("Cannot index non-indexable type");
        }
    }

    private boolean structuralEquals(Object left, Object right) {
        return Objects.equals(left, right);
    }

    private PutsFunction puts = new PutsFunction();

    private static class PutsFunction {
        public Object call(Object... arguments) {
            for (int i = 0; i < arguments.length; i++) {
                if (i > 0) System.out.print(" ");
                System.out.print(formatValue(arguments[i]));
            }
            System.out.println(" ");
            return null; // puts returns nil
        }

        private String formatValue(Object value) {
            if (value == null) {
                return "nil";
            } else if (value instanceof String) {
                return "\"" + value + "\"";
            } else if (value instanceof Boolean) {
                return value.toString();
            } else if (value instanceof Long) {
                return value.toString();
            } else if (value instanceof Double) {
                // Format decimals without unnecessary trailing zeros
                double d = (Double) value;
                if (d == Math.floor(d) && !Double.isInfinite(d)) {
                    return String.valueOf((long) d);
                } else {
                    return value.toString();
                }
            } else if (value instanceof PersistentVector) {
                PersistentVector<?> list = (PersistentVector<?>) value;
                StringBuilder sb = new StringBuilder("[");
                boolean first = true;
                for (Object item : list) {
                    if (!first) sb.append(", ");
                    sb.append(formatValue(item));
                    first = false;
                }
                sb.append("]");
                return sb.toString();
            } else if (value instanceof PersistentSet) {
                PersistentSet<?> set = (PersistentSet<?>) value;
                StringBuilder sb = new StringBuilder("{");
                boolean first = true;
                // Use sorted elements for deterministic output
                for (Object item : set.getSortedElements()) {
                    if (!first) sb.append(", ");
                    sb.append(formatValue(item));
                    first = false;
                }
                sb.append("}");
                return sb.toString();
            } else if (value instanceof PersistentMap) {
                PersistentMap<?, ?> dict = (PersistentMap<?, ?>) value;
                StringBuilder sb = new StringBuilder("#{");
                boolean first = true;
                // Use sorted entries for deterministic output
                for (Map.Entry<?, ?> entry : dict.getSortedEntries()) {
                    if (!first) sb.append(", ");
                    sb.append(formatValue(entry.getKey()));
                    sb.append(": ");
                    sb.append(formatValue(entry.getValue()));
                    first = false;
                }
                sb.append("}");
                return sb.toString();
            } else {
                return value.toString();
            }
        }
    }

    private static class OperatorFunction {
        private final String operator;

        public OperatorFunction(String operator) {
            this.operator = operator;
        }

        public Object call(Object... arguments) {
            if (arguments.length == 1) {
                // Partial application
                return new PartialOperatorFunction(operator, arguments[0]);
            }
            if (arguments.length != 2) {
                throw new RuntimeException("Operator " + operator + " expects exactly 2 arguments");
            }

            Object left = arguments[0];
            Object right = arguments[1];

            switch (operator) {
                case "+":
                    if (left instanceof Long && right instanceof Long) {
                        return (Long) left + (Long) right;
                    } else if (left instanceof Number && right instanceof Number) {
                        return ((Number) left).doubleValue() + ((Number) right).doubleValue();
                    } else if (left instanceof String || right instanceof String) {
                        return convertToStringStatic(left) + convertToStringStatic(right);
                    } else if (left instanceof PersistentVector && right instanceof PersistentVector) {
                        @SuppressWarnings("unchecked")
                        PersistentVector<Object> leftList = (PersistentVector<Object>) left;
                        @SuppressWarnings("unchecked")
                        PersistentVector<Object> rightList = (PersistentVector<Object>) right;
                        return leftList.concat(rightList);
                    } else if (left instanceof PersistentSet && right instanceof PersistentSet) {
                        @SuppressWarnings("unchecked")
                        PersistentSet<Object> leftSet = (PersistentSet<Object>) left;
                        @SuppressWarnings("unchecked")
                        PersistentSet<Object> rightSet = (PersistentSet<Object>) right;
                        return leftSet.union(rightSet);
                    } else if (left instanceof PersistentMap && right instanceof PersistentMap) {
                        @SuppressWarnings("unchecked")
                        PersistentMap<Object, Object> leftMap = (PersistentMap<Object, Object>) left;
                        @SuppressWarnings("unchecked")
                        PersistentMap<Object, Object> rightMap = (PersistentMap<Object, Object>) right;
                        return leftMap.merge(rightMap);
                    }
                    break;

                case "-":
                    if (left instanceof Long && right instanceof Long) {
                        return (Long) left - (Long) right;
                    } else if (left instanceof Number && right instanceof Number) {
                        return ((Number) left).doubleValue() - ((Number) right).doubleValue();
                    }
                    break;

                case "*":
                    if (left instanceof Long && right instanceof Long) {
                        return (Long) left * (Long) right;
                    } else if (left instanceof Number && right instanceof Number) {
                        return ((Number) left).doubleValue() * ((Number) right).doubleValue();
                    } else if (left instanceof String && right instanceof Number) {
                        return repeatStringStatic((String) left, (Number) right);
                    } else if (left instanceof Number && right instanceof String) {
                        return repeatStringStatic((String) right, (Number) left);
                    }
                    break;

                case "/":
                    if (left instanceof Long && right instanceof Long) {
                        if ((Long) right == 0) {
                            throw new RuntimeException("Division by zero");
                        }
                        return (Long) left / (Long) right;
                    } else if (left instanceof Number && right instanceof Number) {
                        if (((Number) right).doubleValue() == 0.0) {
                            throw new RuntimeException("Division by zero");
                        }
                        return ((Number) left).doubleValue() / ((Number) right).doubleValue();
                    }
                    break;

                case ">":
                    if (left instanceof Number && right instanceof Number) {
                        return ((Number) left).doubleValue() > ((Number) right).doubleValue();
                    }
                    break;

                case "<":
                    if (left instanceof Number && right instanceof Number) {
                        return ((Number) left).doubleValue() < ((Number) right).doubleValue();
                    }
                    break;

                case ">=":
                    if (left instanceof Number && right instanceof Number) {
                        return ((Number) left).doubleValue() >= ((Number) right).doubleValue();
                    }
                    break;

                case "<=":
                    if (left instanceof Number && right instanceof Number) {
                        return ((Number) left).doubleValue() <= ((Number) right).doubleValue();
                    }
                    break;

                case "==":
                    return structuralEqualsStatic(left, right);

                case "!=":
                    return !structuralEqualsStatic(left, right);
            }

            throw new RuntimeException("Unsupported operation: " + operator + " on " +
                                       (left != null ? left.getClass().getSimpleName() : "null") + " and " +
                                       (right != null ? right.getClass().getSimpleName() : "null"));
        }

        private static String convertToStringStatic(Object value) {
            if (value == null) {
                return "nil";
            } else if (value instanceof String) {
                return (String) value;
            } else if (value instanceof Boolean) {
                return value.toString();
            } else if (value instanceof Long) {
                return value.toString();
            } else if (value instanceof Double) {
                double d = (Double) value;
                if (d == Math.floor(d) && !Double.isInfinite(d)) {
                    return String.valueOf((long) d);
                } else {
                    return value.toString();
                }
            } else {
                return value.toString();
            }
        }

        private static String repeatStringStatic(String str, Number times) {
            if (times instanceof Double) {
                throw new RuntimeException("Unsupported operation: String * Decimal");
            }

            long count = times.longValue();
            if (count < 0) {
                throw new RuntimeException("Unsupported operation: String * Integer (< 0)");
            }

            if (count == 0) {
                return "";
            }

            StringBuilder result = new StringBuilder();
            for (int i = 0; i < count; i++) {
                result.append(str);
            }
            return result.toString();
        }

        private static boolean structuralEqualsStatic(Object left, Object right) {
            return Objects.equals(left, right);
        }
    }

    private static class PartialOperatorFunction {
        private final String operator;
        private final Object firstArg;

        public PartialOperatorFunction(String operator, Object firstArg) {
            this.operator = operator;
            this.firstArg = firstArg;
        }

        public Object call(Object... arguments) {
            if (arguments.length == 0) {
                throw new RuntimeException("Partial operator function requires at least one argument");
            }

            // Apply the operator with the first argument and the new argument
            return new OperatorFunction(operator).call(firstArg, arguments[0]);
        }
    }

    private static class PushFunction {
        public Object call(Object... arguments) {
            if (arguments.length == 1) {
                // Partial application
                return new PartialPushFunction(arguments[0]);
            }
            if (arguments.length != 2) {
                throw new RuntimeException("push expects exactly 2 arguments");
            }

            Object value = arguments[0];
            Object collection = arguments[1];

            if (collection instanceof PersistentVector) {
                @SuppressWarnings("unchecked")
                PersistentVector<Object> list = (PersistentVector<Object>) collection;
                return list.push(value);
            } else if (collection instanceof PersistentSet) {
                @SuppressWarnings("unchecked")
                PersistentSet<Object> set = (PersistentSet<Object>) collection;
                return set.push(value);
            } else {
                throw new RuntimeException("push requires a List or Set as second argument");
            }
        }
    }

    private static class PartialPushFunction {
        private final Object value;

        public PartialPushFunction(Object value) {
            this.value = value;
        }

        public Object call(Object... arguments) {
            if (arguments.length == 0) {
                throw new RuntimeException("Partial push function requires a collection argument");
            }

            return new PushFunction().call(value, arguments[0]);
        }
    }

    private static class PartialAssocFunction {
        private final Object key;
        private final Object value;

        public PartialAssocFunction(Object key, Object value) {
            this.key = key;
            this.value = value;
        }

        public Object call(Object... arguments) {
            if (arguments.length == 0) {
                throw new RuntimeException("Partial assoc function requires a dictionary argument");
            }

            return new AssocFunction().call(key, value, arguments[0]);
        }
    }

    private static class AssocFunction {
        public Object call(Object... arguments) {
            if (arguments.length == 2) {
                // Partial application: assoc(key, value) returns a function waiting for dict
                return new PartialAssocFunction(arguments[0], arguments[1]);
            }
            if (arguments.length != 3) {
                throw new RuntimeException("assoc expects exactly 3 arguments");
            }

            Object key = arguments[0];
            Object value = arguments[1];
            Object dict = arguments[2];

            if (dict instanceof PersistentMap) {
                @SuppressWarnings("unchecked")
                PersistentMap<Object, Object> map = (PersistentMap<Object, Object>) dict;
                return map.assoc(key, value);
            } else {
                throw new RuntimeException("assoc requires a Dictionary as third argument");
            }
        }
    }

    private static class FirstFunction {
        public Object call(Object... arguments) {
            if (arguments.length != 1) {
                throw new RuntimeException("first expects exactly 1 argument");
            }

            Object collection = arguments[0];

            if (collection instanceof PersistentVector) {
                @SuppressWarnings("unchecked")
                PersistentVector<Object> list = (PersistentVector<Object>) collection;
                return list.first();
            } else if (collection instanceof String) {
                String str = (String) collection;
                return str.isEmpty() ? null : String.valueOf(str.charAt(0));
            } else {
                throw new RuntimeException("first requires a List or String");
            }
        }
    }

    private static class RestFunction {
        public Object call(Object... arguments) {
            if (arguments.length != 1) {
                throw new RuntimeException("rest expects exactly 1 argument");
            }

            Object collection = arguments[0];

            if (collection instanceof PersistentVector) {
                @SuppressWarnings("unchecked")
                PersistentVector<Object> list = (PersistentVector<Object>) collection;
                return list.rest();
            } else if (collection instanceof String) {
                String str = (String) collection;
                if (str.isEmpty()) {
                    return "";
                } else {
                    return str.substring(1);
                }
            } else {
                throw new RuntimeException("rest requires a List or String");
            }
        }
    }

    private static class SizeFunction {
        public Object call(Object... arguments) {
            if (arguments.length != 1) {
                throw new RuntimeException("size expects exactly 1 argument");
            }

            Object collection = arguments[0];

            if (collection instanceof PersistentVector) {
                return (long) ((PersistentVector<?>) collection).size();
            } else if (collection instanceof PersistentSet) {
                return (long) ((PersistentSet<?>) collection).size();
            } else if (collection instanceof PersistentMap) {
                return (long) ((PersistentMap<?, ?>) collection).size();
            } else if (collection instanceof String) {
                // Count UTF-8 bytes for Unicode size
                return (long) ((String) collection).getBytes(java.nio.charset.StandardCharsets.UTF_8).length;
            } else {
                throw new RuntimeException("size requires a List, Set, Dictionary, or String");
            }
        }
    }

    private Object evaluateBlock(TreeMap<String, Object> block) {
        List<TreeMap<String, Object>> statements = (List<TreeMap<String, Object>>) block.get("statements");
        Object lastResult = null;

        for (TreeMap<String, Object> statement : statements) {
            lastResult = evaluateStatement(statement);
        }

        return lastResult;
    }

    private Object evaluateIf(TreeMap<String, Object> ifExpr) {
        TreeMap<String, Object> condition = (TreeMap<String, Object>) ifExpr.get("condition");
        TreeMap<String, Object> consequence = (TreeMap<String, Object>) ifExpr.get("consequence");
        TreeMap<String, Object> alternative = (TreeMap<String, Object>) ifExpr.get("alternative");

        Object conditionValue = evaluateExpression(condition);

        if (isTruthy(conditionValue)) {
            return evaluateExpression(consequence);
        } else if (alternative != null) {
            return evaluateExpression(alternative);
        } else {
            return null;
        }
    }

    private Object evaluateFunctionComposition(TreeMap<String, Object> composition) {
        List<TreeMap<String, Object>> functions = (List<TreeMap<String, Object>>) composition.get("functions");

        // Evaluate all functions in the composition
        List<Object> evaluatedFunctions = new ArrayList<>();
        for (TreeMap<String, Object> fn : functions) {
            evaluatedFunctions.add(evaluateExpression(fn));
        }

        return new ComposedFunction(evaluatedFunctions);
    }

    private Object evaluateFunctionThread(TreeMap<String, Object> thread) {
        List<TreeMap<String, Object>> functions = (List<TreeMap<String, Object>>) thread.get("functions");
        TreeMap<String, Object> initial = (TreeMap<String, Object>) thread.get("initial");

        // Evaluate the initial value
        Object value = evaluateExpression(initial);

        // Thread through each function
        for (TreeMap<String, Object> fn : functions) {
            Object function = evaluateExpression(fn);
            value = callFunction(function, value);
        }

        return value;
    }

    private Object evaluateFunction(TreeMap<String, Object> function) {
        List<TreeMap<String, Object>> parameters = (List<TreeMap<String, Object>>) function.get("parameters");
        TreeMap<String, Object> body = (TreeMap<String, Object>) function.get("body");

        List<String> paramNames = new ArrayList<>();
        for (TreeMap<String, Object> param : parameters) {
            paramNames.add((String) param.get("name"));
        }

        // Capture current environment for closure
        // Capture all variables, but distinguish between local and global scope
        Map<String, Object> capturedEnv = new HashMap<>(variables);
        Map<String, Boolean> capturedMutability = new HashMap<>(mutableVariables);

        // If we're evaluating inside a function, use the original interpreter from that function
        // Otherwise, use this interpreter as the original
        Interpreter rootInterpreter = this;
        return new UserFunction(paramNames, body, capturedEnv, capturedMutability, rootInterpreter);
    }

    private static class UserFunction {
        private final List<String> parameters;
        private final TreeMap<String, Object> body;
        private final Map<String, Object> capturedEnv;
        private final Map<String, Boolean> capturedMutability;
        private final Interpreter originalInterpreter; // Reference to scope where closure was created

        public UserFunction(List<String> parameters, TreeMap<String, Object> body,
                           Map<String, Object> capturedEnv, Map<String, Boolean> capturedMutability,
                           Interpreter originalInterpreter) {
            this.parameters = parameters;
            this.body = body;
            this.capturedEnv = capturedEnv;
            this.capturedMutability = capturedMutability;
            this.originalInterpreter = originalInterpreter;
        }

        public Object call(Interpreter interpreter, Object... arguments) {
            // Handle partial application and extra arguments
            if (arguments.length < parameters.size()) {
                // Partial application - return a new function with some parameters filled
                List<String> remainingParams = parameters.subList(arguments.length, parameters.size());

                // Create a new function that captures these arguments
                return new PartiallyAppliedFunction(this, arguments, remainingParams);
            }

            // Save current environment
            Map<String, Object> oldVariables = new HashMap<>(interpreter.variables);
            Map<String, Boolean> oldMutability = new HashMap<>(interpreter.mutableVariables);

            try {
                // Restore captured environment (closures)
                interpreter.variables.putAll(capturedEnv);
                interpreter.mutableVariables.putAll(capturedMutability);


                // Bind parameters to arguments (ignoring extra arguments)
                for (int i = 0; i < parameters.size() && i < arguments.length; i++) {
                    interpreter.variables.put(parameters.get(i), arguments[i]);
                    interpreter.mutableVariables.put(parameters.get(i), false); // Parameters are immutable
                }

                // Execute function body
                return interpreter.evaluateExpression(body);
            } finally {
                // Capture changes before restoring old environment
                Map<String, Object> updatedVariables = new HashMap<>();
                for (String key : capturedEnv.keySet()) {
                    if (interpreter.variables.containsKey(key)) {
                        updatedVariables.put(key, interpreter.variables.get(key));
                        capturedEnv.put(key, interpreter.variables.get(key));
                    }
                }

                // Also capture mutable variable changes
                for (Map.Entry<String, Boolean> entry : originalInterpreter.mutableVariables.entrySet()) {
                    if (entry.getValue()) { // If variable is mutable in original scope
                        String varName = entry.getKey();
                        if (interpreter.variables.containsKey(varName)) {
                            updatedVariables.put(varName, interpreter.variables.get(varName));
                        }
                    }
                }

                // Restore previous environment
                interpreter.variables.clear();
                interpreter.variables.putAll(oldVariables);
                interpreter.mutableVariables.clear();
                interpreter.mutableVariables.putAll(oldMutability);

                // Write back changes to original scope AFTER restoring
                for (Map.Entry<String, Object> entry : updatedVariables.entrySet()) {
                    String varName = entry.getKey();
                    if (originalInterpreter.mutableVariables.getOrDefault(varName, false)) {
                        originalInterpreter.variables.put(varName, entry.getValue());
                    }
                }
            }
        }
    }

    private static class PartiallyAppliedFunction {
        private final UserFunction originalFunction;
        private final Object[] appliedArgs;
        private final List<String> remainingParams;

        public PartiallyAppliedFunction(UserFunction originalFunction, Object[] appliedArgs, List<String> remainingParams) {
            this.originalFunction = originalFunction;
            this.appliedArgs = appliedArgs.clone();
            this.remainingParams = remainingParams;
        }

        public Object call(Interpreter interpreter, Object... arguments) {
            // Combine applied and new arguments
            Object[] allArgs = new Object[appliedArgs.length + arguments.length];
            System.arraycopy(appliedArgs, 0, allArgs, 0, appliedArgs.length);
            System.arraycopy(arguments, 0, allArgs, appliedArgs.length, arguments.length);

            return originalFunction.call(interpreter, allArgs);
        }
    }

    private static class ComposedFunction {
        private final List<Object> functions;

        public ComposedFunction(List<Object> functions) {
            this.functions = functions;
        }

        public Object call(Interpreter interpreter, Object... arguments) {
            if (functions.isEmpty()) {
                throw new RuntimeException("Cannot call empty composed function");
            }

            // For right-associative composition: f >> g means g(f(x))
            // Apply functions from left to right
            Object result = arguments[0]; // Start with the first argument

            for (Object function : functions) {
                result = interpreter.callFunction(function, result);
            }

            return result;
        }
    }

    private static class MapFunction {
        public Object call(Interpreter interpreter, Object... arguments) {
            if (arguments.length < 2) {
                // Partial application
                if (arguments.length == 1) {
                    return new PartialMapFunction(arguments[0]);
                }
                throw new RuntimeException("Unexpected argument: map() requires at least 2 arguments");
            }

            Object fn = arguments[0];
            Object list = arguments[1];

            if (!(list instanceof PersistentVector)) {
                throw new RuntimeException("Unexpected argument: map(" + getTypeName(fn) + ", " + getTypeName(list) + ")");
            }

            if (!(fn instanceof UserFunction || fn instanceof PartiallyAppliedFunction ||
                  fn instanceof OperatorFunction || fn instanceof PartialOperatorFunction ||
                  fn instanceof MapFunction || fn instanceof FilterFunction || fn instanceof FoldFunction ||
                  fn instanceof PartialMapFunction || fn instanceof PartialFilterFunction ||
                  fn instanceof PartialFoldFunction1 || fn instanceof PartialFoldFunction2 ||
                  fn instanceof PushFunction || fn instanceof PartialPushFunction ||
                  fn instanceof AssocFunction || fn instanceof FirstFunction || fn instanceof RestFunction ||
                  fn instanceof SizeFunction || fn instanceof ComposedFunction)) {
                throw new RuntimeException("Unexpected argument: map(" + getTypeName(fn) + ", " + getTypeName(list) + ")");
            }

            @SuppressWarnings("unchecked")
            PersistentVector<Object> vector = (PersistentVector<Object>) list;
            PersistentVector<Object> result = PersistentVector.empty();

            for (int i = 0; i < vector.size(); i++) {
                Object element = vector.get(i);
                Object mappedValue = interpreter.callFunction(fn, element);
                result = result.push(mappedValue);
            }

            return result;
        }
    }

    private static class PartialMapFunction {
        private final Object fn;

        public PartialMapFunction(Object fn) {
            this.fn = fn;
        }

        public Object call(Interpreter interpreter, Object... arguments) {
            if (arguments.length == 0) {
                throw new RuntimeException("Unexpected argument: map(Function) requires a list argument");
            }

            return new MapFunction().call(interpreter, fn, arguments[0]);
        }
    }

    private static class FilterFunction {
        public Object call(Interpreter interpreter, Object... arguments) {
            if (arguments.length < 2) {
                if (arguments.length == 1) {
                    return new PartialFilterFunction(arguments[0]);
                }
                throw new RuntimeException("Unexpected argument: filter() requires at least 2 arguments");
            }

            Object fn = arguments[0];
            Object list = arguments[1];

            if (!(list instanceof PersistentVector)) {
                throw new RuntimeException("Unexpected argument: filter(" + getTypeName(fn) + ", " + getTypeName(list) + ")");
            }

            if (!(fn instanceof UserFunction || fn instanceof PartiallyAppliedFunction ||
                  fn instanceof OperatorFunction || fn instanceof PartialOperatorFunction ||
                  fn instanceof MapFunction || fn instanceof FilterFunction || fn instanceof FoldFunction ||
                  fn instanceof PartialMapFunction || fn instanceof PartialFilterFunction ||
                  fn instanceof PartialFoldFunction1 || fn instanceof PartialFoldFunction2 ||
                  fn instanceof PushFunction || fn instanceof PartialPushFunction ||
                  fn instanceof AssocFunction || fn instanceof FirstFunction || fn instanceof RestFunction ||
                  fn instanceof SizeFunction || fn instanceof ComposedFunction)) {
                throw new RuntimeException("Unexpected argument: filter(" + getTypeName(fn) + ", " + getTypeName(list) + ")");
            }

            @SuppressWarnings("unchecked")
            PersistentVector<Object> vector = (PersistentVector<Object>) list;
            PersistentVector<Object> result = PersistentVector.empty();

            for (int i = 0; i < vector.size(); i++) {
                Object element = vector.get(i);
                Object testResult = interpreter.callFunction(fn, element);
                if (interpreter.isTruthy(testResult)) {
                    result = result.push(element);
                }
            }

            return result;
        }
    }

    private static class PartialFilterFunction {
        private final Object fn;

        public PartialFilterFunction(Object fn) {
            this.fn = fn;
        }

        public Object call(Interpreter interpreter, Object... arguments) {
            if (arguments.length == 0) {
                throw new RuntimeException("Unexpected argument: filter(Function) requires a list argument");
            }

            return new FilterFunction().call(interpreter, fn, arguments[0]);
        }
    }

    private static class FoldFunction {
        public Object call(Interpreter interpreter, Object... arguments) {
            if (arguments.length < 3) {
                if (arguments.length == 2) {
                    return new PartialFoldFunction2(arguments[0], arguments[1]);
                } else if (arguments.length == 1) {
                    return new PartialFoldFunction1(arguments[0]);
                }
                throw new RuntimeException("Unexpected argument: fold() requires at least 3 arguments");
            }

            Object init = arguments[0];
            Object fn = arguments[1];
            Object list = arguments[2];

            if (!(list instanceof PersistentVector)) {
                throw new RuntimeException("Unexpected argument: fold(" + getTypeName(init) + ", " + getTypeName(fn) + ", " + getTypeName(list) + ")");
            }

            if (!(fn instanceof UserFunction || fn instanceof PartiallyAppliedFunction ||
                  fn instanceof OperatorFunction || fn instanceof PartialOperatorFunction ||
                  fn instanceof MapFunction || fn instanceof FilterFunction || fn instanceof FoldFunction ||
                  fn instanceof PartialMapFunction || fn instanceof PartialFilterFunction ||
                  fn instanceof PartialFoldFunction1 || fn instanceof PartialFoldFunction2 ||
                  fn instanceof PushFunction || fn instanceof PartialPushFunction ||
                  fn instanceof AssocFunction || fn instanceof FirstFunction || fn instanceof RestFunction ||
                  fn instanceof SizeFunction || fn instanceof ComposedFunction)) {
                throw new RuntimeException("Unexpected argument: fold(" + getTypeName(init) + ", " + getTypeName(fn) + ", " + getTypeName(list) + ")");
            }

            @SuppressWarnings("unchecked")
            PersistentVector<Object> vector = (PersistentVector<Object>) list;
            Object accumulator = init;

            for (int i = 0; i < vector.size(); i++) {
                Object element = vector.get(i);
                accumulator = interpreter.callFunction(fn, accumulator, element);
            }

            return accumulator;
        }
    }

    private static class PartialFoldFunction1 {
        private final Object init;

        public PartialFoldFunction1(Object init) {
            this.init = init;
        }

        public Object call(Interpreter interpreter, Object... arguments) {
            if (arguments.length < 2) {
                if (arguments.length == 1) {
                    return new PartialFoldFunction2(init, arguments[0]);
                }
                throw new RuntimeException("fold requires more arguments");
            }

            return new FoldFunction().call(interpreter, init, arguments[0], arguments[1]);
        }
    }

    private static class PartialFoldFunction2 {
        private final Object init;
        private final Object fn;

        public PartialFoldFunction2(Object init, Object fn) {
            this.init = init;
            this.fn = fn;
        }

        public Object call(Interpreter interpreter, Object... arguments) {
            if (arguments.length == 0) {
                throw new RuntimeException("fold requires a list argument");
            }

            return new FoldFunction().call(interpreter, init, fn, arguments[0]);
        }
    }

    // Helper method to call any function
    private Object callFunction(Object fn, Object... arguments) {
        if (fn instanceof PutsFunction) {
            return ((PutsFunction) fn).call(arguments);
        } else if (fn instanceof OperatorFunction) {
            return ((OperatorFunction) fn).call(arguments);
        } else if (fn instanceof PartialOperatorFunction) {
            return ((PartialOperatorFunction) fn).call(arguments);
        } else if (fn instanceof PushFunction) {
            return ((PushFunction) fn).call(arguments);
        } else if (fn instanceof PartialPushFunction) {
            return ((PartialPushFunction) fn).call(arguments);
        } else if (fn instanceof PartialAssocFunction) {
            return ((PartialAssocFunction) fn).call(arguments);
        } else if (fn instanceof AssocFunction) {
            return ((AssocFunction) fn).call(arguments);
        } else if (fn instanceof FirstFunction) {
            return ((FirstFunction) fn).call(arguments);
        } else if (fn instanceof RestFunction) {
            return ((RestFunction) fn).call(arguments);
        } else if (fn instanceof SizeFunction) {
            return ((SizeFunction) fn).call(arguments);
        } else if (fn instanceof UserFunction) {
            return ((UserFunction) fn).call(this, arguments);
        } else if (fn instanceof PartiallyAppliedFunction) {
            return ((PartiallyAppliedFunction) fn).call(this, arguments);
        } else if (fn instanceof ComposedFunction) {
            return ((ComposedFunction) fn).call(this, arguments);
        } else if (fn instanceof MapFunction) {
            return ((MapFunction) fn).call(this, arguments);
        } else if (fn instanceof PartialMapFunction) {
            return ((PartialMapFunction) fn).call(this, arguments);
        } else if (fn instanceof FilterFunction) {
            return ((FilterFunction) fn).call(this, arguments);
        } else if (fn instanceof PartialFilterFunction) {
            return ((PartialFilterFunction) fn).call(this, arguments);
        } else if (fn instanceof FoldFunction) {
            return ((FoldFunction) fn).call(this, arguments);
        } else if (fn instanceof PartialFoldFunction1) {
            return ((PartialFoldFunction1) fn).call(this, arguments);
        } else if (fn instanceof PartialFoldFunction2) {
            return ((PartialFoldFunction2) fn).call(this, arguments);
        } else {
            throw new RuntimeException("Expected a Function, found: " + getTypeName(fn));
        }
    }

    private static String getTypeName(Object obj) {
        if (obj == null) return "Nil";
        if (obj instanceof Long) return "Integer";
        if (obj instanceof Double) return "Decimal";
        if (obj instanceof String) return "String";
        if (obj instanceof Boolean) return "Boolean";
        if (obj instanceof PersistentVector) return "List";
        if (obj instanceof PersistentSet) return "Set";
        if (obj instanceof PersistentMap) return "Dictionary";
        return "Function";
    }
}