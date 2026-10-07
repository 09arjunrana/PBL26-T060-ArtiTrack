#include <iostream>
#include <string>
#include <limits>
using namespace std;

const int MAX_ARTIFACTS = 100;

// Safely read an int; clears bad input and the leftover newline
int readInt(const string& prompt)
{
    int value;
    while (true)
    {
        cout << prompt;
        if (cin >> value)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return value;
        }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input. Please enter a number.\n";
    }
}

// Read a non-empty line
string readLine(const string& prompt)
{
    string s;
    while (true)
    {
        cout << prompt;
        getline(cin, s);
        if (!s.empty()) return s;
        cout << "This field cannot be empty.\n";
    }
}

class Artifact
{
public:
    int id = 0;
    string name, origin, era, condition, location;

    // ID is set separately so duplicates can be rejected before asking for the rest
    void inputDetails()
    {
        name      = readLine("Enter Artifact Name: ");
        origin    = readLine("Enter Origin: ");
        era       = readLine("Enter Era: ");
        condition = readLine("Enter Condition: ");
        location  = readLine("Enter Location: ");
    }

    void display() const
    {
        cout << "Artifact ID: " << id << '\n'
             << "Name: "        << name << '\n'
             << "Origin: "      << origin << '\n'
             << "Era: "         << era << '\n'
             << "Condition: "   << condition << '\n'
             << "Location: "    << location << '\n';
    }
};

int findIndex(const Artifact a[], int count, int id)
{
    for (int i = 0; i < count; i++)
        if (a[i].id == id) return i;
    return -1;
}

int main()
{
    Artifact a[MAX_ARTIFACTS];
    int count = 0;
    int choice;

    do
    {
        cout << "\n1. Add Artifact\n"
             << "2. Display All Artifacts\n"
             << "3. Search Artifact\n"
             << "4. Update Location\n"
             << "5. Exit\n";
        choice = readInt("Enter your choice: ");

        if (choice == 1)
        {
            if (count >= MAX_ARTIFACTS)
            {
                cout << "Storage is full!\n";
            }
            else
            {
                int newId = readInt("Enter Artifact ID: ");
                if (findIndex(a, count, newId) != -1)
                {
                    cout << "An artifact with this ID already exists!\n";
                }
                else
                {
                    a[count].id = newId;
                    a[count].inputDetails();
                    count++;
                    cout << "Artifact added successfully!\n";
                }
            }
        }
        else if (choice == 2)
        {
            if (count == 0)
                cout << "No artifacts available.\n";
            else
                for (int i = 0; i < count; i++)
                {
                    a[i].display();
                    cout << "-----\n";
                }
        }
        else if (choice == 3)
        {
            int idx = findIndex(a, count, readInt("Enter Artifact ID to search: "));
            if (idx == -1) cout << "Artifact not found.\n";
            else a[idx].display();
        }
        else if (choice == 4)
        {
            int idx = findIndex(a, count, readInt("Enter Artifact ID: "));
            if (idx == -1)
                cout << "Artifact not found.\n";
            else
            {
                a[idx].location = readLine("Enter new location: ");
                cout << "Location updated successfully!\n";
            }
        }
        else if (choice == 5)
            cout << "Thank you for using Artifact Tracker!\n";
        else
            cout << "Invalid choice!\n";

    } while (choice != 5);

    return 0;
}