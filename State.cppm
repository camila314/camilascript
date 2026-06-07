module;

#include <string>
#include <vector>

import Expression;
import TokenStream;

export module State;

export struct Error : std::exception {
    std::string message;
    std::pair<size_t, size_t> position; // {col, row}

    Error(const std::string& msg, const std::pair<size_t, size_t>& pos) : message(msg), position(pos) {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

export class State {
    std::vector<Scope> scopes;
public:
    State(TokenStream s) {
        scopes.push_back({std::move(s), {}});
    }

    TokenStream& stream() {
        return scopes.back().stream;
    }

    Scope& current_scope() {
        return scopes.back();
    }

    auto guard_scope(Scope s) {
        scopes.push_back(std::move(s));

        struct Guard {
            State& state;
            ~Guard() {
                state.scopes.pop_back();
            }
        };
        return Guard{*this};
    }

    Expression* get_variable(std::string const& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto var_it = it->variables.find(name);
            if (var_it != it->variables.end()) {
                return &var_it->second;
            }
        }
        return nullptr;
    }

    Expression& set_variable(std::string const& name, Expression value) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto var_it = it->variables.find(name);
            if (var_it != it->variables.end()) {
                return (var_it->second = std::move(value));
            }
        }
        return (scopes.back().variables[name] = std::move(value));
    }

    Expression& define_variable(std::string const& name, Expression value) {
        return (scopes.back().variables[name] = std::move(value));
    }
};