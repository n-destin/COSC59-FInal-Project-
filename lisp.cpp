#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <functional>
#include <cctype>

// Tokenization
enum class TokenType {
    PAREN_OPEN,
    PAREN_CLOSE,
    NUMBER,
    SYMBOL,
};

struct Token {
    TokenType type;
    std::string value;
};

std::vector<Token> tokenize(const std::string& code) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < code.length()) {
        char c = code[i];

        if (isspace(c)) {
            i++;
            continue;
        } else if (c == '(') {
            tokens.push_back({TokenType::PAREN_OPEN, "("});
            i++;
        } else if (c == ')') {
            tokens.push_back({TokenType::PAREN_CLOSE, ")"});
            i++;
        } else if (isdigit(c) || (c == '-' && isdigit(code[i + 1]))) {
            size_t start = i;
            while (i < code.length() && (isdigit(code[i]) || code[i] == '.')) i++;
            tokens.push_back({TokenType::NUMBER, code.substr(start, i - start)});
        } else if (isalpha(c) || strchr("+-*/%<>=!", c)) {
            size_t start = i;
            while (i < code.length() && (isalnum(code[i]) || strchr("+-*/%<>=!", code[i]))) i++;
            tokens.push_back({TokenType::SYMBOL, code.substr(start, i - start)});
        } else {
            throw std::runtime_error(std::string("Unexpected character: ") + c);
        }
    }

    return tokens;
}

// Forward declaration
struct Expression;
using ExprPtr = std::shared_ptr<Expression>;

// Function object
using BuiltinFunc = std::function<ExprPtr(const std::vector<ExprPtr>&)>;

// Environment
struct Environment {
    std::unordered_map<std::string, ExprPtr> vars; 
    std::shared_ptr<Environment> outer;

    Environment(std::shared_ptr<Environment> outer = nullptr) : outer(outer) {}

    bool find(const std::string& var, ExprPtr& result) {
        if (vars.find(var) != vars.end()) {
            result = vars[var];
            return true;
        } else if (outer) {
            return outer->find(var, result);
        }
        return false;
    }

    void set(const std::string& var, ExprPtr value) {
        vars[var] = value;
    }
};

struct Function {
    std::vector<std::string> params;
    ExprPtr body;
    std::shared_ptr<Environment> env;
    BuiltinFunc builtin;

    // Constructor for user-defined functions
    Function(const std::vector<std::string>& params, ExprPtr body, std::shared_ptr<Environment> env)
        : params(params), body(body), env(env) {}

    // Constructor for built-in functions
    Function(BuiltinFunc builtin) : builtin(builtin) {}

    // Default copy and move constructors and assignment operators
    Function(const Function&) = default;
    Function(Function&&) = default;
    Function& operator=(const Function&) = default;
    Function& operator=(Function&&) = default;
};

// AST
struct Expression {
    std::variant<double, std::string, std::vector<ExprPtr>, std::shared_ptr<Function>> value;

    Expression(double num) : value(num) {}
    Expression(const std::string& sym) : value(sym) {}
    Expression(const std::vector<ExprPtr>& list) : value(list) {}
    Expression(std::shared_ptr<Function> func) : value(func) {}
};


// Parsing
ExprPtr parseExpression(std::vector<Token>& tokens, size_t& pos);

ExprPtr parseList(std::vector<Token>& tokens, size_t& pos) {
    std::vector<ExprPtr> list;
    pos++; // Skip '('
    while (pos < tokens.size() && tokens[pos].type != TokenType::PAREN_CLOSE) {
        list.push_back(parseExpression(tokens, pos));
    }
    if (pos == tokens.size() || tokens[pos].type != TokenType::PAREN_CLOSE) {
        throw std::runtime_error("Missing closing parenthesis");
    }
    pos++; // Skip ')'
    return std::make_shared<Expression>(list);
}

ExprPtr parseExpression(std::vector<Token>& tokens, size_t& pos) {
    Token token = tokens[pos];
    if (token.type == TokenType::NUMBER) {
        pos++;
        return std::make_shared<Expression>(std::stod(token.value));
    } else if (token.type == TokenType::SYMBOL) {
        pos++;
        return std::make_shared<Expression>(token.value);
    } else if (token.type == TokenType::PAREN_OPEN) {
        return parseList(tokens, pos);
    } else {
        throw std::runtime_error("Unexpected token");
    }
}

ExprPtr parse(const std::string& code) {
    auto tokens = tokenize(code);
    size_t pos = 0;
    return parseExpression(tokens, pos);
}

// Evaluation
bool isSymbol(ExprPtr expr) {
    return std::holds_alternative<std::string>(expr->value);
}

bool isNumber(ExprPtr expr) {
    return std::holds_alternative<double>(expr->value);
}

bool isList(ExprPtr expr) {
    return std::holds_alternative<std::vector<ExprPtr>>(expr->value);
}

bool isFunction(ExprPtr expr) {
    return std::holds_alternative<std::shared_ptr<Function>>(expr->value);
}

