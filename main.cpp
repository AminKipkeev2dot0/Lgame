#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <memory>

using namespace std;

// Base Unit class
class Unit {
public:
    string name;
    int hp, max_hp, attack, position, cost;
    virtual void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) = 0;
    virtual unique_ptr<Unit> clone() const = 0; // Prototype pattern
    virtual void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName) {}
    virtual ~Unit() = default;
};

// Concrete Unit Classes
class LightInfantry : public Unit {
public:
    LightInfantry(int pos) {
        name = "Light Infantry"; hp = max_hp = 50; attack = 8; position = pos; cost = 10;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) override {
        if (!target) return;
        int attacks = rand() % 2 + 2; // 2 or 3 attacks
        for (int i = 0; i < attacks && target->hp > 0; i++) {
            target->hp -= attack;
            cout << attackerTeam << ": " << name << " [" << position << "] attacks "
                 << targetTeam << ": " << target->name << " [" << target->position << "] and deals " << attack << " damage.\n";
        }
    }
    unique_ptr<Unit> clone() const override { return make_unique<LightInfantry>(position); }
};

class HeavyInfantry : public Unit {
public:
    HeavyInfantry(int pos) {
        name = "Heavy Infantry"; hp = max_hp = 100; attack = 20; position = pos; cost = 30;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) override {
        if (!target) return;
        target->hp -= attack;
        cout << attackerTeam << ": " << name << " [" << position << "] attacks "
             << targetTeam << ": " << target->name << " [" << target->position << "] and deals " << attack << " damage.\n";
    }
    unique_ptr<Unit> clone() const override { return make_unique<HeavyInfantry>(position); }
};

class Archer : public Unit {
public:
    Archer(int pos) {
        name = "Archer"; hp = max_hp = 40; attack = 7; position = pos; cost = 20;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) override {
        if (!target) return;
        int attacks = rand() % 5 + 1; // 1 to 5 attacks
        for (int i = 0; i < attacks && target->hp > 0; i++) {
            target->hp -= attack;
            cout << attackerTeam << ": " << name << " [" << position << "] attacks "
                 << targetTeam << ": " << target->name << " [" << target->position << "] and deals " << attack << " damage.\n";
        }
    }
    unique_ptr<Unit> clone() const override { return make_unique<Archer>(position); }
};

class Wizard : public Unit {
public:
    Wizard(int pos) {
        name = "Wizard"; hp = max_hp = 30; attack = 5; position = pos; cost = 30;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) override {
        if (!target) return;
        target->hp -= attack;
        cout << attackerTeam << ": " << name << " [" << position << "] attacks "
             << targetTeam << ": " << target->name << " [" << target->position << "] and deals " << attack << " damage.\n";
    }
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName) override {
        if (rand() % 100 < 45) { // 45% chance to clone
            for (size_t i = 0; i < team.size(); i++) {
                if (team[i]->hp > 0 && (dynamic_cast<LightInfantry*>(team[i].get()) || dynamic_cast<Archer*>(team[i].get()))) {
                    auto cloned = team[i]->clone();
                    cloned->position = team[i]->position; // Clone takes same position initially
                    team.insert(team.begin() + i, move(cloned));
                    cout << teamName << ": " << name << " [" << position << "] clones " << team[i]->name << "!\n";
                    break;
                }
            }
            // Reassign positions after insertion
            for (size_t j = 0; j < team.size(); j++) {
                team[j]->position = j + 1;
            }
        }
    }
    unique_ptr<Unit> clone() const override { return make_unique<Wizard>(position); }
};

class Healer : public Unit {
    int healing_charges = 3;
public:
    Healer(int pos) {
        name = "Healer"; hp = max_hp = 50; attack = 8; position = pos; cost = 15;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) override {} // Healer doesn't attack
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName) override {
        if (healing_charges > 0) {
            for (auto& unit : team) {
                if (unit->hp > 0 && unit->hp < 30 && dynamic_cast<Wizard*>(unit.get()) == nullptr) {
                    unit->hp += 20;
                    healing_charges--;
                    cout << teamName << ": " << name << " [" << position << "] heals "
                         << unit->name << " [" << unit->position << "] for 20 HP.\n";
                    break;
                }
            }
        }
    }
    unique_ptr<Unit> clone() const override { return make_unique<Healer>(position); }
};

// Factory Method pattern
class UnitFactory {
public:
    static unique_ptr<Unit> createUnit(const string& type, int pos) {
        if (type == "LI") return make_unique<LightInfantry>(pos);
        if (type == "HI") return make_unique<HeavyInfantry>(pos);
        if (type == "A") return make_unique<Archer>(pos);
        if (type == "W") return make_unique<Wizard>(pos);
        if (type == "H") return make_unique<Healer>(pos);
        return nullptr;
    }
};

