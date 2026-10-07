#include <iostream>
#include <string>
using namespace std;

class Artifact {
protected:
    string id, name, currentLocation;
    int estimatedAge;
    double conditionScore;
public:
    Artifact(const string& id, const string& name, int age,
             double condition, const string& loc)
        : id(id), name(name), currentLocation(loc),
          estimatedAge(age), conditionScore(condition) {}

    virtual ~Artifact() {}

    virtual double calculatePriority() const {
        return (10.0 - conditionScore) * 4.0 + (estimatedAge / 100.0) * 1.5;
    }

    virtual void display() const {
        cout << "ID: " << id << " | Name: " << name
             << " | Age: " << estimatedAge << " yrs"
             << " | Condition: " << conditionScore
             << " | Location: " << currentLocation << "\n";
    }
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
};

// Demo: polymorphic use through base-class pointers
int main() {
    const int N = 3;
    Artifact* items[N] = {
        new PotteryArtifact("P001", "Clay Urn", 2500, 4.5, "Vault A", 0.6),
        new CoinArtifact("C001", "Silver Dirham", 800, 7.0, "Vault B", "Silver"),
        new AncientCoin("C002", "Punch-marked Coin", 2300, 5.0, "Vault B", "Copper", "Maurya")
    };

    for (int i = 0; i < N; i++) {
        items[i]->display();
        cout << "  Priority: " << items[i]->calculatePriority() << "\n\n";
    }

    for (int i = 0; i < N; i++) delete items[i]; // virtual destructor makes this safe
    return 0;
}