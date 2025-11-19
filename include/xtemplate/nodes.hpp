#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// Soft parse issues collected during body/VARS parsing. Prefer attaching a
// sink to TokenStream over threading error strings through every call.
struct ParseDiagnostics {
        std::vector<std::string> messages;

        bool empty() const { return messages.empty(); }

        void add(std::string msg) {
            if (!msg.empty())
                messages.push_back(std::move(msg));
        }

        std::string join(const char *bullet = "- ") const {
            std::string out;
            for (const auto &m : messages) {
                out += bullet;
                out += m;
                out += '\n';
            }
            return out;
        }
};

struct ParseError : std::runtime_error {
        using std::runtime_error::runtime_error;
};

typedef enum {
    NODE_TEXT,
    NODE_COND,
} NodeType;

typedef enum {
    ON,
    OFF,
    EQ,
    NEQ,
    ELIF_ON,
    ELIF_OFF,
    ELIF_EQ,
    ELIF_NEQ,
    ELSE,
    EMPTY,
    ELIF_EMPTY,
    NEMPTY,
    ELIF_NEMPTY,
} CondType;

typedef struct {
        CondType type;
        const char *arg1;
        const char *arg2; // null for ON/OFF
} Cond;

struct Node {
        NodeType type;
        Node *next_sibling;
        Node *cond_chain_next_sibling;
        Node *first_child;
        union {
                struct {
                        const char *content;
                        size_t len;
                } text;
                struct {
                        Cond *cond;
                } cond;
        } as;

        // Soft-fails into diag (if set); otherwise throws ParseError.
        static Node *parse(const std::string &body,
                           ParseDiagnostics *diag = nullptr);
        static void destroy(Node *n);

        void render(const std::unordered_map<std::string, std::string> &vars,
                    std::string &result, bool render_empty_vals) const;
};

// Parse body into an AST and render with named $var substitutions.
// Soft parse issues go to diag (and rendering still proceeds with fallbacks).
void render_xtemplate(const std::string &body,
                      const std::unordered_map<std::string, std::string> &vars,
                      std::string &result, bool render_empty_vals,
                      ParseDiagnostics *diag = nullptr);

// Split "XVARIANT:a-b-c" into {"a","b","c"}; empty if not an XVARIANT type.
std::vector<std::string> parse_xvariant_options(const std::string &var_type);
bool is_xcheckbox(const std::string &var_type);
bool is_xvariant(const std::string &var_type);
std::string display_var_type(const std::string &var_type);
