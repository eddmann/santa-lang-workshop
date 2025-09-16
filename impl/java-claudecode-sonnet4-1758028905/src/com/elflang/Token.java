package com.elflang;

public class Token {
    private final String type;
    private final String value;

    public Token(String type, String value) {
        this.type = type;
        this.value = value;
    }

    public String getType() {
        return type;
    }

    public String getValue() {
        return value;
    }

    public String toJSON() {
        return "{\"type\":\"" + escapeJson(type) + "\",\"value\":\"" + escapeJson(value) + "\"}";
    }

    private String escapeJson(String str) {
        return str.replace("\\", "\\\\").replace("\"", "\\\"");
    }
}