#pragma once
#include "lexer.hpp"
#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <stdexcept>

// ─── AST Node Types ──────────────────────────────────────────────────────────

struct ASTNode;
using NodePtr = std::shared_ptr<ASTNode>;

struct ASTNode {
    enum class Kind {
        // Literals
        NumberLit, StringLit, BoolLit, NilLit, ListLit,
        // Expressions
        Ident, BinOp, UnaryOp, Call, Index, FieldAccess,
        // Statements
        Block, LetDecl, Assign, If, For, While, Return, Break, Continue,
        FuncDecl, Import,
        // String concat
        Concat
    };

    Kind kind;
    int line = 0;

    // Literal values
    double num_val = 0;
    std::string str_val;
    bool bool_val = false;

    // Operator
    std::string op;

    // Children
    std::vector<NodePtr> children;   // general children
    std::vector<std::string> params; // func params / let name

    // For 'for' loop: var name + optional step
    std::string loop_var;
    bool has_step = false;
};

inline NodePtr make_node(ASTNode::Kind k, int line = 0) {
    auto n = std::make_shared<ASTNode>();
    n->kind = k; n->line = line;
    return n;
}

// ─── Parser ──────────────────────────────────────────────────────────────────

class Parser {
public:
    std::vector<Token> tokens;
    size_t pos = 0;

    Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

    NodePtr parse() {
        skip_newlines();
        auto block = parse_block();
        return block;
    }

private:
    Token& cur() { return tokens[pos]; }
    Token& peek(int offset = 1) {
        size_t p = pos + offset;
        if (p >= tokens.size()) return tokens.back();
        return tokens[p];
    }

    bool at_end() { return cur().type == TokenType::END_OF_FILE; }

    Token consume() { return tokens[pos++]; }

    Token expect(TokenType t, const std::string& msg = "") {
        if (cur().type != t) {
            throw std::runtime_error("Line " + std::to_string(cur().line) +
                ": Expected " + msg + ", got '" + cur().value + "'");
        }
        return consume();
    }

    bool match(TokenType t) {
        if (cur().type == t) { consume(); return true; }
        return false;
    }

    void skip_newlines() {
        while (cur().type == TokenType::NEWLINE || cur().type == TokenType::SEMICOLON)
            consume();
    }

    // ── Block ──
    NodePtr parse_block() {
        auto block = make_node(ASTNode::Kind::Block, cur().line);
        skip_newlines();
        while (!at_end()) {
            auto tt = cur().type;
            if (tt == TokenType::END || tt == TokenType::ELSE || tt == TokenType::ELIF)
                break;
            auto stmt = parse_statement();
            if (stmt) block->children.push_back(stmt);
            skip_newlines();
        }
        return block;
    }

    // ── Statement ──
    NodePtr parse_statement() {
        skip_newlines();
        int ln = cur().line;
        auto tt = cur().type;

        if (tt == TokenType::LET)      return parse_let();
        if (tt == TokenType::FUNC)     return parse_func();
        if (tt == TokenType::RETURN)   return parse_return();
        if (tt == TokenType::IMPORT)   return parse_import();
        if (tt == TokenType::BREAK)    { consume(); return make_node(ASTNode::Kind::Break, ln); }
        if (tt == TokenType::CONTINUE) { consume(); return make_node(ASTNode::Kind::Continue, ln); }
        if (tt == TokenType::IF)       return parse_if();
        if (tt == TokenType::FOR)      return parse_for();
        if (tt == TokenType::WHILE)    return parse_while();

        // Assignment or expression
        return parse_assign_or_expr();
    }

    NodePtr parse_import() {
        int ln = cur().line;
        consume(); // import
        // import "extname"  or  import "extname" as alias
        std::string name = expect(TokenType::STRING, "extension name").value;
        auto node = make_node(ASTNode::Kind::Import, ln);
        node->str_val = name; // extension name / path stem
        return node;
    }

    NodePtr parse_let() {
        int ln = cur().line;
        consume(); // let
        std::string name = expect(TokenType::IDENT, "variable name").value;
        expect(TokenType::ASSIGN, "'='");
        auto val = parse_expr();
        auto node = make_node(ASTNode::Kind::LetDecl, ln);
        node->str_val = name;
        node->children.push_back(val);
        return node;
    }

