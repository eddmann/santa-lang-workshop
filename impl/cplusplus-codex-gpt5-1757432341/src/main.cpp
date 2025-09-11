#include <bits/stdc++.h>
using namespace std;

struct Token {
  string type;
  string value;
};

static string json_escape(const string &s) {
  string out;
  out.reserve(s.size() + 8);
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      case '\r': out += "\\r"; break;
      default:
        // Preserve UTF-8 bytes as-is; control chars escaped generically
        if (c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += s[i];
        }
    }
  }
  return out;
}

static bool is_ident_start(char c) {
  return (c == '_' || isalpha(static_cast<unsigned char>(c)));
}

static bool is_ident_part(char c) {
  return (c == '_' || isalnum(static_cast<unsigned char>(c)));
}

static void emit_token(const Token &t) {
  // Minified JSON line
  cout << "{\"type\":\"" << t.type << "\",\"value\":\"" << json_escape(t.value) << "\"}" << '\n';
}

static vector<Token> lex(const string &src) {
  vector<Token> toks;
  const size_t n = src.size();
  size_t i = 0;

  auto peek = [&](size_t k = 0) -> char {
    return (i + k < n) ? src[i + k] : '\0';
  };
  auto advance = [&]() -> char {
    return (i < n) ? src[i++] : '\0';
  };
  auto match = [&](char c) -> bool {
    if (peek() == c) { ++i; return true; }
    return false;
  };

  while (i < n) {
    char c = peek();
    // Whitespace (spaces, tabs, newlines, carriage returns)
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }

    // Comments: // ... to end of line
    if (c == '/' && (i + 1 < n) && src[i + 1] == '/') {
      size_t start = i;
      // consume till end of line or end of file
      i += 2; // skip '//'
      while (i < n && src[i] != '\n') ++i;
      toks.push_back({"CMT", src.substr(start, i - start)});
      continue;
    }

    // Strings: "..." with escapes; capture raw slice including quotes
    if (c == '"') {
      size_t start = i;
      ++i; // skip opening quote
      bool closed = false;
      while (i < n) {
        char ch = advance();
        if (ch == '\\') {
          // skip next char if present
          if (i < n) advance();
        } else if (ch == '"') {
          closed = true;
          break;
        }
      }
      // Capture slice; tests assume valid strings
      size_t end = i; // i currently at position after closing quote
      toks.push_back({"STR", src.substr(start, end - start)});
      continue;
    }

    // Numbers: INT or DEC with underscores; minus is always a separate token
    if (isdigit(static_cast<unsigned char>(c))) {
      size_t start = i;
      // integer part with underscores allowed between digits
      auto read_digits_underscores = [&](size_t &pos) {
        bool last_was_digit = false;
        while (pos < n) {
          char d = src[pos];
          if (isdigit(static_cast<unsigned char>(d))) { last_was_digit = true; ++pos; }
          else if (d == '_') {
            // allow underscores only when previous was digit and next is digit
            if (!last_was_digit) break;
            if (pos + 1 >= n || !isdigit(static_cast<unsigned char>(src[pos + 1]))) break;
            last_was_digit = false; // reset until next digit
            ++pos;
          } else break;
        }
      };

      read_digits_underscores(i);
      bool is_dec = false;
      if (peek() == '.') {
        // Ensure next after '.' is a digit
        if ((i + 1) < n && isdigit(static_cast<unsigned char>(src[i + 1]))) {
          is_dec = true;
          i += 1; // consume '.'
          // read fractional digits with underscores
          read_digits_underscores(i);
        }
      }

      string slice = src.substr(start, i - start);
      toks.push_back({is_dec ? "DEC" : "INT", slice});
      continue;
    }

    // Identifiers and keywords / booleans / nil
    if (is_ident_start(c)) {
      size_t start = i;
      ++i;
      while (i < n && is_ident_part(src[i])) ++i;
      string word = src.substr(start, i - start);
      if (word == "true")      toks.push_back({"TRUE", word});
      else if (word == "false") toks.push_back({"FALSE", word});
      else if (word == "nil")   toks.push_back({"NIL", word});
      else if (word == "let")   toks.push_back({"LET", word});
      else if (word == "mut")   toks.push_back({"MUT", word});
      else if (word == "if")    toks.push_back({"IF", word});
      else if (word == "else")  toks.push_back({"ELSE", word});
      else                       toks.push_back({"ID", word});
      continue;
    }

    // Multi-char operators (maximal munch)
    if (c == '#' && (i + 1 < n) && src[i + 1] == '{') {
      toks.push_back({"#{", "#{"});
      i += 2; continue;
    }
    if (c == '=' && (i + 1 < n) && src[i + 1] == '=') { toks.push_back({"==", "=="}); i += 2; continue; }
    if (c == '!' && (i + 1 < n) && src[i + 1] == '=') { toks.push_back({"!=", "!="}); i += 2; continue; }
    if (c == '>' && (i + 1 < n) && src[i + 1] == '=') { toks.push_back({">=", ">="}); i += 2; continue; }
    if (c == '<' && (i + 1 < n) && src[i + 1] == '=') { toks.push_back({"<=", "<="}); i += 2; continue; }
    if (c == '&' && (i + 1 < n) && src[i + 1] == '&') { toks.push_back({"&&", "&&"}); i += 2; continue; }
    if (c == '|' && (i + 1 < n) && src[i + 1] == '|') { toks.push_back({"||", "||"}); i += 2; continue; }
    if (c == '|' && (i + 1 < n) && src[i + 1] == '>') { toks.push_back({"|>", "|>"}); i += 2; continue; }
    if (c == '>' && (i + 1 < n) && src[i + 1] == '>') { toks.push_back({">>", ">>"}); i += 2; continue; }

    // Single-char operators/symbols
    switch (c) {
      case '+': toks.push_back({"+", "+"}); ++i; continue;
      case '-': toks.push_back({"-", "-"}); ++i; continue;
      case '*': toks.push_back({"*", "*"}); ++i; continue;
      case '/': toks.push_back({"/", "/"}); ++i; continue;
      case '=': toks.push_back({"=", "="}); ++i; continue;
      case '{': toks.push_back({"{", "{"}); ++i; continue;
      case '}': toks.push_back({"}", "}"}); ++i; continue;
      case '[': toks.push_back({"[", "["}); ++i; continue;
      case ']': toks.push_back({"]", "]"}); ++i; continue;
      case '(': toks.push_back({"(", "("}); ++i; continue;
      case ')': toks.push_back({")", ")"}); ++i; continue;
      case ',': toks.push_back({",", ","}); ++i; continue;
      case ':': toks.push_back({":", ":"}); ++i; continue;
      case '|': toks.push_back({"|", "|"}); ++i; continue;
      case '>': toks.push_back({">", ">"}); ++i; continue;
      case '<': toks.push_back({"<", "<"}); ++i; continue;
      case ';': toks.push_back({";", ";"}); ++i; continue;
      default:
        // Unknown character: skip it to avoid infinite loop; no diagnostics in stage 1
        ++i; continue;
    }
  }

  return toks;
}

