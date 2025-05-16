#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <fstream>
#include <sstream>
#include <cctype>

using namespace std;

// === Базовый класс юнита ===
class Unit {
public:
    string name;
    int hp, max_hp, attack, position, cost;
    virtual void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) = 0;
    virtual unique_ptr<Unit> clone() const = 0;
    virtual void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round) {}
    virtual void saveExtra(ofstream& out) const { out << max_hp << ' '; }
    virtual void loadExtra(istringstream& iss) { iss >> max_hp; }
    virtual void updatePositions(vector<unique_ptr<Unit>>& team) {
        for (size_t i = 0; i < team.size(); ++i) {
            team[i]->position = i + 1;
        }
    }
    virtual ~Unit() = default;
};

// === GuliayGorod (Adaptee) ===
class GuliayGorod {
public:
    string name;
    int hp, max_hp, cost;
    GuliayGorod(int pos) {
        name = "GuliayGorod";
        hp = max_hp = 80;
        cost = 25;
        position = pos;
    }
    void boostAllies(vector<unique_ptr<Unit>>& team, const string& teamName, int round) {
        if (round != 1) return; // Only boost in Round 1
        for (auto& unit : team) {
            if (unit->hp > 0 && abs(unit->position - position) == 1) {
                unit->hp += 10;
                unit->max_hp += 10;
                cout << teamName << ": GuliayGorod [" << position << "] boosts "
                     << unit->name << " [" << unit->position << "] HP and max HP to " << unit->hp << ".\n";
            }
        }
    }
    int getPosition() const { return position; }
    void setPosition(int pos) { position = pos; }
private:
    int position;
};

// === Адаптер для GuliayGorod ===
class GuliayGorodAdapter : public Unit {
public:
    GuliayGorodAdapter(int pos) : guliayGorod(pos) {
        name = guliayGorod.name;
        hp = guliayGorod.hp;
        max_hp = guliayGorod.max_hp;
        attack = 0;
        position = pos;
        cost = guliayGorod.cost;
    }
    void attackUnit(Unit*, const string&, const string&) override {}
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round) override {
        guliayGorod.boostAllies(team, teamName, round);
    }
    unique_ptr<Unit> clone() const override {
        auto adapter = make_unique<GuliayGorodAdapter>(position);
        adapter->hp = hp;
        return adapter;
    }
    void saveExtra(ofstream& out) const override {}
    void loadExtra(istringstream& iss) override {}
private:
    GuliayGorod guliayGorod;
};

// === Конкретные классы юнитов ===
class LightInfantry : public Unit {
public:
    LightInfantry(int pos) {
        name = "Light Infantry"; hp = max_hp = 50; attack = 8; position = pos; cost = 10;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam) override {
        if (!target) return;
        int attacks = rand() % 2 + 2;
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
        int attacks = rand() % 5 + 1;
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
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round) override {
        if (rand() % 100 < 10) {
            for (size_t i = 0; i < team.size(); i++) {
                if (team[i]->hp > 0 &&
                    (dynamic_cast<LightInfantry*>(team[i].get()) ||
                     dynamic_cast<Archer*>(team[i].get()))) {
                    string originalName = team[i]->name;
                    auto cloned = team[i]->clone();
                    cloned->position = i + 2;
                    team.insert(team.begin() + i + 1, move(cloned));
                    cout << teamName << ": " << name << " [" << position << "] clones " << originalName << " at position " << i + 2 << "!\n";
                    updatePositions(team);
                    break;
                }
            }
        }
    }
    unique_ptr<Unit> clone() const override { return make_unique<Wizard>(position); }
};