    NodePtr parse_func() {
        int ln = cur().line;
        consume(); // func
        std::string name = expect(TokenType::IDENT, "function name").value;
        expect(TokenType::LPAREN, "'('");
        std::vector<std::string> params;
        while (cur().type != TokenType::RPAREN && !at_end()) {
            params.push_back(expect(TokenType::IDENT, "parameter name").value);
            if (cur().type == TokenType::COMMA) consume();
        }
        expect(TokenType::RPAREN, "')'");
        skip_newlines();
        auto body = parse_block();
        expect(TokenType::END, "'end'");

        auto node = make_node(ASTNode::Kind::FuncDecl, ln);
        node->str_val = name;
        node->params = params;
        node->children.push_back(body);
        return node;
    }

    NodePtr parse_return() {
        int ln = cur().line;
        consume();
        auto node = make_node(ASTNode::Kind::Return, ln);
        if (!at_end() && cur().type != TokenType::NEWLINE &&
            cur().type != TokenType::SEMICOLON && cur().type != TokenType::END)
            node->children.push_back(parse_expr());
        return node;
    }

    NodePtr parse_if() {
        int ln = cur().line;
        consume(); // if
        auto node = make_node(ASTNode::Kind::If, ln);
        node->children.push_back(parse_expr());  // condition
        expect(TokenType::THEN, "'then'");
        skip_newlines();
        node->children.push_back(parse_block()); // then body

        while (cur().type == TokenType::ELIF) {
            consume();
            node->children.push_back(parse_expr());
            expect(TokenType::THEN, "'then'");
            skip_newlines();
            node->children.push_back(parse_block());
        }

        if (cur().type == TokenType::ELSE) {
            consume();
            skip_newlines();
            node->children.push_back(parse_block());
        }
        expect(TokenType::END, "'end'");
        return node;
    }

    NodePtr parse_for() {
        int ln = cur().line;
        consume(); // for
        std::string var = expect(TokenType::IDENT, "loop variable").value;
        expect(TokenType::ASSIGN, "'='");
        auto node = make_node(ASTNode::Kind::For, ln);
        node->loop_var = var;
        node->children.push_back(parse_expr()); // start
        expect(TokenType::TO, "'to'");
        node->children.push_back(parse_expr()); // end
        if (cur().type == TokenType::STEP) {
            consume();
            node->children.push_back(parse_expr()); // step
            node->has_step = true;
        }
        expect(TokenType::DO, "'do'");
        skip_newlines();
        node->children.push_back(parse_block());
        expect(TokenType::END, "'end'");
        return node;
    }

    NodePtr parse_while() {
        int ln = cur().line;
        consume(); // while
        auto node = make_node(ASTNode::Kind::While, ln);
        node->children.push_back(parse_expr());
        expect(TokenType::DO, "'do'");
        skip_newlines();
        node->children.push_back(parse_block());
        expect(TokenType::END, "'end'");
        return node;
    }

    NodePtr parse_assign_or_expr() {
        int ln = cur().line;
        auto expr = parse_expr();
        if (cur().type == TokenType::ASSIGN) {
            consume();
            auto val = parse_expr();
            auto node = make_node(ASTNode::Kind::Assign, ln);
            node->children.push_back(expr);
            node->children.push_back(val);
            return node;
        }
        return expr;
    }

    // ── Expressions (Pratt-style) ──
    NodePtr parse_expr() { return parse_or(); }

    NodePtr parse_or() {
        auto left = parse_and();
        while (cur().type == TokenType::OR) {
            consume();
            auto node = make_node(ASTNode::Kind::BinOp, left->line);
            node->op = "or"; node->children = {left, parse_and()};
            left = node;
        }
        return left;
    }

    NodePtr parse_and() {
        auto left = parse_not();
        while (cur().type == TokenType::AND) {
            consume();
            auto node = make_node(ASTNode::Kind::BinOp, left->line);
            node->op = "and"; node->children = {left, parse_not()};
            left = node;
        }
        return left;
    }

    NodePtr parse_not() {
        if (cur().type == TokenType::NOT) {
            int ln = cur().line; consume();
            auto node = make_node(ASTNode::Kind::UnaryOp, ln);
            node->op = "not"; node->children = {parse_not()};
            return node;
        }
        return parse_comparison();
    }

    NodePtr parse_comparison() {
        auto left = parse_concat();
        auto tt = cur().type;
        if (tt == TokenType::EQ || tt == TokenType::NEQ ||
            tt == TokenType::LT || tt == TokenType::LE ||
            tt == TokenType::GT || tt == TokenType::GE) {
            std::string op = consume().value;
            auto node = make_node(ASTNode::Kind::BinOp, left->line);
            node->op = op; node->children = {left, parse_concat()};
            return node;
        }
        return left;
    }