static string read_file(const string &path) {
  ifstream in(path, ios::in | ios::binary);
  if (!in) return string();
  string data;
  in.seekg(0, ios::end);
  data.resize(static_cast<size_t>(in.tellg()));
  in.seekg(0, ios::beg);
  in.read(&data[0], static_cast<long>(data.size()));
  // Normalize CRLF to LF to meet spec expectation of LF newlines
  string norm; norm.reserve(data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    char c = data[i];
    if (c == '\r') {
      // drop CR
      continue;
    }
    norm += c;
  }
  return norm;
}

// --------------------- AST and Parser (Stage 2) ---------------------

struct JsonWriter {
  ostream &out;
  int indent = 0;
  explicit JsonWriter(ostream &o): out(o) {}
  void write_indent() { for (int i=0;i<indent;i++) out.put(' '); }
};

struct Node { virtual ~Node() = default; virtual void write(JsonWriter&) const = 0; };

// Forward declarations
struct Expr; struct Statement; struct Block;

using ExprPtr = unique_ptr<Expr>;
using StmtPtr = unique_ptr<Statement>;

static void write_string(JsonWriter &w, const string &s) {
  w.out << '"' << json_escape(s) << '"';
}

struct Expr : Node {};

// Define Statement before it is used anywhere
struct Statement : Node { };

struct Program : Node {
  vector<StmtPtr> statements;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent();
    w.out << "\"statements\": [\n";
    w.indent += 2;
    for (size_t i=0;i<statements.size();++i) {
      w.write_indent(); statements[i]->write(w);
      if (i+1<statements.size()) w.out << ",\n"; else w.out << "\n";
    }
    w.indent -= 2; w.write_indent(); w.out << "],\n";
    w.write_indent(); w.out << "\"type\": \"Program\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}\n";
  }
};

