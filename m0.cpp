// m0.cpp - Login module + shared input helpers (NO main here)
//
// Passwords: users.txt holds one "username record" pair per line, where record is
//   sha256$<32 hex salt>$<64 hex SHA-256 of salt+password>
// A random salt per user means two users with the same password get different records.
// NOTE (honest limit): a single salted SHA-256 is far better than plain text but is
// not a modern password hash (bcrypt / argon2 are deliberately slow). Fine for this
// project; use a real password-hashing library in production.
//
// Old plain-text lines ("username password") are upgraded automatically the next
// time anyone registers or logs in.
#include "m0.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <vector>
using namespace std;

// ================== Input helpers ==================
// Handles Ctrl+Z / Ctrl+D (end of input) so loops can never spin forever
void exitIfInputClosed()
{
    if (cin.eof())
    {
        cout << "\nInput closed. Exiting.\n";
        exit(0);
    }
}

// Read one word (no spaces) and discard the rest of the line
string readToken(const string& prompt)
{
    string s;
    while (true)
    {
        cout << prompt;
        if (cin >> s)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return s;
        }
        exitIfInputClosed();
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// Read a whole line. Must be non-empty and must not contain '|'
// (the pipe is the field separator in the save files)
string readLine(const string& prompt)
{
    string s;
    while (true)
    {
        cout << prompt;
        if (!getline(cin, s)) exitIfInputClosed();
        if (s.empty())
            cout << "This field cannot be empty.\n";
        else if (s.find('|') != string::npos)
            cout << "The '|' character is not allowed.\n";
        else
            return s;
    }
}

int readInt(const string& prompt, int minVal, int maxVal)
{
    int v;
    while (true)
    {
        cout << prompt;
        if (cin >> v)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            if (v >= minVal && v <= maxVal) return v;
            cout << "Enter a number between " << minVal << " and " << maxVal << ".\n";
            continue;
        }
        exitIfInputClosed();
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input. Please enter a number.\n";
    }
}

double readDouble(const string& prompt, double minVal, double maxVal)
{
    double v;
    while (true)
    {
        cout << prompt;
        if (cin >> v)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            if (v >= minVal && v <= maxVal) return v;
            cout << "Enter a value between " << minVal << " and " << maxVal << ".\n";
            continue;
        }
        exitIfInputClosed();
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input. Please enter a number.\n";
    }
}

// ================== SHA-256 (FIPS 180-4) ==================
namespace {

const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void compress(uint32_t h[8], const unsigned char block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
    for (int i = 16; i < 64; i++)
    {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++)
    {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

string toHex(const unsigned char* bytes, size_t n)
{
    static const char* digits = "0123456789abcdef";
    string out;
    for (size_t i = 0; i < n; i++) { out += digits[bytes[i] >> 4]; out += digits[bytes[i] & 15]; }
    return out;
}

string randomSaltHex()
{
    random_device rd;
    unsigned char bytes[16];
    for (unsigned char& b : bytes) b = static_cast<unsigned char>(rd() & 0xFF);
    return toHex(bytes, sizeof bytes);
}

const string PREFIX = "sha256$";
bool isHashedRecord(const string& r) { return r.compare(0, PREFIX.size(), PREFIX) == 0; }

struct UserLine { string name, record; };

vector<UserLine> readUsers(bool& changed)
{
    vector<UserLine> users;
    changed = false;
    ifstream file(USER_FILE);
    string uName, secret;
    while (file >> uName >> secret)
    {
        if (!isHashedRecord(secret)) { secret = makePasswordRecord(secret); changed = true; }
        users.push_back({uName, secret});
    }
    return users;
}

// Write to a temp file, then swap it in, so a crash cannot leave users.txt half-written
bool writeUsers(const vector<UserLine>& users)
{
    string tmp = string(USER_FILE) + ".tmp";
    {
        ofstream out(tmp, ios::trunc);
        if (!out.is_open()) return false;
        for (const UserLine& u : users) out << u.name << " " << u.record << "\n";
        out.flush();
        if (!out) return false;
    }
    std::remove(USER_FILE);
    return std::rename(tmp.c_str(), USER_FILE) == 0;
}

// Loads users.txt; upgrades any old plain-text lines on the spot
vector<UserLine> loadUsersUpgrading()
{
    bool changed;
    vector<UserLine> users = readUsers(changed);
    if (changed && writeUsers(users))
        cout << "(Old plain-text passwords in " << USER_FILE << " were upgraded to hashed ones.)\n";
    return users;
}

} // namespace

string sha256Hex(const string& data)
{
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    string msg = data;
    uint64_t bitLen = uint64_t(data.size()) * 8;
    msg += char(0x80);
    while (msg.size() % 64 != 56) msg += char(0);
    for (int i = 7; i >= 0; i--) msg += char((bitLen >> (i * 8)) & 0xFF);
    for (size_t off = 0; off < msg.size(); off += 64)
        compress(h, reinterpret_cast<const unsigned char*>(msg.data()) + off);
    unsigned char digest[32];
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++) digest[i * 4 + j] = (h[i] >> (24 - 8 * j)) & 0xFF;
    return toHex(digest, 32);
}

string makePasswordRecord(const string& password)
{
    string salt = randomSaltHex();
    return PREFIX + salt + "$" + sha256Hex(salt + password);
}

bool verifyPassword(const string& password, const string& record)
{
    if (!isHashedRecord(record)) return false;
    size_t dollar = record.find('$', PREFIX.size());
    if (dollar == string::npos) return false;
    string salt = record.substr(PREFIX.size(), dollar - PREFIX.size());
    string expected = record.substr(dollar + 1);
    string actual = sha256Hex(salt + password);
    if (actual.size() != expected.size()) return false;
    unsigned char diff = 0;                       // compare every character: no early exit
    for (size_t i = 0; i < actual.size(); i++) diff |= (unsigned char)(actual[i] ^ expected[i]);
    return diff == 0;
}

// ================== Login / Register ==================
const char* USER_FILE = "users.txt";

bool usernameExists(const string& username)
{
    for (const UserLine& u : loadUsersUpgrading())
        if (u.name == username) return true;
    return false;
}

void registerUser()
{
    cout << "\n--- Register ---\n";
    string username = readToken("Enter username: ");

    if (usernameExists(username))
    {
        cout << "Username already taken!\n";
        return;
    }

    string password = readToken("Enter password: ");

    ofstream file(USER_FILE, ios::app);
    if (!file.is_open())
    {
        cout << "Error opening file!\n";
        return;
    }
    file << username << " " << makePasswordRecord(password) << "\n";
    cout << "Registration successful!\n";
}

// Returns true on success and puts the logged-in name in loggedInUser
bool loginUser(string& loggedInUser)
{
    cout << "\n--- Login ---\n";

    ifstream probe(USER_FILE);
    if (!probe.is_open())
    {
        cout << "No registered users found! Please register first.\n";
        return false;
    }
    probe.close();

    string username = readToken("Enter username: ");
    string password = readToken("Enter password: ");

    for (const UserLine& u : loadUsersUpgrading())
    {
        if (u.name == username && verifyPassword(password, u.record))
        {
            loggedInUser = username;
            return true;
        }
    }
    return false;
}