    NodePtr parse_concat() {
        auto left = parse_add();
        while (cur().type == TokenType::DOTDOT) {
            int ln = cur().line; consume();
            auto node = make_node(ASTNode::Kind::Concat, ln);
            node->children = {left, parse_add()};
            left = node;
        }
        return left;
    }

    NodePtr parse_add() {
        auto left = parse_mul();
        while (cur().type == TokenType::PLUS || cur().type == TokenType::MINUS) {
            std::string op = consume().value;
            auto node = make_node(ASTNode::Kind::BinOp, left->line);
            node->op = op; node->children = {left, parse_mul()};
            left = node;
        }
        return left;
    }

    NodePtr parse_mul() {
        auto left = parse_unary();
        while (cur().type == TokenType::STAR || cur().type == TokenType::SLASH ||
               cur().type == TokenType::PERCENT) {
            std::string op = consume().value;
            auto node = make_node(ASTNode::Kind::BinOp, left->line);
            node->op = op; node->children = {left, parse_unary()};
            left = node;
        }
        return left;
    }

    NodePtr parse_unary() {
        if (cur().type == TokenType::MINUS) {
            int ln = cur().line; consume();
            auto node = make_node(ASTNode::Kind::UnaryOp, ln);
            node->op = "-"; node->children = {parse_power()};
            return node;
        }
        return parse_power();
    }

    NodePtr parse_power() {
        auto base = parse_postfix();
        if (cur().type == TokenType::CARET) {
            consume();
            auto node = make_node(ASTNode::Kind::BinOp, base->line);
            node->op = "^"; node->children = {base, parse_unary()};
            return node;
        }
        return base;
    }

    NodePtr parse_postfix() {
        auto node = parse_primary();
        while (true) {
            if (cur().type == TokenType::LPAREN) {
                // function call
                int ln = cur().line; consume();
                auto call = make_node(ASTNode::Kind::Call, ln);
                call->children.push_back(node);
                while (cur().type != TokenType::RPAREN && !at_end()) {
                    call->children.push_back(parse_expr());
                    if (cur().type == TokenType::COMMA) consume();
                }
                expect(TokenType::RPAREN, "')'");
                node = call;
            } else if (cur().type == TokenType::LBRACKET) {
                int ln = cur().line; consume();
                auto idx = make_node(ASTNode::Kind::Index, ln);
                idx->children.push_back(node);
                idx->children.push_back(parse_expr());
                expect(TokenType::RBRACKET, "']'");
                node = idx;
            } else if (cur().type == TokenType::DOT) {
                int ln = cur().line; consume();
                std::string field = expect(TokenType::IDENT, "field name").value;
                auto fa = make_node(ASTNode::Kind::FieldAccess, ln);
                fa->str_val = field;
                fa->children.push_back(node);
                node = fa;
            } else {
                break;
            }
        }
        return node;
    }

    NodePtr parse_primary() {
        int ln = cur().line;
        auto tt = cur().type;

        if (tt == TokenType::NUMBER) {
            auto n = make_node(ASTNode::Kind::NumberLit, ln);
            n->num_val = std::stod(consume().value);
            return n;
        }
        if (tt == TokenType::STRING) {
            auto n = make_node(ASTNode::Kind::StringLit, ln);
            n->str_val = consume().value;
            return n;
        }
        if (tt == TokenType::TRUE) {
            consume();
            auto n = make_node(ASTNode::Kind::BoolLit, ln);
            n->bool_val = true; return n;
        }
        if (tt == TokenType::FALSE) {
            consume();
            auto n = make_node(ASTNode::Kind::BoolLit, ln);
            n->bool_val = false; return n;
        }
        if (tt == TokenType::NIL) {
            consume(); return make_node(ASTNode::Kind::NilLit, ln);
        }
        if (tt == TokenType::IDENT) {
            auto n = make_node(ASTNode::Kind::Ident, ln);
            n->str_val = consume().value;
            return n;
        }
        if (tt == TokenType::LPAREN) {
            consume();
            auto e = parse_expr();
            expect(TokenType::RPAREN, "')'");
            return e;
        }
        if (tt == TokenType::LBRACKET) {
            consume();
            auto list = make_node(ASTNode::Kind::ListLit, ln);
            while (cur().type != TokenType::RBRACKET && !at_end()) {
                list->children.push_back(parse_expr());
                if (cur().type == TokenType::COMMA) consume();
            }
            expect(TokenType::RBRACKET, "']'");
            return list;
        }

        throw std::runtime_error("Line " + std::to_string(ln) +
            ": Unexpected token '" + cur().value + "'");
    }
};
