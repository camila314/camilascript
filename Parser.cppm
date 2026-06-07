module;

#include <charconv>
#include <codecvt>
#include <string>
#include <string_view>
#include <functional>
#include <iostream>
#include <fstream>

import TokenStream;
import Expression;
import State;

export module Parser;

export Expression parse(State& state);

namespace helper {
    std::string convert_to_utf8(std::u16string_view str) {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
        return convert.to_bytes(std::u16string(str).data());
    }

    std::string variable_name(TokenStream& stream) {
        stream.trim();
        auto str = stream.get_while([](char16_t c) {
            return std::isalnum(c);
        });
        return convert_to_utf8(str);
    }

    template <typename T>
    T parse_as(State& state) {
        auto data = parse(state);

        if (auto ptr = std::get_if<T>(&data.inner)) {
            return *ptr;
        } else {
            std::string realtypename;
            std::visit([&](auto&& t) {
                realtypename = typeid(decltype(t)).name();
            }, data.inner);
            throw Error(std::string("Expected type was ") + typeid(T).name() + " found " + realtypename, state.stream().getPos());
        }
    }

    Expression eval_closure(State& state, Expression::Closure const& closure, std::vector<Expression> const& args) {
        auto guard = state.guard_scope(closure.scope);

        for (size_t i = 0; i < args.size(); ++i) {
            state.define_variable(std::to_string(i), args[i]);
        }

        Expression res;
        while (!state.stream().eof()) {
            res = parse(state);
            state.stream().trim();
        }
        return res;
    }
}

// builtin
namespace builtin {
    Expression print(State& state) {
        std::cout << parse(state) << std::flush;
        return Expression();
    }

    Expression range(State& state) {
        auto from = helper::parse_as<Expression::Number>(state);
        auto to = helper::parse_as<Expression::Number>(state);

        Expression::List list;
        for (int i = from; i < to; ++i) {
            list.emplace_back(i);
        }
        return list;
    }

    Expression input(State& state) {
        std::string line;
        std::getline(std::cin, line);
        return line;
    }

    Expression len(State& state) {
        auto thing = parse(state);
        if (thing.is<Expression::String>()) {
            return thing.as<Expression::String>().size();
        } else if (thing.is<Expression::List>()) {
            return thing.as<Expression::List>().size();
        } else {
            throw Error("Unsupported type for len", state.stream().getPos());
        }
    }

    Expression concat(State& state) {
        // optionally taken a string separator as the first argument
        // second ( or first argument if no separator ) is a list

        std::string separator;
        if (state.stream().peek() == '"') {
            separator = helper::parse_as<Expression::String>(state);
        }

        auto list = helper::parse_as<Expression::List>(state);

        std::string result;
        for (size_t i = 0; i < list.size(); ++i) {
            result += list[i].to_string();
            if (i < list.size() - 1) {
                result += separator;
            }
        }
        return result;
    }

    Expression sqrt(State& state) {
        auto num = helper::parse_as<Expression::Number>(state);
        if (num < 0) {
            throw Error("Cannot take square root of a negative number", state.stream().getPos());
        }
        return static_cast<int>(std::sqrt(num));
    }
};

// keychars
namespace parser {
    Expression builtin(State& state) {
        auto name = helper::variable_name(state.stream());

        if (name == "print") {
            return builtin::print(state);
        } else if (name == "range") {
            return builtin::range(state);
        } else if (name == "input") {
            return builtin::input(state);
        } else if (name == "len") {
            return builtin::len(state);
        } else if (name == "concat") {
            return builtin::concat(state);
        } else if (name == "sqrt") {
            return builtin::sqrt(state);
        } else {
            throw Error("Unknown builtin: " + std::string(name), state.stream().getPos());
        }
    }

    Expression numeric(State& state) {
        if (state.stream().matches(u"true"))
            return Expression::Bool(true);
        if (state.stream().matches(u"false"))
            return Expression::Bool(false);

        auto chars = state.stream().get_while([](char c) { return std::isdigit(c); });
        int value;

        std::string narrow_str = helper::convert_to_utf8(chars);

        if (std::from_chars(narrow_str.data(), narrow_str.data() + narrow_str.size(), value).ec == std::errc()) {
            return value;
        } else {
            throw Error("Invalid number format", state.stream().getPos());
        }
    }

    Expression string(State& state) {
        std::string result;
        while (true) {
            auto ch = state.stream().get();
            if (ch == '"') {
                break;
            } else if (ch == '\\') {
                auto next = state.stream().get();
                switch (next) {
                case 'n':
                    result += '\n';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '"':
                    result += '"';
                    break;
                default:
                    throw Error(std::string("Unknown escape sequence: \\") + (char)next, state.stream().getPos());
                }
            } else {
                result += static_cast<char>(ch);
            }
        }
        return result;
    }

    Expression define(State& state) {
        auto name = std::string(helper::variable_name(state.stream()));
        return state.define_variable(name, parse(state));
    }

    Expression assign(State& state) {
        auto name = std::string(helper::variable_name(state.stream()));

        if (state.get_variable(name)) {
            return state.set_variable(name, parse(state));
        } else {
            throw Error("Undefined variable: " + name, state.stream().getPos());
        }
    }

    Expression variable(State& state) {
        auto name = helper::variable_name(state.stream());

        if (auto var = state.get_variable(std::string(name))) {
            return *var;
        } else {
            throw Error("Undefined variable: " + std::string(name), state.stream().getPos());
        }
    }

