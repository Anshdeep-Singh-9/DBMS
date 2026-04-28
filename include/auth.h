#ifndef AUTH_H
#define AUTH_H

#include <string>
#include <vector>
#include <unordered_map>
#include "declaration.h"

class AuthManager {
public:
    static bool init();
    static bool register_user(const std::string& username, const std::string& password);
    static bool authenticate(const std::string& username, const std::string& password);
    static bool user_exists(const std::string& username);
    static bool has_any_user();

    // Session management for API
    static std::string create_session(const std::string& username);
    static bool validate_session(const std::string& token);
    static void end_session(const std::string& token);
    static std::string get_user_from_session(const std::string& token);

private:
    static void create_auth_table();
    static std::unordered_map<std::string, std::string> sessions; // token -> username
};

#endif
