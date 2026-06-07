module;

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

import TokenStream;

export module Expression;

export struct Expression;
export struct Scope {
    TokenStream stream;
    std::unordered_map<std::string, Expression> variables;
};

export struct Expression {
    using Nil = std::monostate;
    using String = std::string;
    using Number = int;
    // no implicit casting allowed
    struct Bool {
        bool value;
        Bool(bool v) : value(v) {}
        bool operator==(Bool const& other) const {
            return value == other.value;
        }
    };
    struct Closure {
        Scope scope;
    };
    using List = std::vector<Expression>;

    std::variant<Nil, String, Number, Bool, List, Closure, Scope> inner;

    Expression() : inner(Nil{}) {}
    Expression(String str) : inner(std::move(str)) {}
    Expression(Number num) : inner(std::move(num)) {}
    Expression(Bool b) : inner(std::move(b)) {}
    Expression(List list) : inner(std::move(list)) {}
    Expression(Closure closure) : inner(std::move(closure)) {}
    Expression(Scope scope) : inner(std::move(scope)) {}

    template <typename ...Args>
    bool is() const {
        return (std::holds_alternative<Args>(inner) || ...);
    }

    template <typename T>
    T& as() {
        return std::get<T>(inner);
    }
    template <typename T>
    T const& as() const {
        return std::get<T>(inner);
    }

    std::string to_string() const {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Nil>) {
                return "nil";
            } else if constexpr (std::is_same_v<T, String>) {
                return arg;
            } else if constexpr (std::is_same_v<T, Number>) {
                return std::to_string(arg);
            } else if constexpr (std::is_same_v<T, Bool>) {
                return arg.value ? "true" : "false";
            } else if constexpr (std::is_same_v<T, List>) {
                std::string result = "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    result += arg[i].to_string();
                    if (i < arg.size() - 1) {
                        result += ", ";
                    }
                }
                result += "]";
                return result;
            } else if constexpr (std::is_same_v<T, Closure>) {
                return "<closure>";
            } else if constexpr (std::is_same_v<T, Scope>) {
                return "<scope>";
            } else {
                return "<unknown>";
            }
        }, inner);
    }

    bool truthy() const {
        return !is<Nil>() &&
               !(is<String>() && as<String>().empty()) &&
               !(is<List>() && as<List>().empty()) &&
               !(is<Bool>() && !as<Bool>().value) &&
               !(is<Number>() && as<Number>() == 0);
    }

    Expression operator+(Expression const& other) {
        if (this->is<Number>() && other.is<Number>()) {
            return this->as<Number>() + other.as<Number>();
        } else if (this->is<String>() && other.is<String>()) {
            return this->as<String>() + other.as<String>();
        } else if (this->is<List>() && other.is<List>()) {
            List result = this->as<List>();
            const List& other_list = other.as<List>();
            result.insert(result.end(), other_list.begin(), other_list.end());
            return result;
        } else {
            throw std::runtime_error("Unsupported types for addition");
        }
    }
    Expression operator-(Expression const& other) {
        if (this->is<Number>() && other.is<Number>()) {
            return this->as<Number>() - other.as<Number>();
        } else {
            throw std::runtime_error("Unsupported types for subtraction");
        }
    }
    Expression operator*(Expression const& other) {
        if (this->is<Number>() && other.is<Number>()) {
            return this->as<Number>() * other.as<Number>();
        } else {
            throw std::runtime_error("Unsupported types for multiplication");
        }
    }
    Expression operator/(Expression const& other) {
        if (this->is<Number>() && other.is<Number>()) {
            if (other.as<Number>() == 0) {
                throw std::runtime_error("Division by zero");
            }
            return this->as<Number>() / other.as<Number>();
        } else {
            throw std::runtime_error("Unsupported types for division");
        }
    }
    Expression operator%(Expression const& other) {
        if (this->is<Number>() && other.is<Number>()) {
            if (other.as<Number>() == 0) {
                throw std::runtime_error("Modulo by zero");
            }
            return this->as<Number>() % other.as<Number>();
        } else {
            throw std::runtime_error("Unsupported types for modulo");
        }
    }
    bool operator==(Expression const& other) const {
        if (this->inner.index() != other.inner.index()) {
            return false;
        }
        return std::visit([&](auto&& arg1, auto&& arg2) -> bool {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;
            if constexpr (std::is_same_v<T1, T2> && requires (T1 a, T2 b) { a == b; }) {
                return arg1 == arg2;
            } else if constexpr (std::is_same_v<T1, T2>) {
                return &arg1 == &arg2;
            } else {
                throw std::runtime_error("Unsupported types for equality comparison");
            }
        }, this->inner, other.inner);
    }
    bool operator!=(Expression const& other) const {
        return !(*this == other);
    }
    auto operator<=>(Expression const& other) const {
        if (this->inner.index() != other.inner.index()) {
            throw std::runtime_error("Cannot compare different types");
        }
        return std::visit([&](auto&& arg1, auto&& arg2) -> std::partial_ordering {
            using T1 = std::decay_t<decltype(arg1)>;
            using T2 = std::decay_t<decltype(arg2)>;
            if constexpr (std::is_same_v<T1, T2> && requires (T1 a, T2 b) { a < b; }) {
                if (arg1 < arg2) return std::partial_ordering::less;
                if (arg1 > arg2) return std::partial_ordering::greater;
                return std::partial_ordering::equivalent;
            } else {
                throw std::runtime_error("Unsupported types for comparison");
            }
        }, this->inner, other.inner);
    }

    Expression operator[](Expression const& index) const {
        if (this->is<String>() && index.is<Number>()) {
            const String& str = this->as<String>();
            int idx = index.as<Number>();
            if (idx < 0 || idx >= static_cast<int>(str.size())) {
                throw std::runtime_error("String index out of range");
            }
            return String(1, str[idx]);
        } else if (this->is<List>() && index.is<Number>()) {
            const List& list = this->as<List>();
            int idx = index.as<Number>();
            if (idx < 0 || idx >= static_cast<int>(list.size())) {
                throw std::runtime_error("List index out of range");
            }
            return list[idx];
        } else if (this->is<Scope>() && index.is<String>()) {
            const Scope& scope = this->as<Scope>();
            const std::string& var_name = index.as<String>();
            auto it = scope.variables.find(var_name);
            if (it != scope.variables.end()) {
                return it->second;
            } else {
                throw std::runtime_error("Variable not found in scope: " + var_name);
            }
        } else {
            throw std::runtime_error("Unsupported types for indexing");
        }
    }
};

export std::ostream& operator<<(std::ostream& os, Expression const& expr) {
    return os << expr.to_string();
}