    Expression list_builder(State& state) {
        std::vector<Expression> list;
        while (state.stream().peek() != ']') {
            list.push_back(parse(state));
        }
        state.stream().get(); // consume ']'
        return list;
    }

    Expression closure(State& state) {
        auto copy = state.stream();
        auto size = state.stream().get_until('}', '{').size();

        state.stream().get();
        return Expression::Closure({copy.substream(size), {}});
    }

    Expression evaluate(State& state) {
        auto closure = helper::parse_as<Expression::Closure>(state);
        auto args = helper::parse_as<Expression::List>(state);

        try {
            return helper::eval_closure(state, closure, args);
        } catch (Expression expr) {
            return expr;
        }
    }

    Expression scope_accessor(State& state) {
        auto name = helper::variable_name(state.stream());
        auto scope = parse(state);

        auto ret = scope[name];

        if (scope.is<Scope>() && ret.is<Expression::Closure>()) {
            auto closure = ret.as<Expression::Closure>();
            for (auto& [k, v] : scope.as<Scope>().variables) {
                closure.scope.variables[k] = v;
            }
            return closure;
        }

        return ret;
    }

    Expression enclosure(State& state) {
        auto left = parse(state);

        try {
            while (state.stream().peek() != ')') {
                state.stream().trim();
                auto op = state.stream().get();
                auto next = parse(state);

                switch (op) {
                case '+':
                    left = left + next;
                    break;
                case '-':
                    left = left - next;
                    break;
                case '*':
                    left = left * next;
                    break;
                case u'÷':
                    left = left / next;
                    break;
                case '%':
                    left = left % next;
                    break;
                case '=':
                    left = Expression::Bool(left == next);
                    break;
                case u'≠':
                    left = Expression::Bool(left != next);
                    break;
                case '>':
                    left = Expression::Bool(left > next);
                    break;
                case u'≥':
                    left = Expression::Bool(left >= next);
                    break;
                case '<':
                    left = Expression::Bool(left < next);
                    break;
                case u'≤':
                    left = Expression::Bool(left <= next);
                    break;
                case '&':
                    left = Expression::Bool(left.truthy() && next.truthy());
                    break;
                case '|':
                    left = Expression::Bool(left.truthy() || next.truthy());
                    break;
                case '.':
                    left = left[next];
                    break;
                default:
                    throw Error(std::string("Unexpected operator in enclosure: ") + (char)op, state.stream().getPos());
                }
                state.stream().trim();
            }
            state.stream().get();
            return left;
        } catch (std::runtime_error e) {
            throw Error(e.what(), state.stream().getPos());
        }
    }

    Expression condition(State& state) {
        auto condition = parse(state);
        auto true_branch = helper::parse_as<Expression::Closure>(state);
        auto false_branch = helper::parse_as<Expression::Closure>(state);

        if (condition.truthy()) {
            return helper::eval_closure(state, true_branch, {});
        } else {
            return helper::eval_closure(state, false_branch, {});
        }
    }

    Expression infinite_loop(State& state) {
        auto closure = helper::parse_as<Expression::Closure>(state);

        Expression res;
        while (true) {
            auto guard = state.guard_scope(closure.scope);

            try {
                while (!state.stream().eof()) {
                    res = parse(state);
                }
            } catch (Expression expr) {
                if (expr.is<Expression::Nil>()) {
                    continue;
                } else {
                    res = expr;
                    break;
                }
            }
        }

        return res;
    }

    Expression for_in(State& state) {
        auto var = helper::variable_name(state.stream());
        auto iterable = parse(state);
        auto closure = helper::parse_as<Expression::Closure>(state);

        if (!iterable.is<Expression::String>() && !iterable.is<Expression::List>()) {
            throw Error("Invalid iterable type", state.stream().getPos());
        }
        
        Expression res;
        std::visit([&](auto&& v) -> void {
            if constexpr (requires (decltype(v) a) { a.begin(); }) {
                for (auto& value : v) {
                    auto guard = state.guard_scope(closure.scope);
                    state.set_variable(var, value);
                    try {
                        while (!state.stream().eof()) {
                            res = parse(state);
                        }
                    } catch (Expression expr) {
                        if (expr.is<Expression::Nil>()) {
                            continue;
                        } else {
                            res = expr;
                            break;
                        }
                    }
                }
            }
        }, iterable.inner);
        return res;
    }
};

export Expression parse(State& state) {
    while (!state.stream().eof()) {
        auto keychar = state.stream().get();
        switch (keychar) {
        case '#':
            return parser::numeric(state);
        case '\'':
            return parser::builtin(state);
        case ':':
            return parser::define(state);
        case ';':
            return parser::assign(state);
        case '$':
            return parser::variable(state);
        case '"':
            return parser::string(state);
        case '_':
            return Expression();
        case '[':
            return parser::list_builder(state);
        case '@':
            return parser::evaluate(state);
        case '{':
            return parser::closure(state);
        case '?':
            return parser::condition(state);
        case '\\':
            throw parse(state);
        case '/':
            state.stream().get_until('\n');
            return {};
        case '^':
            return parser::infinite_loop(state);
        case '~':
            return state.current_scope();
        case ',':
            return parser::scope_accessor(state);
        case '\t':
        case ' ':
        case '\n':
            continue;
        case '(':
            return parser::enclosure(state);
        case '!':
            return Expression::Bool(!parse(state).truthy());
        case '`':
            return parser::for_in(state);
        default:
            throw Error("Unexpected character in input", state.stream().getPos());
        }
    }

    return {};
}
