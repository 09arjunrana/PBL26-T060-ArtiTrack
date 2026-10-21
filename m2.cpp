// Module 2 Implementation - Artifact logic & priority calculations
// m2.cpp - Artifact class hierarchy + save/load support (NO main here)
#include "m2.h"
#include <vector>
using namespace std;

// ---------------- Artifact ----------------
Artifact::Artifact(const string& id, const string& name, int age,
                   double condition, const string& loc)
    : id(id), name(name), currentLocation(loc),
      estimatedAge(age), conditionScore(condition) {}

string Artifact::baseFields() const
{
    return id + "|" + name + "|" + to_string(estimatedAge) + "|" +
           to_string(conditionScore) + "|" + currentLocation;
}

Artifact& Artifact::setLocation(const string& loc)
{
    this->currentLocation = loc;
    return *this;
}

// Base urgency: condition dominates (0..40), age adds 1.5 points per century
double Artifact::calculatePriority() const
{
    return (10.0 - conditionScore) * 4.0 + (estimatedAge / 100.0) * 1.5;
}

void Artifact::display() const
{
    cout << "ID: " << id << " | Name: " << name
         << " | Age: " << estimatedAge << " yrs"
         << " | Condition: " << conditionScore
         << " | Location: " << currentLocation << "\n";
}

// ---------------- operators ----------------
bool operator<(const Artifact& a, const Artifact& b)  { return a.calculatePriority() < b.calculatePriority(); }
bool operator>(const Artifact& a, const Artifact& b)  { return b < a; }
bool operator==(const Artifact& a, const Artifact& b) { return a.getId() == b.getId(); }

ostream& operator<<(ostream& out, const Artifact& a)
{
    out << a.getId() << " (" << a.typeName() << ", " << a.getName() << ") at " << a.getLocation();
    return out;
}
// Calculate restoration urgency weighting condition decay against porosity
// ---------------- PotteryArtifact ----------------
PotteryArtifact::PotteryArtifact(const string& id, const string& name, int age,
                                 double condition, const string& loc, double porosity)
    : Artifact(id, name, age, condition, loc), porosityIndex(porosity) {}

double PotteryArtifact::calculatePriority() const
{
    double baseScore = Artifact::calculatePriority();
    double porosityPenalty = (10.0 - conditionScore) * porosityIndex * 5.0;
    return baseScore + porosityPenalty;
}

void PotteryArtifact::display() const
{
    Artifact::display();
    cout << "  Type: Pottery | Porosity: " << porosityIndex << "\n";
}

string PotteryArtifact::serialize() const
{
    return "POTTERY|" + baseFields() + "|" + to_string(porosityIndex);
}

string PotteryArtifact::restorationAdvice() const
{
    if (porosityIndex >= 0.5)
        return "Porous body: keep humidity steady, consolidate before handling, no wet cleaning.";
    return "Dense body: dry-clean with a soft brush and check for salt crystals.";
}

// ---------------- CoinArtifact ----------------
CoinArtifact::CoinArtifact(const string& id, const string& name, int age,
                           double condition, const string& loc, const string& metal)
    : Artifact(id, name, age, condition, loc), metalComposition(metal) {}

double CoinArtifact::calculatePriority() const
{
    double baseScore = Artifact::calculatePriority();
    // Oxidizable metals need urgent attention
    if (metalComposition == "Bronze" || metalComposition == "Copper")
        baseScore += 12.0;
    return baseScore;
}

void CoinArtifact::display() const
{
    Artifact::display();
    cout << "  Type: Coin | Metal: " << metalComposition << "\n";
}

string CoinArtifact::serialize() const
{
    return "COIN|" + baseFields() + "|" + metalComposition;
}

string CoinArtifact::restorationAdvice() const
{
    if (metalComposition == "Bronze" || metalComposition == "Copper")
        return "Check for bronze disease (green powdery spots); store dry, away from PVC holders.";
    if (metalComposition == "Silver")
        return "Use anti-tarnish storage; never polish.";
    return "Stable metal: handle by the edges and keep in an inert holder.";
}

// ---------------- AncientCoin ----------------
AncientCoin::AncientCoin(const string& id, const string& name, int age,
                         double condition, const string& loc,
                         const string& metal, const string& dynasty)
    : CoinArtifact(id, name, age, condition, loc, metal), dynasty(dynasty) {}

