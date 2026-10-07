// Simple authentication module (DEVELOPMENT ONLY: passwords are stored in plaintext)
// Build: g++ -std=c++17 module3_authentication_simple.cpp -o auth
#include <iostream>
#include <fstream>
#include <string>
#include <limits>
using namespace std;

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
    string username, password;
    cout << "\n--- Register ---\n";
    cout << "Enter username: ";
    cin >> username;

    if (usernameExists(username))
    {
        cout << "Username already taken!\n";
        return;
    }

    cout << "Enter password: ";
    cin >> password;

    ofstream file(USER_FILE, ios::app);
    if (!file.is_open())
    {
        cout << "Error opening file!\n";
        return;
    }
    file << username << " " << password << "\n";
    cout << "Registration successful!\n";
}

bool loginUser()
{
    string username, password, uName, pWord;
    cout << "\n--- Login ---\n";
    cout << "Enter username: ";
    cin >> username;
    cout << "Enter password: ";
    cin >> password;

    ifstream file(USER_FILE);
    if (!file.is_open())
    {
        cout << "No registered users found!\n";
        return false;
    }

    while (file >> uName >> pWord)
        if (uName == username && pWord == password)
            return true;

    return false;
}

int main()
{
    int choice = 0;
    do
    {
        cout << "\n1. Register\n2. Login\n3. Exit\nChoice: ";
        if (!(cin >> choice))
        {
            if (cin.eof()) return 0;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input. Enter a number.\n";
            choice = 0;
            continue;
        }

        switch (choice)
        {
            case 1:
                registerUser();
                break;
            case 2:
                if (loginUser()) cout << "\nLogin Successful! Welcome!\n";
                else cout << "\nInvalid username or password.\n";
                break;
            case 3:
                cout << "Exiting program.\n";
                break;
            default:
                cout << "Invalid choice. Try again.\n";
        }
    } while (choice != 3);

    return 0;
}