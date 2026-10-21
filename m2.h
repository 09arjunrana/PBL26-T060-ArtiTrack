// m2.h - Artifact class hierarchy
//
//   Artifact                Restorable   (interface / mixin)
//      |  (is-a)              (mixin)
//      |    +--- PotteryArtifact   (derives from Artifact AND Restorable)
//      |    +--- CoinArtifact      (derives from Artifact AND Restorable)
//      |    |        |
//      |    |   AncientCoin        (derives from CoinArtifact)
//      |    +--- Weapon            (derives from Artifact AND Restorable)
//      |    +--- Sculpture         (derives from Artifact AND Restorable)
//      |    +--- Manuscript        (derives from Artifact AND Restorable)
//
//  - single inheritance : every concrete type derives from Artifact
//  - multilevel         : AncientCoin -> CoinArtifact -> Artifact
//  - multiple           : Pottery, Coin, Weapon, Sculpture, Manuscript derive from Artifact AND Restorable
//  - hybrid             : the whole tree combines multilevel + multiple
#pragma once
#include <iostream>
#include <memory>
#include <string>
using namespace std;

// Mixin: "this kind of object can be given conservation advice". It has no data, so
// there is no diamond problem; any class can opt in by deriving from it.
class Restorable {
public:
    virtual ~Restorable() {}
    virtual string restorationAdvice() const = 0;
};
// Base class representing a museum artifact with core tracking attributes
class Artifact {
protected:
    string id, name, currentLocation;
    int estimatedAge;
    double conditionScore;

    // Fields common to every type, in save-file order
    string baseFields() const;

public:
    Artifact(const string& id, const string& name, int age,
             double condition, const string& loc);
    virtual ~Artifact() {}

    const string& getId() const { return id; }
    const string& getName() const { return name; }
    const string& getLocation() const { return currentLocation; }
    Artifact& setLocation(const string& loc);        // returns *this so calls can be chained

    int getAge() const { return estimatedAge; }
    double getCondition() const { return conditionScore; }

    // Type label used by "filter by type"
    virtual string typeName() const { return "Artifact"; }

    // The report table prints one row per artifact (defined in m3.cpp)
    friend void printReportRow(ostream& out, const Artifact& a);

    // Restoration urgency: higher = restore sooner
    virtual double calculatePriority() const;

    virtual void display() const;

    // One line for the save file
    virtual string serialize() const { return "ARTIFACT|" + baseFields(); }
};

// ---- operator overloading: artifacts compare by restoration priority ----
bool operator<(const Artifact& a, const Artifact& b);     // less urgent than
bool operator>(const Artifact& a, const Artifact& b);     // more urgent than
bool operator==(const Artifact& a, const Artifact& b);    // same ID = same artifact
ostream& operator<<(ostream& out, const Artifact& a);     // one-line summary

class PotteryArtifact : public Artifact, public Restorable {
private:
    double porosityIndex;
public:
    PotteryArtifact(const string& id, const string& name, int age,
                    double condition, const string& loc, double porosity);
    double calculatePriority() const override;
    string typeName() const override { return "Pottery"; }
    void display() const override;
    string serialize() const override;
    string restorationAdvice() const override;
};

class CoinArtifact : public Artifact, public Restorable {
protected:
    string metalComposition;   // Gold, Silver, Bronze, Copper
public:
    CoinArtifact(const string& id, const string& name, int age,
                 double condition, const string& loc, const string& metal);
    double calculatePriority() const override;
    string typeName() const override { return "Coin"; }
    void display() const override;
    string serialize() const override;
    string restorationAdvice() const override;
};

class AncientCoin : public CoinArtifact {
private:
    string dynasty;
public:
    AncientCoin(const string& id, const string& name, int age,
                double condition, const string& loc,
                const string& metal, const string& dynasty);
    double calculatePriority() const override;
    string typeName() const override { return "Ancient Coin"; }
    void display() const override;
    string serialize() const override;
    string restorationAdvice() const override;
};

// Weapon: rust is the main threat. Same shape as pottery: a 0..1 index scales a condition penalty.
class Weapon : public Artifact, public Restorable {
private:
    double rustIndex;          // 0 = no rust, 1 = fully corroded
public:
    Weapon(const string& id, const string& name, int age,
           double condition, const string& loc, double rust);
    double calculatePriority() const override;
    string typeName() const override { return "Weapon"; }
    void display() const override;
    string serialize() const override;
    string restorationAdvice() const override;
};

// Sculpture: cracks are the main threat.
class Sculpture : public Artifact, public Restorable {
private:
    double crackIndex;         // 0 = no cracks, 1 = severely cracked
public:
    Sculpture(const string& id, const string& name, int age,
              double condition, const string& loc, double crack);
    double calculatePriority() const override;
    string typeName() const override { return "Sculpture"; }
    void display() const override;
    string serialize() const override;
    string restorationAdvice() const override;
};

// Manuscript: decay (ink, paper, damp) is the main threat.
class Manuscript : public Artifact, public Restorable {
private:
    double decayIndex;         // 0 = intact, 1 = severely decayed
public:
    Manuscript(const string& id, const string& name, int age,
               double condition, const string& loc, double decay);
    double calculatePriority() const override;
    string typeName() const override { return "Manuscript"; }
    void display() const override;
    string serialize() const override;
    string restorationAdvice() const override;
};

// Rebuild an artifact from one save-file line. Returns nullptr if malformed.
unique_ptr<Artifact> parseArtifact(const string& line);