double AncientCoin::calculatePriority() const
{
    double coinScore = CoinArtifact::calculatePriority();
    // Extra urgency for high-rarity dynasties
    coinScore += (dynasty == "Maurya" || dynasty == "Gupta") ? 20.0 : 10.0;
    return coinScore;
}

void AncientCoin::display() const
{
    CoinArtifact::display();
    cout << "  Subtype: Ancient Coin | Dynasty: " << dynasty << "\n";
}

string AncientCoin::serialize() const
{
    return "ANCIENT|" + baseFields() + "|" + metalComposition + "|" + dynasty;
}

string AncientCoin::restorationAdvice() const
{
    return CoinArtifact::restorationAdvice() + " Document it fully before any treatment.";
}

// ---------------- Weapon ----------------
Weapon::Weapon(const string& id, const string& name, int age,
               double condition, const string& loc, double rust)
    : Artifact(id, name, age, condition, loc), rustIndex(rust) {}

double Weapon::calculatePriority() const
{
    double baseScore = Artifact::calculatePriority();
    double penalty = (10.0 - conditionScore) * rustIndex * 5.0;
    return baseScore + penalty;
}

void Weapon::display() const
{
    Artifact::display();
    cout << "  Type: Weapon | Rust: " << rustIndex << "\n";
}

string Weapon::serialize() const
{
    return "WEAPON|" + baseFields() + "|" + to_string(rustIndex);
}

string Weapon::restorationAdvice() const
{
    if (rustIndex >= 0.5)
        return "Heavy rust: keep it dry, stabilise the corrosion before any cleaning, never polish.";
    return "Light rust: dry-clean, apply a thin protective wax and re-check for active corrosion.";
}

// ---------------- Sculpture ----------------
Sculpture::Sculpture(const string& id, const string& name, int age,
                     double condition, const string& loc, double crack)
    : Artifact(id, name, age, condition, loc), crackIndex(crack) {}

double Sculpture::calculatePriority() const
{
    double baseScore = Artifact::calculatePriority();
    double penalty = (10.0 - conditionScore) * crackIndex * 5.0;
    return baseScore + penalty;
}

void Sculpture::display() const
{
    Artifact::display();
    cout << "  Type: Sculpture | Crack Index: " << crackIndex << "\n";
}

string Sculpture::serialize() const
{
    return "SCULPTURE|" + baseFields() + "|" + to_string(crackIndex);
}

string Sculpture::restorationAdvice() const
{
    if (crackIndex >= 0.5)
        return "Cracked: support it before moving, never lift by projecting parts, consolidate cracks first.";
    return "Stable: dust gently and keep monitoring the existing cracks.";
}

// ---------------- Manuscript ----------------
Manuscript::Manuscript(const string& id, const string& name, int age,
                       double condition, const string& loc, double decay)
    : Artifact(id, name, age, condition, loc), decayIndex(decay) {}

double Manuscript::calculatePriority() const
{
    double baseScore = Artifact::calculatePriority();
    double penalty = (10.0 - conditionScore) * decayIndex * 5.0;
    return baseScore + penalty;
}

void Manuscript::display() const
{
    Artifact::display();
    cout << "  Type: Manuscript | Decay Index: " << decayIndex << "\n";
}

string Manuscript::serialize() const
{
    return "MANUSCRIPT|" + baseFields() + "|" + to_string(decayIndex);
}

string Manuscript::restorationAdvice() const
{
    if (decayIndex >= 0.5)
        return "Advanced decay: handle only on flat supports, limit light and damp, digitise before further handling.";
    return "Stable: keep it in an acid-free enclosure, away from light and damp.";
}

// ---------------- parsing ----------------
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
        if (type == "WEAPON" && f.size() == 7)
            return make_unique<Weapon>(f[1], f[2], age, cond, f[5], stod(f[6]));
        if (type == "SCULPTURE" && f.size() == 7)
            return make_unique<Sculpture>(f[1], f[2], age, cond, f[5], stod(f[6]));
        if (type == "MANUSCRIPT" && f.size() == 7)
            return make_unique<Manuscript>(f[1], f[2], age, cond, f[5], stod(f[6]));
        if (type == "ARTIFACT" && f.size() == 6)
            return make_unique<Artifact>(f[1], f[2], age, cond, f[5]);
    }
    catch (...) {}
    return nullptr;
}
