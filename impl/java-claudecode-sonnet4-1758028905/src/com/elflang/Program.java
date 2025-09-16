package com.elflang;

import com.elflang.json.JSONGenerator;
import java.util.List;
import java.util.TreeMap;

public class Program {
    private final List<TreeMap<String, Object>> statements;

    public Program(List<TreeMap<String, Object>> statements) {
        this.statements = statements;
    }

    public List<TreeMap<String, Object>> getStatements() {
        return statements;
    }

    public String toJSON() {
        JSONGenerator json = new JSONGenerator();
        json.put("statements", statements);
        json.put("type", "Program");
        return json.toJSON();
    }
}