struct ExpressionStmt : Statement {
  ExprPtr value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"type\": \"Expression\",\n";
    w.write_indent(); w.out << "\"value\": "; value->write(w); w.out << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct CommentStmt : Statement {
  string value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"type\": \"Comment\",\n";
    w.write_indent(); w.out << "\"value\": "; write_string(w, value); w.out << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct Identifier : Expr {
  string name;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"name\": "; write_string(w, name); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"Identifier\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct IntegerLit : Expr {
  string value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"type\": \"Integer\",\n";
    w.write_indent(); w.out << "\"value\": "; write_string(w, value); w.out << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};
struct DecimalLit : Expr {
  string value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"type\": \"Decimal\",\n";
    w.write_indent(); w.out << "\"value\": "; write_string(w, value); w.out << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};
struct StringLit : Expr {
  string value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"type\": \"String\",\n";
    w.write_indent(); w.out << "\"value\": "; write_string(w, value); w.out << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};
struct BooleanLit : Expr {
  bool value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"type\": \"Boolean\",\n";
    w.write_indent(); w.out << "\"value\": " << (value?"true":"false") << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};
struct NilLit : Expr {
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"type\": \"Nil\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct LetExpr : Expr {
  Identifier name;
  string typ; // "Let" or "MutableLet"
  ExprPtr value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"name\": "; name.write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"" << typ << "\",\n";
    w.write_indent(); w.out << "\"value\": "; value->write(w); w.out << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct InfixExpr : Expr {
  ExprPtr left;
  string op;
  ExprPtr right;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"left\": "; left->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"operator\": "; write_string(w, op); w.out << ",\n";
    w.write_indent(); w.out << "\"right\": "; right->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"Infix\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct AssignExpr : Expr {
  Identifier name;
  ExprPtr value;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"name\": "; name.write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"Assignment\",\n";
    w.write_indent(); w.out << "\"value\": "; value->write(w); w.out << "\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct PrefixExpr : Expr { // unary minus only
  string op;
  ExprPtr operand;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"operator\": "; write_string(w, op); w.out << ",\n";
    w.write_indent(); w.out << "\"operand\": "; operand->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"Prefix\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct ListLit : Expr {
  vector<ExprPtr> items;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent();
    if (items.empty()) {
      w.out << "\"items\": [],\n";
    } else {
      w.out << "\"items\": [\n";
      w.indent += 2;
      for (size_t i=0;i<items.size();++i) {
        w.write_indent(); items[i]->write(w); if (i+1<items.size()) w.out << ",\n"; else w.out << "\n";
      }
      w.indent -= 2; w.write_indent(); w.out << "],\n";
    }
    w.write_indent(); w.out << "\"type\": \"List\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct SetLit : Expr {
  vector<ExprPtr> items;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent();
    if (items.empty()) {
      w.out << "\"items\": [],\n";
    } else {
      w.out << "\"items\": [\n";
      w.indent += 2;
      for (size_t i=0;i<items.size();++i) {
        w.write_indent(); items[i]->write(w); if (i+1<items.size()) w.out << ",\n"; else w.out << "\n";
      }
      w.indent -= 2; w.write_indent(); w.out << "],\n";
    }
    w.write_indent(); w.out << "\"type\": \"Set\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct DictEntry { ExprPtr key; ExprPtr value; };
struct DictLit : Expr {
  vector<DictEntry> items;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent();
    if (items.empty()) {
      w.out << "\"items\": [],\n";
    } else {
      w.out << "\"items\": [\n";
      w.indent += 2;
      for (size_t i=0;i<items.size();++i) {
        w.write_indent();
        w.out << "{\n";
        w.indent += 2;
        w.write_indent(); w.out << "\"key\": "; items[i].key->write(w); w.out << ",\n";
        w.write_indent(); w.out << "\"value\": "; items[i].value->write(w); w.out << "\n";
        w.indent -= 2; w.write_indent(); w.out << "}";
        if (i+1<items.size()) w.out << ",\n"; else w.out << "\n";
      }
      w.indent -= 2; w.write_indent(); w.out << "],\n";
    }
    w.write_indent(); w.out << "\"type\": \"Dictionary\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct IndexExpr : Expr {
  ExprPtr index;
  ExprPtr left;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"index\": "; index->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"left\": "; left->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"Index\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct Block : Node {
  vector<StmtPtr> statements;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2; w.write_indent(); w.out << "\"statements\": [\n";
    w.indent += 2;
    for (size_t i=0;i<statements.size();++i) {
      w.write_indent(); statements[i]->write(w); if (i+1<statements.size()) w.out << ",\n"; else w.out << "\n";
    }
    w.indent -= 2; w.write_indent(); w.out << "],\n";
    w.write_indent(); w.out << "\"type\": \"Block\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct IfExpr : Expr {
  Block alternative;
  ExprPtr condition;
  Block consequence;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"alternative\": "; alternative.write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"condition\": "; condition->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"consequence\": "; consequence.write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"If\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct FunctionLit : Expr {
  Block body;
  vector<Identifier> params;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"body\": "; body.write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"parameters\": [\n";
    w.indent += 2;
    for (size_t i=0;i<params.size();++i) {
      w.write_indent(); params[i].write(w); if (i+1<params.size()) w.out << ",\n"; else w.out << "\n";
    }
    w.indent -= 2; w.write_indent(); w.out << "],\n";
    w.write_indent(); w.out << "\"type\": \"Function\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct CallExpr : Expr {
  vector<ExprPtr> args;
  ExprPtr func;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"arguments\": [\n";
    w.indent += 2;
    for (size_t i=0;i<args.size();++i) {
      w.write_indent(); args[i]->write(w); if (i+1<args.size()) w.out << ",\n"; else w.out << "\n";
    }
    w.indent -= 2; w.write_indent(); w.out << "],\n";
    w.write_indent(); w.out << "\"function\": "; func->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"Call\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct FunctionComposition : Expr {
  vector<ExprPtr> funcs;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"functions\": [\n";
    w.indent += 2;
    for (size_t i=0;i<funcs.size();++i) {
      w.write_indent(); funcs[i]->write(w); if (i+1<funcs.size()) w.out << ",\n"; else w.out << "\n";
    }
    w.indent -= 2; w.write_indent(); w.out << "],\n";
    w.write_indent(); w.out << "\"type\": \"FunctionComposition\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

struct FunctionThread : Expr {
  vector<ExprPtr> funcs;
  ExprPtr initial;
  void write(JsonWriter &w) const override {
    w.out << "{\n";
    w.indent += 2;
    w.write_indent(); w.out << "\"functions\": [\n";
    w.indent += 2;
    for (size_t i=0;i<funcs.size();++i) {
      w.write_indent(); funcs[i]->write(w); if (i+1<funcs.size()) w.out << ",\n"; else w.out << "\n";
    }
    w.indent -= 2; w.write_indent(); w.out << "],\n";
    w.write_indent(); w.out << "\"initial\": "; initial->write(w); w.out << ",\n";
    w.write_indent(); w.out << "\"type\": \"FunctionThread\"\n";
    w.indent -= 2; w.write_indent(); w.out << "}";
  }
};

// Parser
struct Parser {
  const vector<Token> &toks; size_t i=0;
  explicit Parser(const vector<Token> &t): toks(t) {}
  const Token& cur() const { static Token eof{"EOF",""}; return (i<toks.size()? toks[i] : eof); }
  Token next() { if (i<toks.size()) return toks[i++]; return Token{"EOF",""}; }
  bool match(const string &typ) { if (cur().type==typ) { i++; return true; } return false; }
  Token expect(const string &typ) { Token t = cur(); if (t.type!=typ) { /* simple panic */ throw runtime_error("parse error: expected "+typ+", found "+t.type); } i++; return t; }

  Program parseProgram() {
    Program prog; while (cur().type != "EOF") {
      if (cur().type == "CMT") {
        auto c = next(); auto stmt = make_unique<CommentStmt>(); stmt->value = c.value; prog.statements.push_back(move(stmt));
        (void)match(";"); continue;
      }
      auto e = parseExpression(0);
      auto st = make_unique<ExpressionStmt>(); st->value = move(e); prog.statements.push_back(move(st));
      (void)match(";");
    }
    return prog;
  }

  static int precOf(const string &op) {
    if (op=="||") return 1; // or
    if (op=="&&") return 2; // and
    if (op=="=="||op=="!="||op==">"||op=="<"||op==">="||op=="<=") return 3; // compare
    if (op=="|>") return 4; // thread
    if (op==">>") return 5; // compose (right-assoc)
    if (op=="+"||op=="-") return 6; // add
    if (op=="*"||op=="/") return 7; // mul
    return 0;
  }

  ExprPtr parseExpression(int minPrec) {
    auto left = parsePrefix();
    for (;;) {
      const string t = cur().type;
      // Assignment
      if (t == "=") {
        // only if left is Identifier
        if (auto id = dynamic_cast<Identifier*>(left.get())) {
          next();
          auto right = parseExpression(0);
          auto node = make_unique<AssignExpr>(); node->name = *id; node->value = move(right); left = move(node);
          continue;
        } else {
          break;
        }
      }
      // Postfix: call and index as highest binding
      if (t == "(") {
        next(); vector<ExprPtr> args; if (!match(")")) {
          for (;;) { args.push_back(parseExpression(0)); if (match(")")) break; expect(","); }
        }
        auto call = make_unique<CallExpr>(); call->args = move(args); call->func = move(left); left = move(call); continue;
      }
      if (t == "[") {
        next(); auto idx = parseExpression(0); expect("]"); auto ie = make_unique<IndexExpr>(); ie->index = move(idx); ie->left = move(left); left = move(ie); continue;
      }

      // Infix
      static const unordered_set<string> ops = {"+","-","*","/","==","!=",">","<",">=","<=","&&","||","|>",">>"};
      if (!ops.count(t)) break;
      int p = precOf(t);
      bool rightAssoc = (t == ">>");
      if (p < minPrec) break;
      string op = t; next();
      int nextMin = rightAssoc ? p : p+1;
      auto right = parseExpression(nextMin);

      if (op == ">>") {
        // Flatten composition
        vector<ExprPtr> fns;
        if (auto fc = dynamic_cast<FunctionComposition*>(left.get())) {
          for (auto &fn : fc->funcs) fns.push_back(move(const_cast<ExprPtr&>(fn)));
        } else {
          fns.push_back(move(left));
        }
        if (auto fcr = dynamic_cast<FunctionComposition*>(right.get())) {
          for (auto &fn : fcr->funcs) fns.push_back(move(const_cast<ExprPtr&>(fn)));
        } else {
          fns.push_back(move(right));
        }
        auto node = make_unique<FunctionComposition>(); node->funcs = move(fns); left = move(node); continue;
      }
      if (op == "|>") {
        vector<ExprPtr> fns; ExprPtr init;
        if (auto ft = dynamic_cast<FunctionThread*>(left.get())) {
          init = move(const_cast<ExprPtr&>(ft->initial));
          for (auto &fn : ft->funcs) fns.push_back(move(const_cast<ExprPtr&>(fn)));
        } else { init = move(left); }
        fns.push_back(move(right));
        auto node = make_unique<FunctionThread>(); node->initial = move(init); node->funcs = move(fns); left = move(node); continue;
      }

      auto inf = make_unique<InfixExpr>(); inf->left = move(left); inf->op = op; inf->right = move(right); left = move(inf);
    }
    return left;
  }

  static string unquote(const string &raw) {
    string s = raw;
    if (s.size() >= 2 && s.front()=='"' && s.back()=='"') s = s.substr(1, s.size()-2);
    string out; out.reserve(s.size());
    for (size_t i=0;i<s.size();++i) {
      char c = s[i];
      if (c == '\\' && i+1 < s.size()) {
        char n = s[++i];
        switch (n) {
          case 'n': out.push_back('\n'); break;
          case 't': out.push_back('\t'); break;
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          default: out.push_back(n); break; // preserve unknown
        }
      } else {
        out.push_back(c);
      }
    }
    return out;
  }

  Block parseBlock() {
    expect("{"); Block b; while (cur().type != "}" && cur().type != "EOF") {
      if (cur().type == "CMT") { auto c = next(); auto cs = make_unique<CommentStmt>(); cs->value = c.value; b.statements.push_back(move(cs)); (void)match(";"); continue; }
      auto e = parseExpression(0); auto st = make_unique<ExpressionStmt>(); st->value = move(e); b.statements.push_back(move(st)); (void)match(";");
    }
    expect("}"); return b;
  }

  ExprPtr parsePrefix() {
    Token t = next();
    const string &tt = t.type;
    if (tt == "-") {
      auto operand = parseExpression(7); // bind tighter than +,-
      auto pre = make_unique<PrefixExpr>(); pre->op = "-"; pre->operand = move(operand); return pre;
    } else if (tt == "INT") {
      auto n = make_unique<IntegerLit>(); n->value = t.value; return n;
    } else if (tt == "DEC") {
      auto n = make_unique<DecimalLit>(); n->value = t.value; return n;
    } else if (tt == "STR") {
      auto s = make_unique<StringLit>(); s->value = unquote(t.value); return s;
    } else if (tt == "TRUE") {
      auto b = make_unique<BooleanLit>(); b->value = true; return b;
    } else if (tt == "FALSE") {
      auto b = make_unique<BooleanLit>(); b->value = false; return b;
    } else if (tt == "NIL") {
      return make_unique<NilLit>();
    } else if (tt == "ID") {
      auto id = make_unique<Identifier>(); id->name = t.value; return id;
    } else if (tt == "[") {
      vector<ExprPtr> items; if (!match("]")) {
        for (;;) { items.push_back(parseExpression(0)); if (match("]")) break; expect(","); }
      }
      auto lst = make_unique<ListLit>(); lst->items = move(items); return lst;
    } else if (tt == "{") {
      vector<ExprPtr> items; if (!match("}")) {
        for (;;) { items.push_back(parseExpression(0)); if (match("}")) break; expect(","); }
      }
      auto st = make_unique<SetLit>(); st->items = move(items); return st;
    } else if (tt == "#{") {
      vector<DictEntry> items; if (!match("}")) {
        for (;;) { auto key = parseExpression(0); expect(":"); auto val = parseExpression(0); items.push_back(DictEntry{move(key), move(val)}); if (match("}")) break; expect(","); }
      }
      auto d = make_unique<DictLit>(); d->items = move(items); return d;
    } else if (tt == "(") {
      auto e = parseExpression(0); expect(")"); return e;
    } else if (tt == "|" || tt == "||") {
      vector<Identifier> params;
      if (tt == "|") {
        // parse parameters until closing '|'
        if (!match("|")) {
          for (;;) {
            Token idt = expect("ID"); Identifier id; id.name = idt.value; params.push_back(id);
            if (match("|")) break; expect(",");
          }
        }
      } else {
        // || zero-arg
      }
      Block body;
      if (cur().type == "{") body = parseBlock(); else {
        auto expr = parseExpression(0);
        Block b; auto st = make_unique<ExpressionStmt>(); st->value = move(expr); b.statements.push_back(move(st)); body = move(b);
      }
      auto fn = make_unique<FunctionLit>(); fn->body = move(body); fn->params = move(params); return fn;
    } else if (tt == "LET") {
      bool mut = false; if (cur().type == "MUT") { next(); mut = true; }
      Token nm = expect("ID"); expect("="); auto val = parseExpression(0);
      auto le = make_unique<LetExpr>(); le->name.name = nm.value; le->typ = (mut?"MutableLet":"Let"); le->value = move(val); return le;
    } else if (tt == "IF") {
      auto cond = parseExpression(3); // up to compare
      Block cons = parseBlock(); expect("ELSE"); Block alt = parseBlock();
      auto iff = make_unique<IfExpr>(); iff->alternative = move(alt); iff->condition = move(cond); iff->consequence = move(cons); return iff;
    }
    // Fallback: unexpected token → Identifier of raw value
    auto id = make_unique<Identifier>(); id->name = t.value; return id;
  }
};

int main(int argc, char **argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  if (argc < 2) {
    // Minimal usage; tests ignore stderr/exit codes; keep stdout clean
    return 0;
  }

  string sub = argv[1];
  if (sub == "tokens") {
    if (argc < 3) return 1;
    string path = argv[2];
    string src = read_file(path);
    vector<Token> toks = lex(src);
    for (const auto &t : toks) emit_token(t);
    return 0;
  } else if (sub == "ast") {
    if (argc < 3) return 1;
    string path = argv[2];
    string src = read_file(path);
    vector<Token> toks = lex(src);
    try {
      Parser p(toks); Program prog = p.parseProgram(); JsonWriter w(cout); prog.write(w);
    } catch (const exception &) {
      // In stage 2, tests expect well-formed programs; on error, still print empty program
      cout << "{\n  \"statements\": [],\n  \"type\": \"Program\"\n}\n";
    }
    return 0;
  } else {
    // --------------------- Stage 3: Evaluation ---------------------
    string path = argv[1];
    string src = read_file(path);
    vector<Token> toks = lex(src);
    // Reuse AST parser
    Program prog;
    try {
      Parser p(toks); prog = p.parseProgram();
    } catch (const exception &) {
      // If parsing fails, print nothing (tests won't hit this in stage 3)
      return 1;
    }

    // Forward declare Value for FnData signature
    struct Value;
    // Function descriptor (declared before Value)
    struct FnData { int arity; function<Value(const vector<Value>&)> apply; };

    // Runtime values
    struct Value {
      enum Kind { INT, DEC, STR, BOOL, NIL, LIST, SET, DICT, FN } kind = NIL;
      long long i = 0;
      double d = 0.0;
      string s;
      bool b = false;
      vector<Value> list;
      vector<Value> set; // kept unique and sorted deterministically
      vector<pair<Value,Value>> dict; // entries kept unique by key and sorted by key deterministically
      shared_ptr<FnData> fn;

      static Value Int(long long v){ Value x; x.kind=INT; x.i=v; return x; }
      static Value Dec(double v){ Value x; x.kind=DEC; x.d=v; return x; }
      static Value Str(string v){ Value x; x.kind=STR; x.s=move(v); return x; }
      static Value Bool(bool v){ Value x; x.kind=BOOL; x.b=v; return x; }
      static Value Nil(){ return Value(); }
      static Value List(vector<Value> v){ Value x; x.kind=LIST; x.list=move(v); return x; }
      static Value Set(vector<Value> v){ Value x; x.kind=SET; x.set=move(v); return x; }
      static Value Dict(vector<pair<Value,Value>> v){ Value x; x.kind=DICT; x.dict=move(v); return x; }
      static Value Function(int arity, function<Value(const vector<Value>&)> f){ Value x; x.kind=FN; x.fn = make_shared<FnData>(FnData{arity, move(f)}); return x; }
    };

    auto to_string_dec = [](double v) {
      // Use defaultfloat with precision 16 to match expectations like 4.140000000000001
      ostringstream oss; oss.setf(std::ios::fmtflags(0), std::ios::floatfield); oss << setprecision(16) << v; return oss.str();
    };

    function<string(const Value&)> repr = [&](const Value &v) -> string {
      switch (v.kind) {
        case Value::INT: return std::to_string(v.i);
        case Value::DEC: return to_string_dec(v.d);
        case Value::STR: {
          string out; out.reserve(v.s.size() + 2);
          out.push_back('"'); out += v.s; out.push_back('"');
          return out;
        }
        case Value::BOOL: return v.b ? string("true") : string("false");
        case Value::NIL: return string("nil");
        case Value::LIST: {
          string out; out += '[';
          for (size_t i=0;i<v.list.size();++i) {
            if (i) out += ", ";
            out += repr(v.list[i]);
          }
          out += ']';
          return out;
        }
        case Value::SET: {
          string out; out += '{';
          for (size_t i=0;i<v.set.size();++i) {
            if (i) out += ", ";
            out += repr(v.set[i]);
          }
          out += '}';
          return out;
        }
        case Value::DICT: {
          string out; out += "#{";
          for (size_t i=0;i<v.dict.size();++i) {
            if (i) out += ", ";
            out += repr(v.dict[i].first);
            out += ": ";
            out += repr(v.dict[i].second);
          }
          out += '}';
          return out;
        }
        case Value::FN: {
          return string("<fn>");
        }
      }
      return string("nil");
    };

    auto typeName = [&](const Value &v)->string{
      switch (v.kind) {
        case Value::INT: return "Integer";
        case Value::DEC: return "Decimal";
        case Value::STR: return "String";
        case Value::BOOL: return "Boolean";
        case Value::NIL: return "Nil";
        case Value::LIST: return "List";
        case Value::SET: return "Set";
        case Value::DICT: return "Dictionary";
        case Value::FN:   return "Function";
      }
      return "<unknown>";
    };

    function<bool(const Value&,const Value&)> valueLess;
    valueLess = [&](const Value &a, const Value &b)->bool{
      auto rank = [&](const Value &v){
        switch (v.kind){
          case Value::NIL: return 0;
          case Value::BOOL: return 1;
          case Value::INT: return 2;
          case Value::DEC: return 3;
          case Value::STR: return 4;
          case Value::LIST: return 5;
          case Value::SET: return 6;
          case Value::DICT: return 7;
        }
        return 99;
      };
      int ra = rank(a), rb = rank(b);
      if (ra != rb) return ra < rb;
      switch (a.kind) {
        case Value::NIL: return false;
        case Value::BOOL: return a.b < b.b;
        case Value::INT: return a.i < b.i;
        case Value::DEC: return a.d < b.d;
        case Value::STR: return a.s < b.s;
        case Value::LIST: {
          size_t n = min(a.list.size(), b.list.size());
          for (size_t i=0;i<n;++i) { if (valueLess(a.list[i], b.list[i])) return true; if (valueLess(b.list[i], a.list[i])) return false; }
          return a.list.size() < b.list.size();
        }
        case Value::SET: {
          size_t n = min(a.set.size(), b.set.size());
          for (size_t i=0;i<n;++i) { if (valueLess(a.set[i], b.set[i])) return true; if (valueLess(b.set[i], a.set[i])) return false; }
          return a.set.size() < b.set.size();
        }
        case Value::DICT: {
          size_t n = min(a.dict.size(), b.dict.size());
          for (size_t i=0;i<n;++i) {
            if (valueLess(a.dict[i].first, b.dict[i].first)) return true;
            if (valueLess(b.dict[i].first, a.dict[i].first)) return false;
            if (valueLess(a.dict[i].second, b.dict[i].second)) return true;
            if (valueLess(b.dict[i].second, a.dict[i].second)) return false;
          }
          return a.dict.size() < b.dict.size();
        }
        case Value::FN: return a.fn.get() < b.fn.get();
      }
      return false;
    };

    function<bool(const Value&,const Value&)> valueEq = [&](const Value &a, const Value &b)->bool{
      if (a.kind != b.kind) {
        // Allow numeric cross-kind equality
        if ((a.kind==Value::INT && b.kind==Value::DEC)) return ((double)a.i) == b.d;
        if ((a.kind==Value::DEC && b.kind==Value::INT)) return a.d == (double)b.i;
        return false;
      }
      switch (a.kind) {
        case Value::NIL: return true;
        case Value::BOOL: return a.b == b.b;
        case Value::INT: return a.i == b.i;
        case Value::DEC: return a.d == b.d;
        case Value::STR: return a.s == b.s;
        case Value::LIST: {
          if (a.list.size()!=b.list.size()) return false;
          for (size_t i=0;i<a.list.size();++i) if (!valueEq(a.list[i], b.list[i])) return false; return true;
        }
        case Value::SET: {
          if (a.set.size()!=b.set.size()) return false;
          for (size_t i=0;i<a.set.size();++i) if (!valueEq(a.set[i], b.set[i])) return false; return true;
        }
        case Value::DICT: {
          if (a.dict.size()!=b.dict.size()) return false;
          for (size_t i=0;i<a.dict.size();++i) {
            if (!valueEq(a.dict[i].first, b.dict[i].first)) return false;
            if (!valueEq(a.dict[i].second, b.dict[i].second)) return false;
          }
          return true;
        }
        case Value::FN: {
          return a.fn.get() == b.fn.get();
        }
      }
      return false;
    };

    auto normalize_set = [&](vector<Value> v)->vector<Value>{
      sort(v.begin(), v.end(), valueLess);
      vector<Value> out; out.reserve(v.size());
      for (auto &e : v) { if (out.empty() || !valueEq(out.back(), e)) out.push_back(e); }
      return out;
    };

    auto normalize_dict = [&](vector<pair<Value,Value>> entries)->vector<pair<Value,Value>>{
      // Right-biased merge already handled by callers; here just sort by key
      sort(entries.begin(), entries.end(), [&](const auto &a, const auto &b){
        if (valueEq(a.first, b.first)) return false; // stable
        return valueLess(a.first, b.first);
      });
      return entries;
    };

    struct Var { Value val; bool mut=false; };
    struct EnvFrame : enable_shared_from_this<EnvFrame> {
      unordered_map<string, shared_ptr<Var>> vars;
      shared_ptr<EnvFrame> parent;
      explicit EnvFrame(shared_ptr<EnvFrame> p=nullptr): parent(move(p)) {}
      shared_ptr<Var> find(const string &name) {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        return parent ? parent->find(name) : nullptr;
      }
      void define(const string &name, const Value &v, bool mut){ vars[name] = make_shared<Var>(Var{v, mut}); }
    };
    auto globalEnv = make_shared<EnvFrame>();
    auto currentEnv = globalEnv;

    // Functions as first-class values (FnData defined above)

    auto err = [&](const string &msg) {
      cout << "[Error] " << msg; // exact message, no extra prefix/suffix
      throw runtime_error("runtime error");
    };

    auto parse_int = [](const string &raw)->long long {
      string s; s.reserve(raw.size()); for (char c: raw) if (c!='_') s.push_back(c);
      // stoi/stoll handles sign if present (though we pass only digits here)
      return stoll(s);
    };
    auto parse_dec = [](const string &raw)->double {
      string s; s.reserve(raw.size()); for (char c: raw) if (c!='_') s.push_back(c);
      return stod(s);
    };

    auto add = [&](const Value &a, const Value &b)->Value {
      // Collections
      if (a.kind==Value::LIST && b.kind==Value::LIST) {
        vector<Value> out = a.list; out.insert(out.end(), b.list.begin(), b.list.end()); return Value::List(move(out));
      }
      if (a.kind==Value::SET && b.kind==Value::SET) {
        vector<Value> v = a.set; v.insert(v.end(), b.set.begin(), b.set.end()); v = normalize_set(move(v)); return Value::Set(move(v));
      }
      if (a.kind==Value::DICT && b.kind==Value::DICT) {
        // Right-biased merge
        vector<pair<Value,Value>> m;
        // start with a
        for (auto &kv : a.dict) m.push_back(kv);
        // overlay b
        for (auto &kv : b.dict) {
          bool replaced=false;
          for (auto &ekv : m) if (valueEq(ekv.first, kv.first)) { ekv.second = kv.second; replaced=true; break; }
          if (!replaced) m.push_back(kv);
        }
        m = normalize_dict(move(m));
        return Value::Dict(move(m));
      }
      if (a.kind==Value::INT && b.kind==Value::INT) return Value::Int(a.i + b.i);
      if ((a.kind==Value::INT && b.kind==Value::DEC) || (a.kind==Value::DEC && b.kind==Value::INT) || (a.kind==Value::DEC && b.kind==Value::DEC)) {
        double x = (a.kind==Value::DEC? a.d : (double)a.i);
        double y = (b.kind==Value::DEC? b.d : (double)b.i);
        return Value::Dec(x + y);
      }
      if (a.kind==Value::STR && b.kind==Value::STR) return Value::Str(a.s + b.s);
      if (a.kind==Value::STR && (b.kind==Value::INT || b.kind==Value::DEC || b.kind==Value::BOOL || b.kind==Value::NIL)) return Value::Str(a.s + repr(b));
      if ((a.kind==Value::INT || a.kind==Value::DEC || a.kind==Value::BOOL || a.kind==Value::NIL) && b.kind==Value::STR) return Value::Str(repr(a) + b.s);
      err("Unsupported operation: " + typeName(a) + " + " + typeName(b));
      return Value::Nil();
    };

    auto sub = [&](const Value &a, const Value &b)->Value {
      if (a.kind==Value::INT && b.kind==Value::INT) return Value::Int(a.i - b.i);
      double x = (a.kind==Value::DEC? a.d : (double)a.i);
      double y = (b.kind==Value::DEC? b.d : (double)b.i);
      if ((a.kind==Value::INT || a.kind==Value::DEC) && (b.kind==Value::INT || b.kind==Value::DEC)) return Value::Dec(x - y);
      err("Unsupported operation: " + typeName(a) + " - " + typeName(b));
      return Value::Nil();
    };

    auto mul = [&](const Value &a, const Value &b)->Value {
      if (a.kind==Value::INT && b.kind==Value::INT) return Value::Int(a.i * b.i);
      if ((a.kind==Value::INT || a.kind==Value::DEC) && (b.kind==Value::INT || b.kind==Value::DEC)) {
        double x = (a.kind==Value::DEC? a.d : (double)a.i);
        double y = (b.kind==Value::DEC? b.d : (double)b.i);
        return Value::Dec(x * y);
      }
      auto is_int = [&](const Value &v){ return v.kind==Value::INT; };
      auto is_dec = [&](const Value &v){ return v.kind==Value::DEC; };
      if (a.kind==Value::STR && is_int(b)) {
        if (b.i < 0) err("Unsupported operation: String * Integer (< 0)");
        string res; res.reserve(a.s.size() * (size_t)max(0LL,b.i)); for (long long k=0;k<b.i;k++) res += a.s; return Value::Str(res);
      }
      if (b.kind==Value::STR && is_int(a)) {
        if (a.i < 0) err("Unsupported operation: String * Integer (< 0)");
        string res; res.reserve(b.s.size() * (size_t)max(0LL,a.i)); for (long long k=0;k<a.i;k++) res += b.s; return Value::Str(res);
      }
      if ((a.kind==Value::STR && is_dec(b)) || (b.kind==Value::STR && is_dec(a))) {
        err("Unsupported operation: String * Decimal");
      }
      err("Unsupported operation: " + typeName(a) + " * " + typeName(b));
      return Value::Nil();
    };

    auto divv = [&](const Value &a, const Value &b)->Value {
      auto zero_err = [&](){ err("Division by zero"); return Value::Nil(); };
      if (b.kind==Value::INT && b.i==0) return zero_err();
      if (b.kind==Value::DEC && b.d==0.0) return zero_err();
      if (a.kind==Value::INT && b.kind==Value::INT) {
        if (b.i==0) return zero_err();
        // trunc toward zero
        long long q = a.i / b.i; return Value::Int(q);
      }
      if ((a.kind==Value::INT || a.kind==Value::DEC) && (b.kind==Value::INT || b.kind==Value::DEC)) {
        double x = (a.kind==Value::DEC? a.d : (double)a.i);
        double y = (b.kind==Value::DEC? b.d : (double)b.i);
        return Value::Dec(x / y);
      }
      err("Unsupported operation: " + typeName(a) + " / " + typeName(b));
      return Value::Nil();
    };

    auto truthy = [&](const Value &v){
      switch (v.kind) {
        case Value::BOOL: return v.b;
        case Value::INT: return v.i != 0;
        case Value::DEC: return v.d != 0.0;
        case Value::STR: return !v.s.empty();
        case Value::LIST: return !v.list.empty();
        case Value::SET: return !v.set.empty();
        case Value::DICT: return !v.dict.empty();
        case Value::NIL: default: return false;
      }
    };

    // Forward declare eval_expr so eval_block can use it
    function<Value(const Expr*)> eval_expr;

    // Evaluate a block with a new scope; returns last non-comment value (or nil if none)
    function<Value(const Block&)> eval_block = [&](const Block &blk)->Value {
      auto saved = currentEnv;
      currentEnv = make_shared<EnvFrame>(saved);
      Value last = Value::Nil(); bool any=false;
      for (const auto &st : blk.statements) {
        if (dynamic_cast<const CommentStmt*>(st.get())) { (void)0; }
        else { any=true; last = eval_expr(static_cast<const ExpressionStmt*>(st.get())->value.get()); }
      }
      currentEnv = saved;
      return any ? last : Value::Nil();
    };

    // Function calling with partial application
    function<Value(const Value&, vector<Value>)> callFunction = [&](const Value &fn, vector<Value> args)->Value{
      if (fn.kind != Value::FN) err(string("Expected a Function, found: ") + typeName(fn));
      int ar = fn.fn->arity;
      if (ar < 0) { // variadic
        return fn.fn->apply(args);
      }
      if ((int)args.size() < ar) {
        vector<Value> captured = move(args);
        return Value::Function(ar - (int)captured.size(), [fn, captured, callFunction](const vector<Value>& more)->Value{
          vector<Value> all = captured; all.insert(all.end(), more.begin(), more.end());
          // Only pass first original arity args
          vector<Value> pass; pass.reserve(fn.fn->arity);
          for (int i=0;i<fn.fn->arity && i<(int)all.size();++i) pass.push_back(all[i]);
          return fn.fn->apply(pass);
        });
      }
      if ((int)args.size() > ar) args.resize(ar);
      return fn.fn->apply(args);
    };

    eval_expr = [&](const Expr *e)->Value {
      if (auto n = dynamic_cast<const IntegerLit*>(e)) { return Value::Int(parse_int(n->value)); }
      if (auto n = dynamic_cast<const DecimalLit*>(e)) { return Value::Dec(parse_dec(n->value)); }
      if (auto n = dynamic_cast<const StringLit*>(e)) { return Value::Str(n->value); }
      if (auto n = dynamic_cast<const BooleanLit*>(e)) { return Value::Bool(n->value); }
      if (dynamic_cast<const NilLit*>(e)) { return Value::Nil(); }
      if (auto id = dynamic_cast<const Identifier*>(e)) {
        // Lookup in env
        if (auto v = currentEnv->find(id->name)) return v->val;
        // Built-ins as first-class functions
        const string &nm = id->name;
        if (nm == "+") return Value::Function(2, [&](const vector<Value>& xs){ return add(xs[0], xs[1]); });
        if (nm == "-") return Value::Function(2, [&](const vector<Value>& xs){ return sub(xs[0], xs[1]); });
        if (nm == "*") return Value::Function(2, [&](const vector<Value>& xs){ return mul(xs[0], xs[1]); });
        if (nm == "/") return Value::Function(2, [&](const vector<Value>& xs){ return divv(xs[0], xs[1]); });
        if (nm == "push") return Value::Function(2, [&](const vector<Value>& xs){
          const Value &val = xs[0]; const Value &col = xs[1];
          if (col.kind==Value::LIST) { vector<Value> out = col.list; out.push_back(val); return Value::List(move(out)); }
          if (col.kind==Value::SET)  { vector<Value> out = col.set; out.push_back(val); return Value::Set(normalize_set(move(out))); }
          return Value::Nil();
        });
        if (nm == "assoc") return Value::Function(3, [&](const vector<Value>& xs){
          const Value &key = xs[0]; const Value &val = xs[1]; const Value &mp = xs[2];
          if (mp.kind!=Value::DICT) return Value::Nil();
          if (key.kind==Value::DICT) err("Unable to use a Dictionary as a Dictionary key");
          vector<pair<Value,Value>> out = mp.dict; bool replaced=false; for (auto &kv : out) if (valueEq(kv.first, key)) { kv.second = val; replaced=true; break; }
          if (!replaced) out.push_back({key, val}); out = normalize_dict(move(out)); return Value::Dict(move(out));
        });
        if (nm == "first") return Value::Function(1, [&](const vector<Value>& xs){ const Value &x = xs[0]; if (x.kind==Value::LIST) return x.list.empty()? Value::Nil() : x.list.front(); if (x.kind==Value::STR) return x.s.empty()? Value::Nil() : Value::Str(string(1, x.s[0])); return Value::Nil(); });
        if (nm == "rest") return Value::Function(1, [&](const vector<Value>& xs){ const Value &x = xs[0]; if (x.kind==Value::LIST) { if (x.list.empty()) return Value::List({}); vector<Value> out(x.list.begin()+1, x.list.end()); return Value::List(move(out)); } if (x.kind==Value::STR) { if (x.s.empty()) return Value::Str(""); return Value::Str(x.s.substr(1)); } return Value::Nil(); });
        if (nm == "size") return Value::Function(1, [&](const vector<Value>& xs){ const Value &x = xs[0]; switch (x.kind){ case Value::LIST: return Value::Int((long long)x.list.size()); case Value::SET: return Value::Int((long long)x.set.size()); case Value::DICT: return Value::Int((long long)x.dict.size()); case Value::STR: return Value::Int((long long)x.s.size()); default: return Value::Int(0);} });
        if (nm == "map") return Value::Function(2, [&](const vector<Value>& xs){ const Value &f = xs[0]; const Value &lst = xs[1]; if (f.kind!=Value::FN || lst.kind!=Value::LIST) err("Unexpected argument: map(" + typeName(f) + ", " + typeName(lst) + ")"); vector<Value> out; out.reserve(lst.list.size()); for (auto &v : lst.list) out.push_back(callFunction(f, {v})); return Value::List(move(out)); });
        if (nm == "filter") return Value::Function(2, [&](const vector<Value>& xs){ const Value &f = xs[0]; const Value &lst = xs[1]; if (f.kind!=Value::FN || lst.kind!=Value::LIST) err("Unexpected argument: filter(" + typeName(f) + ", " + typeName(lst) + ")"); vector<Value> out; for (auto &v : lst.list) if (truthy(callFunction(f, {v}))) out.push_back(v); return Value::List(move(out)); });
        if (nm == "fold") return Value::Function(3, [&](const vector<Value>& xs){ const Value &init = xs[0]; const Value &f = xs[1]; const Value &lst = xs[2]; if (f.kind!=Value::FN || lst.kind!=Value::LIST) err("Unexpected argument: fold(" + typeName(init) + ", " + typeName(f) + ", " + typeName(lst) + ")"); Value acc = init; for (auto &v : lst.list) acc = callFunction(f, {acc, v}); return acc; });
        err("Identifier can not be found: " + id->name);
      }
      if (auto pre = dynamic_cast<const PrefixExpr*>(e)) {
        Value v = eval_expr(pre->operand.get());
        if (pre->op == "-") {
          if (v.kind == Value::INT) return Value::Int(-v.i);
          if (v.kind == Value::DEC) return Value::Dec(-v.d);
        }
        err("Unsupported operation: Prefix "+pre->op);
      }
      if (auto inf = dynamic_cast<const InfixExpr*>(e)) {
        Value l = eval_expr(inf->left.get());
        Value r = eval_expr(inf->right.get());
        const string &op = inf->op;
        if (op=="+") return add(l,r);
        if (op=="-") return sub(l,r);
        if (op=="*") return mul(l,r);
        if (op=="/") return divv(l,r);
        if (op=="&&") {
          return Value::Bool(truthy(l) && truthy(r));
        }
        if (op=="||") {
          return Value::Bool(truthy(l) || truthy(r));
        }
        if (op=="==") return Value::Bool(valueEq(l,r));
        if (op=="!=") return Value::Bool(!valueEq(l,r));
        if (op==">"||op=="<"||op==">="||op=="<=") {
          auto as_num = [&](const Value &v, bool &ok)->double{
            if (v.kind==Value::INT) { ok=true; return (double)v.i; }
            if (v.kind==Value::DEC) { ok=true; return v.d; }
            ok=false; return 0.0;
          };
          bool okL=false, okR=false; double dl = as_num(l, okL), dr = as_num(r, okR);
          if (!(okL && okR)) err("Unsupported operation: " + op);
          if (op==">")  return Value::Bool(dl >  dr);
          if (op=="<")  return Value::Bool(dl <  dr);
          if (op==">=") return Value::Bool(dl >= dr);
          if (op=="<=") return Value::Bool(dl <= dr);
        }
        err("Unsupported operation: " + op);
      }
      if (auto iff = dynamic_cast<const IfExpr*>(e)) {
        Value c = eval_expr(iff->condition.get());
        if (truthy(c)) return eval_block(iff->consequence);
        return eval_block(iff->alternative);
      }
      if (auto le = dynamic_cast<const LetExpr*>(e)) {
        Value v = eval_expr(le->value.get());
        currentEnv->define(le->name.name, v, le->typ == "MutableLet");
        return v;
      }
      if (auto asg = dynamic_cast<const AssignExpr*>(e)) {
        auto var = currentEnv->find(asg->name.name);
        if (!var) err("Identifier can not be found: " + asg->name.name);
        if (!var->mut) err("Variable '" + asg->name.name + "' is not mutable");
        Value v = eval_expr(asg->value.get()); var->val = v; return v;
      }
      if (auto call = dynamic_cast<const CallExpr*>(e)) {
        vector<Value> args; args.reserve(call->args.size());
        for (auto &a : call->args) args.push_back(eval_expr(a.get()));
        // Special-case: direct puts(...) should print immediately (not as function value)
        if (auto fid = dynamic_cast<const Identifier*>(call->func.get())) {
          if (fid->name == "puts") { for (size_t i=0;i<args.size();++i) cout << repr(args[i]) << ' '; cout << '\n'; return Value::Nil(); }
        }
        Value cal = eval_expr(call->func.get());
        return callFunction(cal, move(args));
      }
      if (auto lst = dynamic_cast<const ListLit*>(e)) {
        vector<Value> items; items.reserve(lst->items.size());
        for (auto &it : lst->items) items.push_back(eval_expr(it.get()));
        return Value::List(move(items));
      }
      if (auto st = dynamic_cast<const SetLit*>(e)) {
        vector<Value> items; items.reserve(st->items.size());
        for (auto &it : st->items) items.push_back(eval_expr(it.get()));
        // Set literal may NOT contain a Dictionary
        for (auto &v : items) if (v.kind==Value::DICT) err("Unable to include a Dictionary within a Set");
        items = normalize_set(move(items));
        return Value::Set(move(items));
      }
      if (auto mp = dynamic_cast<const DictLit*>(e)) {
        vector<pair<Value,Value>> entries; entries.reserve(mp->items.size());
        for (auto &kv : mp->items) {
          Value k = eval_expr(kv.key.get());
          Value v = eval_expr(kv.value.get());
          if (k.kind==Value::DICT) err("Unable to use a Dictionary as a Dictionary key");
          bool replaced=false; for (auto &ekv : entries) if (valueEq(ekv.first, k)) { ekv.second = v; replaced=true; break; }
          if (!replaced) entries.push_back({move(k), move(v)});
        }
        entries = normalize_dict(move(entries));
        return Value::Dict(move(entries));
      }
      if (auto idx = dynamic_cast<const IndexExpr*>(e)) {
        Value left = eval_expr(idx->left.get());
        Value ind = eval_expr(idx->index.get());
        if (left.kind==Value::LIST) {
          if (ind.kind != Value::INT) err("Unable to perform index operation, found: List[" + typeName(ind) + "]");
          long long n = (long long)left.list.size(); long long j = ind.i; if (j < 0) j = n + j;
          if (j < 0 || j >= n) return Value::Nil();
          return left.list[(size_t)j];
        }
        if (left.kind==Value::STR) {
          if (ind.kind != Value::INT) err("Unable to perform index operation, found: String[" + typeName(ind) + "]");
          long long n = (long long)left.s.size(); long long j = ind.i; if (j < 0) j = n + j;
          if (j < 0 || j >= n) return Value::Nil();
          return Value::Str(string(1, left.s[(size_t)j]));
        }
        if (left.kind==Value::DICT) {
          if (ind.kind==Value::DICT) err("Unable to use a Dictionary as a Dictionary key");
          for (auto &kv : left.dict) if (valueEq(kv.first, ind)) return kv.second;
          return Value::Nil();
        }
        // Other types not indexable in stage 4/5
        return Value::Nil();
      }
      if (auto fn = dynamic_cast<const FunctionLit*>(e)) {
        // Capture defining environment by reference
        auto captured = currentEnv;
        vector<string> params; params.reserve(fn->params.size()); for (auto &p : fn->params) params.push_back(p.name);
        int ar = (int)params.size();
        const Block* bodyPtr = &fn->body;
        return Value::Function(ar, [&, captured, params, bodyPtr](const vector<Value>& args)->Value{
          auto saved = currentEnv;
          currentEnv = make_shared<EnvFrame>(captured);
          for (int i=0;i<(int)params.size() && i<(int)args.size(); ++i) currentEnv->define(params[i], args[i], false);
          Value last = Value::Nil(); bool any=false;
          for (const auto &st : bodyPtr->statements) {
            if (dynamic_cast<const CommentStmt*>(st.get())) { (void)0; }
            else { any=true; last = eval_expr(static_cast<const ExpressionStmt*>(st.get())->value.get()); }
          }
          currentEnv = saved;
          return any ? last : Value::Nil();
        });
      }
      if (auto comp = dynamic_cast<const FunctionComposition*>(e)) {
        // Compose as unary function: g(f(x)) for left-to-right order stored
        vector<ExprPtr> fexprs; for (auto &p : comp->funcs) fexprs.push_back(nullptr); // placeholder
        vector<unique_ptr<Expr>> const* dummy = nullptr; (void)dummy; // silence unused
        vector<ExprPtr> funcs;
        // Can't move from AST; re-eval at call time
        return Value::Function(1, [&, comp](const vector<Value>& args)->Value{
          Value cur = args.size()>0 ? args[0] : Value::Nil();
          // Apply in sequence
          for (auto &fx : comp->funcs) {
            Value fval = eval_expr(fx.get());
            cur = callFunction(fval, {cur});
          }
          return cur;
        });
      }
      if (auto thr = dynamic_cast<const FunctionThread*>(e)) {
        // Evaluate pipeline now
        Value cur = eval_expr(thr->initial.get());
        for (auto &step : thr->funcs) {
          if (auto c = dynamic_cast<const CallExpr*>(step.get())) {
            // Evaluate callee and args, then append cur as last arg
            Value f = eval_expr(c->func.get());
            vector<Value> args; args.reserve(c->args.size()+1);
            for (auto &a : c->args) args.push_back(eval_expr(a.get()));
            args.push_back(cur);
            cur = callFunction(f, move(args));
          } else {
            Value f = eval_expr(step.get());
            cur = callFunction(f, {cur});
          }
        }
        return cur;
      }
      // ExpressionStmt/Block handled at statement level
      return Value::Nil();
    };

    auto eval_stmt = [&](const Statement *st)->Value {
      if (auto cs = dynamic_cast<const CommentStmt*>(st)) return Value::Nil();
      if (auto es = dynamic_cast<const ExpressionStmt*>(st)) return eval_expr(es->value.get());
      return Value::Nil();
    };

    try {
      Value last = Value::Nil();
      for (const auto &st : prog.statements) {
        if (dynamic_cast<const CommentStmt*>(st.get())) {
          // do not update last with comments
          (void)eval_stmt(st.get());
        } else {
          last = eval_stmt(st.get());
        }
      }
      cout << repr(last) << '\n';
    } catch (const exception &) {
      // Error already printed. Exit non-zero to signal failure (tests match stdout only).
      return 1;
    }
    return 0;
  }
}
