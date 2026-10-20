// m0.h - Login module + shared console input helpers
#pragma once
#include <string>

// ---- input helpers (every loop exits cleanly if input is closed) ----
void exitIfInputClosed();
std::string readToken(const std::string& prompt);                 // one word
std::string readLine(const std::string& prompt);                  // non-empty, no '|'
int    readInt(const std::string& prompt, int minVal, int maxVal);
double readDouble(const std::string& prompt, double minVal, double maxVal);

// ---- login / register (passwords are stored as salted SHA-256, never in plain text) ----
extern const char* USER_FILE;
bool usernameExists(const std::string& username);
void registerUser();
bool loginUser(std::string& loggedInUser);

// ---- exposed so the tests can check them ----
std::string sha256Hex(const std::string& data);
std::string makePasswordRecord(const std::string& password);               // "sha256$<salt>$<hash>"
bool verifyPassword(const std::string& password, const std::string& record);
