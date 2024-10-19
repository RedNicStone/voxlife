
#include <bsp/readfile.h>
#include <bsp/primitives.h>
#include <bsp/readfile_info.h>

#include <iostream>
#include <cctype>
#include <format>


namespace voxlife::bsp {

    enum token_type {
        TOKEN_LEFT_BRACE,
        TOKEN_RIGHT_BRACE,
        TOKEN_STRING,
        TOKEN_EOF,
        TOKEN_ERROR
    };

    struct token {
        token_type type = TOKEN_ERROR;
        std::string_view value;
    };

    enum parser_state {
        STATE_START,
        STATE_BLOCK,
        STATE_KEY,
        STATE_VALUE
    };

    struct tokenizer {
        std::string_view input;
        size_t pos;

        explicit tokenizer(std::string_view input) : input(input), pos(0) {}

        token getToken() {
            skipWhitespace();
            if (pos >= input.size())
                return { TOKEN_EOF, { } };

            char c = input[pos];

            switch (c) {
                case '{':
                    ++pos;
                    return { TOKEN_LEFT_BRACE, "{" };

                case '}':
                    ++pos;
                    return { TOKEN_RIGHT_BRACE, "}" };

                case '"': {
                    size_t start = ++pos;
                    while (pos < input.size()) {
                        c = input[pos++];
                        if (c == '"') {
                            size_t length = pos - start - 1;
                            ++pos;
                            return { TOKEN_STRING, input.substr(start, length) };
                        }
                    }
                    // If we reach here, no closing quote found
                    return { TOKEN_ERROR, "Unterminated string" };
                }

                case '\0':
                    return { TOKEN_EOF, { } };

                default:
                    // Invalid character
                    return { TOKEN_ERROR, input.substr(pos++, 1) };
            }
        }

        void skipWhitespace() {
            while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos])))
                ++pos;
        }
    };



    std::vector<entity> get_entities(bsp_handle handle) {
        auto& info = *reinterpret_cast<bsp_info*>(handle);
        std::vector<entity> result;

        tokenizer tokenizer(info.entities_str);
        parser_state state = STATE_START;

        while (true) {
            token token = tokenizer.getToken();

            if (token.type == TOKEN_EOF) {
                if (state == STATE_START)
                    break; // Successfully reached the end of input
                else
                    throw std::runtime_error("Unexpected end of input");
            } else if (token.type == TOKEN_ERROR)
                throw std::runtime_error(std::format("Error: {}", token.value));

            switch (state) {
                default:
                case STATE_START:
                    if (token.type == TOKEN_LEFT_BRACE) {
                        result.emplace_back();
                        state = STATE_BLOCK;
                    } else
                        throw std::runtime_error("Expected '{' at start");
                    break;

                case STATE_BLOCK:
                    if (token.type == TOKEN_STRING) {
                        result.back().pairs.emplace_back(token.value, "");
                        result.back().pairs.back().key = token.value;
                        state = STATE_VALUE;
                    } else if (token.type == TOKEN_RIGHT_BRACE)
                        state = STATE_START;
                    else
                        throw std::runtime_error("Expected string \"key\" or '}' after '{'");
                    break;

                case STATE_VALUE:
                    if (token.type == TOKEN_STRING) {
                        result.back().pairs.back().value = token.value;
                        state = STATE_BLOCK;
                    } else
                        throw std::runtime_error("Expected string \"value\" after key");
                    break;
            }
        }

        return result;
    }

}