ExprPtr eval(ExprPtr expr, std::shared_ptr<Environment> env) {
    if (isSymbol(expr)) {
        // Variable lookup
        ExprPtr result;
        if (env->find(std::get<std::string>(expr->value), result)) {
            return result;
        } else {
            throw std::runtime_error("Undefined symbol: " + std::get<std::string>(expr->value));
        }
    } else if (isNumber(expr)) {
        // Numbers evaluate to themselves
        return expr;
    } else if (isList(expr)) {
        auto list = std::get<std::vector<ExprPtr>>(expr->value);
        if (list.empty()) {
            return expr;
        }

        // Get the first element to determine the operation
        auto first = list[0];

        if (isSymbol(first)) {
            std::string sym = std::get<std::string>(first->value);

            if (sym == "define") {
                // (define var expr)
                if (list.size() != 3 || !isSymbol(list[1])) {
                    throw std::runtime_error("Invalid define syntax");
                }
                std::string varName = std::get<std::string>(list[1]->value);
                ExprPtr value = eval(list[2], env);
                env->set(varName, value);
                return value;
            } else if (sym == "lambda") {
                // (lambda (params) body)
                if (list.size() != 3 || !isList(list[1])) {
                    throw std::runtime_error("Invalid lambda syntax");
                }
                std::vector<std::string> params;
                for (auto& param : std::get<std::vector<ExprPtr>>(list[1]->value)) {
                    if (!isSymbol(param)) {
                        throw std::runtime_error("Lambda parameters must be symbols");
                    }
                    params.push_back(std::get<std::string>(param->value));
                }
                ExprPtr body = list[2];
                auto func = std::make_shared<Function>(params, body, env);
                return std::make_shared<Expression>(func);
            } else if (sym == "if") {
                // (if cond then else)
                if (list.size() != 4) {
                    throw std::runtime_error("Invalid if syntax");
                }
                ExprPtr cond = eval(list[1], env);
                if (isNumber(cond) && std::get<double>(cond->value) != 0) {
                    return eval(list[2], env);
                } else {
                    return eval(list[3], env);
                }
            } else {
                // Function application
                ExprPtr funcExpr = eval(first, env);
                if (!isFunction(funcExpr)) {
                    throw std::runtime_error("First element is not a function");
                }
                auto func = std::get<std::shared_ptr<Function>>(funcExpr->value);

                // Evaluate arguments
                std::vector<ExprPtr> args;
                for (size_t i = 1; i < list.size(); ++i) {
                    args.push_back(eval(list[i], env));
                }

                if (func->builtin) {
                    // Built-in function
                    return func->builtin(args);
                } else {
                    // User-defined function
                    if (args.size() != func->params.size()) {
                        throw std::runtime_error("Incorrect number of arguments");
                    }
                    auto localEnv = std::make_shared<Environment>(func->env);
                    for (size_t i = 0; i < func->params.size(); ++i) {
                        localEnv->set(func->params[i], args[i]);
                    }
                    return eval(func->body, localEnv);
                }
            }
        } else {
            throw std::runtime_error("First element must be a symbol");
        }
    }

    throw std::runtime_error("Invalid expression");
}

// Built-in Functions
void addBuiltins(std::shared_ptr<Environment> env) {
    env->set("+", std::make_shared<Expression>(std::make_shared<Function>([](const std::vector<ExprPtr>& args) {
        double sum = 0;
        for (auto& arg : args) {
            if (!isNumber(arg)) {
                throw std::runtime_error("Arguments to '+' must be numbers");
            }
            sum += std::get<double>(arg->value);
        }
        return std::make_shared<Expression>(sum);
    })));

    env->set("-", std::make_shared<Expression>(std::make_shared<Function>([](const std::vector<ExprPtr>& args) {
        if (args.empty()) {
            throw std::runtime_error("'-' requires at least one argument");
        }
        if (!isNumber(args[0])) {
            throw std::runtime_error("Arguments to '-' must be numbers");
        }
        double result = std::get<double>(args[0]->value);
        if (args.size() == 1) {
            return std::make_shared<Expression>(-result);
        }
        for (size_t i = 1; i < args.size(); ++i) {
            if (!isNumber(args[i])) {
                throw std::runtime_error("Arguments to '-' must be numbers");
            }
            result -= std::get<double>(args[i]->value);
        }
        return std::make_shared<Expression>(result);
    })));

    // Implement '*', '/', '<', '>', '==' similarly
}

// Main
int main() {
    auto globalEnv = std::make_shared<Environment>();
    addBuiltins(globalEnv);

    std::string line;
    while (true) {
        std::cout << "lisp> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        try {
            ExprPtr expr = parse(line);
            ExprPtr result = eval(expr, globalEnv);
            if (isNumber(result)) {
                std::cout << std::get<double>(result->value) << "\n";
            } else if (isSymbol(result)) {
                std::cout << std::get<std::string>(result->value) << "\n";
            } else if (isFunction(result)) {
                std::cout << "<function>\n";
            } else if (isList(result)) {
                std::cout << "<list>\n";
            } else {
                std::cout << "Unknown result type\n";
            }
        } catch (const std::exception& ex) {
            std::cerr << "Error: " << ex.what() << "\n";
        }
    }
    return 0;
}
