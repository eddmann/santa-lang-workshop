package com.elflang.json;

import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;

public class JSONGenerator {
    private TreeMap<String, Object> fields = new TreeMap<>();

    public JSONGenerator put(String key, Object value) {
        fields.put(key, value);
        return this;
    }

    public String toJSON() {
        return toJSONString(fields, 0);
    }

    private String toJSONString(Object obj, int indent) {
        if (obj == null) {
            return "null";
        }

        if (obj instanceof String) {
            return "\"" + escapeString((String) obj) + "\"";
        }

        if (obj instanceof Boolean || obj instanceof Number) {
            return obj.toString();
        }

        if (obj instanceof List) {
            List<?> list = (List<?>) obj;
            if (list.isEmpty()) {
                return "[]";
            }

            StringBuilder sb = new StringBuilder("[\n");
            for (int i = 0; i < list.size(); i++) {
                sb.append(spaces(indent + 2));
                sb.append(toJSONString(list.get(i), indent + 2));
                if (i < list.size() - 1) {
                    sb.append(",");
                }
                sb.append("\n");
            }
            sb.append(spaces(indent)).append("]");
            return sb.toString();
        }

        if (obj instanceof TreeMap) {
            @SuppressWarnings("unchecked")
            TreeMap<String, Object> map = (TreeMap<String, Object>) obj;
            if (map.isEmpty()) {
                return "{}";
            }

            StringBuilder sb = new StringBuilder("{\n");
            List<String> keys = new ArrayList<>(map.keySet());
            for (int i = 0; i < keys.size(); i++) {
                String key = keys.get(i);
                sb.append(spaces(indent + 2));
                sb.append("\"").append(escapeString(key)).append("\": ");
                sb.append(toJSONString(map.get(key), indent + 2));
                if (i < keys.size() - 1) {
                    sb.append(",");
                }
                sb.append("\n");
            }
            sb.append(spaces(indent)).append("}");
            return sb.toString();
        }

        return obj.toString();
    }

    private String escapeString(String str) {
        return str.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    private String spaces(int count) {
        return " ".repeat(count);
    }
}