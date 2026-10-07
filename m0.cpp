// m0.cpp - Login module + shared input helpers (NO main here; used by m1.cpp)
// DEVELOPMENT ONLY: passwords are stored in plaintext in users.txt
#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <limits>
#include <cstdlib>
using namespace std;

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
// (the pipe is the field separator in the save file)
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

// ================== Login / Register ==================
const char* USER_FILE = "users.txt";

bool usernameExists(const string& username)
{
    ifstream file(USER_FILE);
    string uName, pWord;
    while (file >> uName >> pWord)
        if (uName == username) return true;
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
    file << username << " " << password << "\n";
    cout << "Registration successful!\n";
}

// Returns true on success and puts the logged-in name in loggedInUser
bool loginUser(string& loggedInUser)
{
    cout << "\n--- Login ---\n";

    ifstream file(USER_FILE);
    if (!file.is_open())
    {
        cout << "No registered users found! Please register first.\n";
        return false;
    }

    string username = readToken("Enter username: ");
    string password = readToken("Enter password: ");

    string uName, pWord;
    while (file >> uName >> pWord)
    {
        if (uName == username && pWord == password)
        {
            loggedInUser = username;
            return true;
        }
    }
    return false;
}