class Healer : public Unit {
public:
    int healing_charges = 5;
    Healer(int pos) {
        name = "Healer"; hp = max_hp = 50; attack = 8; position = pos; cost = 15;
    }
    void attackUnit(Unit*, const string&, const string&) override {}
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round) override {
        if (healing_charges > 0) {
            for (auto& unit : team) {
                if (unit->hp > 0 && unit->hp < 30 &&
                    dynamic_cast<Wizard*>(unit.get()) == nullptr &&
                    dynamic_cast<GuliayGorodAdapter*>(unit.get()) == nullptr) {
                    unit->hp += 5;
                    healing_charges--;
                    cout << teamName << ": " << name << " [" << position << "] heals "
                         << unit->name << " [" << unit->position << "] for 5 HP. Charges left: " << healing_charges << ".\n";
                    break;
                }
            }
        }
    }
    void saveExtra(ofstream& out) const override {
        out << max_hp << ' ' << healing_charges << ' ';
    }
    void loadExtra(istringstream& iss) override {
        iss >> max_hp >> healing_charges;
    }
    unique_ptr<Unit> clone() const override {
        auto healer = make_unique<Healer>(position);
        healer->healing_charges = healing_charges;
        return healer;
    }
};

// === Фабрика юнитов ===
class UnitFactory {
public:
    static unique_ptr<Unit> createUnit(const string& type, int pos) {
        if (type == "LI" || type == "L") return make_unique<LightInfantry>(pos);
        if (type == "HI" || type == "I") return make_unique<HeavyInfantry>(pos);
        if (type == "A") return make_unique<Archer>(pos);
        if (type == "W") return make_unique<Wizard>(pos);
        if (type == "H") return make_unique<Healer>(pos);
        if (type == "Gu" || type == "G") return make_unique<GuliayGorodAdapter>(pos);
        return nullptr;
    }
};

// === Менеджер игры (Singleton) ===
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
            cout << "[" << unit->position << "] " << unit->name << " - " << unit->hp << "/" << unit->max_hp << " HP\n";
        }
    }

    bool isTeamAlive(const vector<unique_ptr<Unit>>& team) {
        return !team.empty();
    }

    void cleanAndShift(vector<unique_ptr<Unit>>& team) {
        team.erase(remove_if(team.begin(), team.end(),
            [](const unique_ptr<Unit>& u) { return u->hp <= 0; }), team.end());
        for (size_t i = 0; i < team.size(); i++) {
            team[i]->position = i + 1;
        }
    }

    void createTeam(vector<unique_ptr<Unit>>& team, const string& teamName, int balance) {
        cout << teamName << " - Starting balance: " << balance << "\n";
        cout << "Units: LI (10), HI (30), A (20), W (30), H (15), Gu (25)\n";
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
        cout << "------------------\n";
    }

    void simulateRound(vector<unique_ptr<Unit>>& t1, vector<unique_ptr<Unit>>& t2,
                       const string& n1, const string& n2, int round) {
        cout << "\nRound " << round << ":\n";
        for (auto& u : t1) {
            if (u->hp <= 0) continue;
            u->specialAbility(t1, n1, round);
            if (t2.empty()) break;
            if (dynamic_cast<Archer*>(u.get())) {
                for (auto& tgt : t2) {
                    if (tgt->hp > 0 && abs(u->position - tgt->position) <= 3) {
                        u->attackUnit(tgt.get(), n1, n2);
                        break;
                    }
                }
            } else if (u->position == 1) {
                for (auto& tgt : t2) {
                    if (tgt->hp > 0 && tgt->position == 1) {
                        u->attackUnit(tgt.get(), n1, n2);
                        break;
                    }
                }
            }
        }

        for (auto& u : t2) {
            if (u->hp <= 0) continue;
            u->specialAbility(t2, n2, round);
            if (t1.empty()) break;
            if (dynamic_cast<Archer*>(u.get())) {
                for (auto& tgt : t1) {
                    if (tgt->hp > 0 && abs(u->position - tgt->position) <= 3) {
                        u->attackUnit(tgt.get(), n2, n1);
                        break;
                    }
                }
            } else if (u->position == 1) {
                for (auto& tgt : t1) {
                    if (tgt->hp > 0 && tgt->position == 1) {
                        u->attackUnit(tgt.get(), n2, n1);
                        break;
                    }
                }
            }
        }
    }
};
GameManager* GameManager::instance = nullptr;

