#pragma once
#include <string>
#include <vector>
#include <stdexcept>

enum class TokenType {
    // Literals
    NUMBER, STRING, IDENT, TRUE, FALSE, NIL,
    // Keywords
    LET, FUNC, RETURN, IF, THEN, ELIF, ELSE, END,
    FOR, TO, STEP, DO, WHILE, BREAK, CONTINUE,
    AND, OR, NOT, IN, IMPORT,
    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT, CARET,
    EQ, NEQ, LT, LE, GT, GE,
    ASSIGN,
    // Punctuation
    LPAREN, RPAREN, LBRACKET, RBRACKET, COMMA, DOT, DOTDOT,
    NEWLINE, SEMICOLON,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

class Lexer {
public:
    std::string source;
    size_t pos = 0;
    int line = 1;

    Lexer(const std::string& src) : source(src) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos < source.size()) {
            skip_whitespace_and_comments();
            if (pos >= source.size()) break;

            char c = source[pos];

            // Newlines
            if (c == '\n') {
                tokens.push_back({TokenType::NEWLINE, "\\n", line});
                line++; pos++;
                continue;
            }
            if (c == '\r') { pos++; continue; }
            if (c == ';') { tokens.push_back({TokenType::SEMICOLON, ";", line}); pos++; continue; }

            // Numbers
            if (isdigit(c) || (c == '.' && pos+1 < source.size() && isdigit(source[pos+1]))) {
                tokens.push_back(read_number());
                continue;
            }

            // Strings
            if (c == '"' || c == '\'') {
                tokens.push_back(read_string(c));
                continue;
            }

            // Identifiers/keywords
            if (isalpha(c) || c == '_') {
                tokens.push_back(read_ident());
                continue;
            }

            // Operators
            Token tok = read_operator();
            if (tok.type != TokenType::END_OF_FILE) {
                tokens.push_back(tok);
            }
        }
        tokens.push_back({TokenType::END_OF_FILE, "", line});
        return tokens;
    }

private:
    void skip_whitespace_and_comments() {
        while (pos < source.size()) {
            char c = source[pos];
            if (c == ' ' || c == '\t') { pos++; continue; }
            // Single line comment: --
            if (c == '-' && pos+1 < source.size() && source[pos+1] == '-') {
                while (pos < source.size() && source[pos] != '\n') pos++;
                continue;
            }
            break;
        }
    }

    Token read_number() {
        size_t start = pos;
        while (pos < source.size() && (isdigit(source[pos]) || source[pos] == '.')) pos++;
        return {TokenType::NUMBER, source.substr(start, pos-start), line};
    }

    Token read_string(char delim) {
        pos++; // skip opening quote
        std::string s;
        while (pos < source.size() && source[pos] != delim) {
            if (source[pos] == '\\' && pos+1 < source.size()) {
                pos++;
                switch (source[pos]) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case '\\': s += '\\'; break;
                    default: s += source[pos]; break;
                }
            } else {
                s += source[pos];
            }
            pos++;
        }
        pos++; // skip closing quote
        return {TokenType::STRING, s, line};
    }

    Token read_ident() {
        size_t start = pos;
        while (pos < source.size() && (isalnum(source[pos]) || source[pos] == '_')) pos++;
        std::string word = source.substr(start, pos-start);
        
        // Keywords
        if (word == "let")      return {TokenType::LET,      word, line};
        if (word == "func")     return {TokenType::FUNC,     word, line};
        if (word == "return")   return {TokenType::RETURN,   word, line};
        if (word == "if")       return {TokenType::IF,       word, line};
        if (word == "then")     return {TokenType::THEN,     word, line};
        if (word == "elif")     return {TokenType::ELIF,     word, line};
        if (word == "else")     return {TokenType::ELSE,     word, line};
        if (word == "end")      return {TokenType::END,      word, line};
        if (word == "for")      return {TokenType::FOR,      word, line};
        if (word == "to")       return {TokenType::TO,       word, line};
        if (word == "step")     return {TokenType::STEP,     word, line};
        if (word == "do")       return {TokenType::DO,       word, line};
        if (word == "while")    return {TokenType::WHILE,    word, line};
        if (word == "break")    return {TokenType::BREAK,    word, line};
        if (word == "continue") return {TokenType::CONTINUE, word, line};
        if (word == "and")      return {TokenType::AND,      word, line};
        if (word == "or")       return {TokenType::OR,       word, line};
        if (word == "not")      return {TokenType::NOT,      word, line};
        if (word == "in")       return {TokenType::IN,       word, line};
        if (word == "import")   return {TokenType::IMPORT,   word, line};
        if (word == "true")     return {TokenType::TRUE,     word, line};
        if (word == "false")    return {TokenType::FALSE,    word, line};
        if (word == "nil")      return {TokenType::NIL,      word, line};

        return {TokenType::IDENT, word, line};
    }

    Token read_operator() {
        char c = source[pos];
        char n = (pos+1 < source.size()) ? source[pos+1] : 0;

        auto tok = [&](TokenType t, int len) -> Token {
            std::string v = source.substr(pos, len);
            pos += len;
            return {t, v, line};
        };

        if (c == '+') return tok(TokenType::PLUS, 1);
        if (c == '-') return tok(TokenType::MINUS, 1);
        if (c == '*') return tok(TokenType::STAR, 1);
        if (c == '/') return tok(TokenType::SLASH, 1);
        if (c == '%') return tok(TokenType::PERCENT, 1);
        if (c == '^') return tok(TokenType::CARET, 1);
        if (c == '(' ) return tok(TokenType::LPAREN, 1);
        if (c == ')' ) return tok(TokenType::RPAREN, 1);
        if (c == '[' ) return tok(TokenType::LBRACKET, 1);
        if (c == ']' ) return tok(TokenType::RBRACKET, 1);
        if (c == ',' ) return tok(TokenType::COMMA, 1);
        if (c == '.' && n == '.') return tok(TokenType::DOTDOT, 2);
        if (c == '.' ) return tok(TokenType::DOT, 1);
        if (c == '=' && n == '=') return tok(TokenType::EQ, 2);
        if (c == '=' ) return tok(TokenType::ASSIGN, 1);
        if (c == '!' && n == '=') return tok(TokenType::NEQ, 2);
        if (c == '<' && n == '=') return tok(TokenType::LE, 2);
        if (c == '<' ) return tok(TokenType::LT, 1);
        if (c == '>' && n == '=') return tok(TokenType::GE, 2);
        if (c == '>' ) return tok(TokenType::GT, 1);

        // Skip unknown char
        pos++;
        return {TokenType::END_OF_FILE, "", line};
    }
};
