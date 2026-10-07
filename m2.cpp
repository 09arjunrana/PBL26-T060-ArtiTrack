// m2.cpp - Artifact class hierarchy + save/load support (NO main here; used by m1.cpp)
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
using namespace std;

class Artifact {
protected:
    string id, name, currentLocation;
    int estimatedAge;
    double conditionScore;

    // Fields common to every type, in save-file order
    string baseFields() const {
        return id + "|" + name + "|" + to_string(estimatedAge) + "|" +
               to_string(conditionScore) + "|" + currentLocation;
    }
public:
    Artifact(const string& id, const string& name, int age,
             double condition, const string& loc)
        : id(id), name(name), currentLocation(loc),
          estimatedAge(age), conditionScore(condition) {}

    virtual ~Artifact() {}

    const string& getId() const { return id; }

    virtual double calculatePriority() const {
        return (10.0 - conditionScore) * 4.0 + (estimatedAge / 100.0) * 1.5;
    }

    virtual void display() const {
        cout << "ID: " << id << " | Name: " << name
             << " | Age: " << estimatedAge << " yrs"
             << " | Condition: " << conditionScore
             << " | Location: " << currentLocation << "\n";
    }

    // One line for the save file
    virtual string serialize() const { return "ARTIFACT|" + baseFields(); }
};

class PotteryArtifact : public Artifact {
private:
    double porosityIndex;
public:
    PotteryArtifact(const string& id, const string& name, int age,
                    double condition, const string& loc, double porosity)
        : Artifact(id, name, age, condition, loc),
          porosityIndex(porosity) {}

    double calculatePriority() const override {
        double baseScore = Artifact::calculatePriority();
        double porosityPenalty = (10.0 - conditionScore) * porosityIndex * 5.0;
        return baseScore + porosityPenalty;
    }

    void display() const override {
        Artifact::display();
        cout << "  Type: Pottery | Porosity: " << porosityIndex << "\n";
    }

    string serialize() const override {
        return "POTTERY|" + baseFields() + "|" + to_string(porosityIndex);
    }
};

class CoinArtifact : public Artifact {
protected:
    string metalComposition; // Gold, Silver, Bronze, Copper
public:
    CoinArtifact(const string& id, const string& name, int age,
                 double condition, const string& loc, const string& metal)
        : Artifact(id, name, age, condition, loc), metalComposition(metal) {}

    double calculatePriority() const override {
        double baseScore = Artifact::calculatePriority();
        // Oxidizable metals need urgent attention
        if (metalComposition == "Bronze" || metalComposition == "Copper")
            baseScore += 12.0;
        return baseScore;
    }

    void display() const override {
        Artifact::display();
        cout << "  Type: Coin | Metal: " << metalComposition << "\n";
    }

    string serialize() const override {
        return "COIN|" + baseFields() + "|" + metalComposition;
    }
};

class AncientCoin : public CoinArtifact {
private:
    string dynasty;
public:
    AncientCoin(const string& id, const string& name, int age,
                double condition, const string& loc,
                const string& metal, const string& dynasty)
        : CoinArtifact(id, name, age, condition, loc, metal),
          dynasty(dynasty) {}

    double calculatePriority() const override {
        double coinScore = CoinArtifact::calculatePriority();
        // Extra urgency for high-rarity dynasties
        coinScore += (dynasty == "Maurya" || dynasty == "Gupta") ? 20.0 : 10.0;
        return coinScore;
    }

    void display() const override {
        CoinArtifact::display();
        cout << "  Subtype: Ancient Coin | Dynasty: " << dynasty << "\n";
    }

    string serialize() const override {
        return "ANCIENT|" + baseFields() + "|" + metalComposition + "|" + dynasty;
    }
};

// Rebuild an artifact from one save-file line. Returns nullptr if malformed.
unique_ptr<Artifact> parseArtifact(const string& line)
{
    vector<string> f;
    string cur;
    for (char c : line)
    {
        if (c == '|') { f.push_back(cur); cur.clear(); }
        else cur += c;
    }
    f.push_back(cur);

    if (f.size() < 6) return nullptr;
    try
    {
        const string& type = f[0];
        int age = stoi(f[3]);
        double cond = stod(f[4]);

        if (type == "POTTERY" && f.size() == 7)
            return make_unique<PotteryArtifact>(f[1], f[2], age, cond, f[5], stod(f[6]));
        if (type == "COIN" && f.size() == 7)
            return make_unique<CoinArtifact>(f[1], f[2], age, cond, f[5], f[6]);
        if (type == "ANCIENT" && f.size() == 8)
            return make_unique<AncientCoin>(f[1], f[2], age, cond, f[5], f[6], f[7]);
        if (type == "ARTIFACT" && f.size() == 6)
            return make_unique<Artifact>(f[1], f[2], age, cond, f[5]);
    }
    catch (...) {}
    return nullptr;
}