// === Сохранение и загрузка ===
void saveGame(const string& filename, const string& t1, const string& t2, int round,
              const vector<unique_ptr<Unit>>& team1, const vector<unique_ptr<Unit>>& team2) {
    ofstream out(filename);
    if (!out) {
        cout << "Error: Could not save game to " << filename << "\n";
        return;
    }
    out << t1 << '\n' << t2 << '\n' << round << '\n';
    for (const auto& u : team1) {
        out << u->name[0] << ' ' << u->position << ' ' << u->hp << ' ';
        u->saveExtra(out);
        out << '\n';
    }
    out << "---\n";
    for (const auto& u : team2) {
        out << u->name[0] << ' ' << u->position << ' ' << u->hp << ' ';
        u->saveExtra(out);
        out << '\n';
    }
    out.close();
}

void loadGame(const string& filename, string& t1, string& t2, int& round,
              vector<unique_ptr<Unit>>& team1, vector<unique_ptr<Unit>>& team2) {
    ifstream in(filename);
    if (!in) {
        cout << "Error: Could not open file " << filename << "\n";
        return;
    }
    getline(in, t1);
    getline(in, t2);
    string roundStr;
    getline(in, roundStr);
    try {
        round = stoi(roundStr);
    } catch (...) {
        cout << "Error: Invalid round number in save file\n";
        in.close();
        return;
    }

    string line;
    while (getline(in, line) && line != "---") {
        istringstream iss(line);
        string type;
        int pos, hp;
        if (iss >> type >> pos >> hp) {
            auto u = UnitFactory::createUnit(type, pos);
            if (u) {
                u->hp = hp;
                u->loadExtra(iss);
                team1.push_back(move(u));
            } else {
                cout << "Warning: Invalid unit type '" << type << "' in team1\n";
            }
        } else {
            cout << "Warning: Invalid line in team1: " << line << "\n";
        }
    }

    while (getline(in, line)) {
        istringstream iss(line);
        string type;
        int pos, hp;
        if (iss >> type >> pos >> hp) {
            auto u = UnitFactory::createUnit(type, pos);
            if (u) {
                u->hp = hp;
                u->loadExtra(iss);
                team2.push_back(move(u));
            } else {
                cout << "Warning: Invalid unit type '" << type << "' in team2\n";
            }
        } else {
            cout << "Warning: Invalid line in team2: " << line << "\n";
        }
    }

    // Reassign positions to ensure consistency
    for (size_t i = 0; i < team1.size(); i++) {
        team1[i]->position = i + 1;
    }
    for (size_t i = 0; i < team2.size(); i++) {
        team2[i]->position = i + 1;
    }

    in.close();
}

// === Главная функция ===
int main() {
    srand(time(0));
    GameManager* gm = GameManager::getInstance();
    vector<unique_ptr<Unit>> team1, team2;
    string t1, t2, input;
    int round = 1;

    cout << "1. Start New Game\n2. Load Game\nChoice: ";
    int choice;
    if (!(cin >> choice)) {
        cout << "Invalid input. Exiting.\n";
        return 1;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    if (choice == 2) {
        loadGame("save.txt", t1, t2, round, team1, team2);
        if (t1.empty() || t2.empty()) {
            cout << "Failed to load game. Starting new game.\n";
            choice = 1;
        } else {
            srand(time(0));
        }
    }

    if (choice == 1) {
        cout << "Enter Team 1 name: ";
        getline(cin, t1);
        cout << "Enter Team 2 name: ";
        getline(cin, t2);
        gm->createTeam(team1, t1, 100);
        gm->createTeam(team2, t2, 100);
    }

    cout << "Type 'Start' to begin: ";
    cin >> input;
    if (input != "Start") return 0;

    while (gm->isTeamAlive(team1) && gm->isTeamAlive(team2)) {
        gm->simulateRound(team1, team2, t1, t2, round++);
        gm->cleanAndShift(team1);
        gm->cleanAndShift(team2);
        gm->displayTeam(team1, t1);
        gm->displayTeam(team2, t2);
        cout << "Save game? (y/n): ";
        cin >> input;
        if (input == "y") saveGame("save.txt", t1, t2, round, team1, team2);
        cout << "------------------\n";
    }

    cout << "\n" << (gm->isTeamAlive(team1) ? t1 : t2) << " wins!\n";
    return 0;
}