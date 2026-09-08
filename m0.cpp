#include <iostream>
#include <fstream>
#include <string>

using namespace std;

// Function to register a new user
void registerUser() {
    string username, password;
    cout << "\n--- Register ---" << endl;
    cout << "Enter username: ";
    cin >> username;
    cout << "Enter password: ";
    cin >> password;

    // Open file in append mode
    ofstream file("users.txt", ios::app);
    if (file.is_open()) {
        file << username << " " << password << endl;
        file.close();
        cout << "Registration successful!" << endl;
    } else {
        cout << "Error opening file!" << endl;
    }
}

// Function to log in an existing user
bool loginUser() {
    string username, password;
    string uName, pWord;
    
    cout << "\n--- Login ---" << endl;
    cout << "Enter username: ";
    cin >> username;
    cout << "Enter password: ";
    cin >> password;

    // Open file to read credentials
    ifstream file("users.txt");
    if (file.is_open()) {
        while (file >> uName >> pWord) {
            if (uName == username && pWord == password) {
                file.close();
                return true; // Login success
            }
        }
        file.close();
    } else {
        cout << "No registered users found!" << endl;
    }
    return false; // Login failure
}

// Main menu
int main() {
    int choice;
    do {
        cout << "\n1. Register\n2. Login\n3. Exit\nChoice: ";
        cin >> choice;

        switch (choice) {
            case 1:
                registerUser();
                break;
            case 2:
                if (loginUser()) {
                    cout << "\nLogin Successful! Welcome!" << endl;
                } else {
                    cout << "\nInvalid username or password." << endl;
                }
                break;
            case 3:
                cout << "Exiting program." << endl;
                break;
            default:
                cout << "Invalid choice. Try again." << endl;
        }
    } while (choice != 3);

    return 0;
}