// Singleton for Game Manager
class GameManager {
    static GameManager* instance;
    GameManager() {}
public:
    static GameManager* getInstance() {
        if (!instance) instance = new GameManager();
        return instance;
    }
    void displayTeam(const vector<unique_ptr<Unit>>& team, const string& teamName) {
        cout << teamName << ":\n";
        if (team.empty()) {
            cout << "No units remaining.\n";
            return;
        }
        for (const auto& unit : team) {
            cout << "[" << unit->position << "] " << unit->name << " - " << unit->hp << " HP\n";
        }
    }
    bool isTeamAlive(const vector<unique_ptr<Unit>>& team) {
        return !team.empty();
    }
    void cleanAndShift(vector<unique_ptr<Unit>>& team) {
        // Remove dead units (HP <= 0)
        for (auto it = team.begin(); it != team.end();) {
            if ((*it)->hp <= 0) {
                it = team.erase(it);
            } else {
                ++it;
            }
        }
        // Shift positions
        for (size_t i = 0; i < team.size(); i++) {
            team[i]->position = i + 1;
        }
    }
};
GameManager* GameManager::instance = nullptr;

// Game logic
void createTeam(vector<unique_ptr<Unit>>& team, const string& teamName, int balance) {
    cout << teamName << " - Starting balance: " << balance << "\n";
    cout << "Units: LI (10), HI (30), A (20), W (30), H (15)\n";
    int pos = 1;
    while (balance > 0) {
        string type;
        cout << "Enter unit type (or 'done' to finish): ";
        cin >> type;
        if (type == "done") break;
        auto unit = UnitFactory::createUnit(type, pos++);
        if (unit && balance >= unit->cost) {
            balance -= unit->cost;
            team.push_back(move(unit));
            cout << "Added " << team.back()->name << ". Remaining balance: " << balance << "\n";
        } else {
            cout << "Invalid unit or insufficient balance.\n";
        }
    }
}

void simulateRound(vector<unique_ptr<Unit>>& team1, vector<unique_ptr<Unit>>& team2, const string& team1Name, const string& team2Name, int round) {
    cout << "\nRound " << round << ":\n";
    // Team 1 actions
    for (size_t i = 0; i < team1.size(); i++) {
        if (team1[i]->hp <= 0) continue;
        team1[i]->specialAbility(team1, team1Name);
        if (team2.empty()) break; // Stop if no targets remain
        if (dynamic_cast<Archer*>(team1[i].get())) {
            for (auto& target : team2) {
                if (target->hp > 0 && abs(team1[i]->position - target->position) <= 3) {
                    team1[i]->attackUnit(target.get(), team1Name, team2Name);
                    break;
                }
            }
        } else if (team1[i]->position == 1) {
            for (auto& target : team2) {
                if (target->hp > 0 && target->position == 1) {
                    team1[i]->attackUnit(target.get(), team1Name, team2Name);
                    break;
                }
            }
        }
    }
    // Team 2 actions
    for (size_t i = 0; i < team2.size(); i++) {
        if (team2[i]->hp <= 0) continue;
        team2[i]->specialAbility(team2, team2Name);
        if (team1.empty()) break; // Stop if no targets remain
        if (dynamic_cast<Archer*>(team2[i].get())) {
            for (auto& target : team1) {
                if (target->hp > 0 && abs(team2[i]->position - target->position) <= 3) {
                    team2[i]->attackUnit(target.get(), team2Name, team1Name);
                    break;
                }
            }
        } else if (team2[i]->position == 1) {
            for (auto& target : team1) {
                if (target->hp > 0 && target->position == 1) {
                    team2[i]->attackUnit(target.get(), team2Name, team1Name);
                    break;
                }
            }
        }
    }
}

int main() {
    srand(time(0));
    GameManager* gm = GameManager::getInstance();
    string input;
    cout << "Create game? (Yes/No): ";
    cin >> input;
    if (input != "Yes") return 0;

    vector<unique_ptr<Unit>> team1, team2;
    string team1Name, team2Name;
    cout << "Enter Team 1 name: ";
    cin >> team1Name;
    cout << "Enter Team 2 name: ";
    cin >> team2Name;

    createTeam(team1, team1Name, 150);
    createTeam(team2, team2Name, 150);

    gm->displayTeam(team1, team1Name);
    gm->displayTeam(team2, team2Name);

    cout << "Press 'Start' to begin: ";
    cin >> input;
    if (input != "Start") return 0;

    int round = 1;
    while (gm->isTeamAlive(team1) && gm->isTeamAlive(team2)) {
        simulateRound(team1, team2, team1Name, team2Name, round++);
        gm->cleanAndShift(team1);
        gm->cleanAndShift(team2);
        gm->displayTeam(team1, team1Name);
        gm->displayTeam(team2, team2Name);
    }

    cout << (gm->isTeamAlive(team1) ? team1Name : team2Name) << " wins!\n";
    return 0;
}