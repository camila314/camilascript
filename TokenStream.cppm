module;

#include <string_view>
#include <functional>

export module TokenStream;

export class TokenStream {
    using str_type = std::u16string_view;
    str_type str;
    size_t pos = 0;
public:
    TokenStream(str_type s, size_t p = 0) : str(s), pos(p) {}

    TokenStream substream(size_t length) const {
        return TokenStream(str.substr(0, pos + length), pos);
    }

    char16_t peek() const {
        return str[pos];
    }

    char16_t get() {
        return str[pos++];
    }

    str_type get_while(std::function<bool(char16_t)> pred) {
        size_t start = pos;
        while (pos < str.size() && pred(str[pos])) {
            pos++;
        }
        return str.substr(start, pos - start);
    }

    str_type get_until(char16_t del, char16_t nest = '\0') {
        size_t start = pos;
        int depth = 0;
        while (pos < str.size()) {
            if (str[pos] == del && depth == 0) {
                break;
            } else if (str[pos] == nest) {
                depth++;
            } else if (str[pos] == del && depth > 0) {
                depth--;
            }
            pos++;
        }
        return str.substr(start, pos - start);
    }

    bool matches(str_type s) {
        if (str.substr(pos, s.size()) == s) {
            pos += s.size();
            return true;
        }
        return false;
    }

    void trim() {
        get_while([](char c) { return std::isspace(c); });
    }

    bool eof() const {
        return pos >= str.size();
    }

    std::pair<size_t, size_t> getPos() const {
        // return {col, row}.
        std::pair<size_t, size_t> pair;

        for (size_t i = 0; i < pos; ++i) {
            if (str[i] == '\n') {
                pair.first = 0;
                pair.second++;
            } else {
                pair.first++;
            }
        }

        return pair;
    }
};