#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <fstream>
#include <sstream>
#include <cctype>
#include <map>
#include <chrono>
#include <algorithm>
#include <random>
#include <stack>

using namespace std;

// === Logger Interface ===
class Logger {
public:
    virtual void log(const string& message, const string& level = "INFO") = 0;
    virtual ~Logger() = default;
};

// === Real Logger (Console) ===
class ConsoleLogger : public Logger {
public:
    void log(const string& message, const string& level) override {
        cout << message << "\n";
    }
};

// === Logger Proxy ===
class LoggerProxy : public Logger {
public:
    LoggerProxy(const string& filename) : file_logger(filename.c_str(), ios::app) {
        real_logger = make_unique<ConsoleLogger>();
        if (!file_logger.is_open()) {
            real_logger->log("Failed to open log file: " + filename, "ERROR");
        } else {
            real_logger->log("Successfully opened log file: " + filename, "DEBUG");
        }
    }
    void log(const string& message, const string& level) override {
        real_logger->log(message, level); // Log to console
        if (file_logger.is_open()) {
            auto now = chrono::system_clock::now();
            auto time = chrono::system_clock::to_time_t(now);
            string timestamp = ctime(&time);
            timestamp.pop_back();
            file_logger << "[" << timestamp << "] [" << level << "] " << message << "\n";
            file_logger.flush(); // Ensure immediate write
        }
    }
    ~LoggerProxy() {
        if (file_logger.is_open()) {
            file_logger.close();
        }
    }
private:
    unique_ptr<Logger> real_logger;
    ofstream file_logger;
};

// === Buff Structure ===
struct Buff {
    string name;
    int hp_boost, attack_boost, extra_attacks, armor, cost, damage_threshold;
};

const map<string, Buff> BUFFS = {
    {"Ho", {"Horse", 5, 0, 2, 0, 5, 15}},
    {"Sp", {"Spear", 0, 5, 0, 0, 3, 10}},
    {"Sh", {"Shield", 0, 0, 0, 10, 4, 20}},
    {"He", {"Helmet", 5, 0, 0, 0, 2, 25}}
};

// === Базовый класс юнита ===
class Unit {
public:
    string name;
    int hp, max_hp, attack, position, cost;
    virtual void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam, Logger& logger) = 0;
    virtual void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round, Logger& logger) {}
    virtual unique_ptr<Unit> clone() const = 0;
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
    void boostAllies(vector<unique_ptr<Unit>>& team, const string& teamName, int round, Logger& logger) {
        if (round != 1) return;
        for (auto& unit : team) {
            if (unit->hp > 0 && abs(unit->position - position) == 1) {
                unit->hp += 10;
                unit->max_hp += 10;
                logger.log(teamName + ": GuliayGorod [" + to_string(position) + "] boosts " +
                           unit->name + " [" + to_string(unit->position) + "] HP and max HP to " + to_string(unit->hp) + ".", "INFO");
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
    void attackUnit(Unit*, const string&, const string&, Logger&) override {}
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round, Logger& logger) override {
        guliayGorod.boostAllies(team, teamName, round, logger);
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
    vector<string> active_buffs;
    int total_damage_taken = 0;
    int armor = 0;
    LightInfantry(int pos, const vector<string>& buffs = {}) {
        name = "Light Infantry";
        hp = max_hp = 50;
        attack = 8;
        position = pos;
        cost = 10;
        active_buffs = buffs;
        applyBuffs();
    }
    void applyBuffs() {
        double hp_ratio = max_hp > 0 ? static_cast<double>(hp) / max_hp : 1.0;
        max_hp = 50;
        attack = 8;
        armor = 0;
        for (const auto& buff : active_buffs) {
            auto it = BUFFS.find(buff);
            if (it != BUFFS.end()) {
                max_hp += it->second.hp_boost;
                attack += it->second.attack_boost;
                armor += it->second.armor;
            }
        }
        hp = static_cast<int>(max_hp * hp_ratio);
        if (hp < 0) hp = 0;
    }
    void applyDamage(int damage, Logger& logger) {
        int reduced_damage = max(0, damage - armor);
        logger.log(name + " [" + to_string(position) + "] takes " + to_string(reduced_damage) +
                   " damage (raw: " + to_string(damage) + ", armor: " + to_string(armor) +
                   "). HP before: " + to_string(hp), "INFO");
        hp -= reduced_damage;
        if (reduced_damage > 0) {
            total_damage_taken += reduced_damage;
            checkBuffLoss(logger);
        }
        if (hp < 0) hp = 0;
        logger.log(name + " [" + to_string(position) + "] HP after: " + to_string(hp), "INFO");
    }
    void checkBuffLoss(Logger& logger) {
        vector<string> remaining_buffs;
        for (const auto& buff : active_buffs) {
            auto it = BUFFS.find(buff);
            if (it != BUFFS.end() && total_damage_taken <= it->second.damage_threshold) {
                remaining_buffs.push_back(buff);
            } else if (it != BUFFS.end()) {
                logger.log(name + " [" + to_string(position) + "] loses " + it->second.name +
                           " due to " + to_string(total_damage_taken) + " damage taken.", "INFO");
                if (it->second.hp_boost > 0) {
                    max_hp -= it->second.hp_boost;
                    hp = min(hp, max_hp);
                }
            }
        }
        active_buffs = remaining_buffs;
        applyBuffs();
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam, Logger& logger) override {
        if (!target) return;
        int attacks = rand() % 2 + (hasBuff("Ho") ? 4 : 2);
        for (int i = 0; i < attacks && target->hp > 0; i++) {
            logger.log(attackerTeam + ": " + name + " [" + to_string(position) + "] attacks " +
                       targetTeam + ": " + target->name + " [" + to_string(target->position) + "] and deals " + to_string(attack) + " damage.", "INFO");
            if (auto li = dynamic_cast<LightInfantry*>(target)) {
                li->applyDamage(attack, logger);
            } else {
                target->hp -= attack;
            }
        }
    }
    bool hasBuff(const string& buff_code) const {
        return find(active_buffs.begin(), active_buffs.end(), buff_code) != active_buffs.end();
    }
    unique_ptr<Unit> clone() const override {
        auto li = make_unique<LightInfantry>(position, active_buffs);
        li->hp = hp;
        li->max_hp = max_hp;
        li->total_damage_taken = total_damage_taken;
        li->applyBuffs();
        return li;
    }
    void saveExtra(ofstream& out) const override {
        out << max_hp << ' ' << total_damage_taken << ' ';
        for (const auto& buff : active_buffs) out << buff;
        out << ' ';
    }
    void loadExtra(istringstream& iss) override {
        iss >> max_hp >> total_damage_taken;
        string buff_str;
        iss >> buff_str;
        active_buffs.clear();
        for (size_t i = 0; i < buff_str.size(); i += 2) {
            string buff_code = buff_str.substr(i, 2);
            if (BUFFS.find(buff_code) != BUFFS.end()) {
                active_buffs.push_back(buff_code);
            }
        }
        applyBuffs();
    }
};

class HeavyInfantry : public Unit {
public:
    HeavyInfantry(int pos) {
        name = "Heavy Infantry"; hp = max_hp = 100; attack = 20; position = pos; cost = 30;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam, Logger& logger) override {
        if (!target) return;
        logger.log(attackerTeam + ": " + name + " [" + to_string(position) + "] attacks " +
                   targetTeam + ": " + target->name + " [" + to_string(target->position) + "] and deals " + to_string(attack) + " damage.", "INFO");
        if (auto li = dynamic_cast<LightInfantry*>(target)) {
            li->applyDamage(attack, logger);
        } else {
            target->hp -= attack;
        }
    }
    unique_ptr<Unit> clone() const override { return make_unique<HeavyInfantry>(position); }
};

class Archer : public Unit {
public:
    Archer(int pos) {
        name = "Archer"; hp = max_hp = 40; attack = 7; position = pos; cost = 20;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam, Logger& logger) override {
        if (!target) return;
        int attacks = rand() % 5 + 1;
        for (int i = 0; i < attacks && target->hp > 0; i++) {
            logger.log(attackerTeam + ": " + name + " [" + to_string(position) + "] attacks " +
                       targetTeam + ": " + target->name + " [" + to_string(target->position) + "] and deals " + to_string(attack) + " damage.", "INFO");
            if (auto li = dynamic_cast<LightInfantry*>(target)) {
                li->applyDamage(attack, logger);
            } else {
                target->hp -= attack;
            }
        }
    }
    unique_ptr<Unit> clone() const override { return make_unique<Archer>(position); }
};

class Wizard : public Unit {
public:
    Wizard(int pos) {
        name = "Wizard"; hp = max_hp = 30; attack = 5; position = pos; cost = 30;
    }
    void attackUnit(Unit* target, const string& attackerTeam, const string& targetTeam, Logger& logger) override {
        if (!target) return;
        logger.log(attackerTeam + ": " + name + " [" + to_string(position) + "] attacks " +
                   targetTeam + ": " + target->name + " [" + to_string(target->position) + "] and deals " + to_string(attack) + " damage.", "INFO");
        if (auto li = dynamic_cast<LightInfantry*>(target)) {
            li->applyDamage(attack, logger);
        } else {
            target->hp -= attack;
        }
    }
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round, Logger& logger) override {
        if (rand() % 100 < 10) {
            for (size_t i = 0; i < team.size(); i++) {
                if (team[i]->hp > 0 &&
                    (dynamic_cast<LightInfantry*>(team[i].get()) ||
                     dynamic_cast<Archer*>(team[i].get()))) {
                    string originalName = team[i]->name;
                    auto cloned = team[i]->clone();
                    cloned->position = i + 2;
                    team.insert(team.begin() + i + 1, std::move(cloned));
                    logger.log(teamName + ": " + name + " [" + to_string(position) + "] clones " + originalName + " at position " + to_string(i + 2) + "!", "INFO");
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
    void attackUnit(Unit*, const string&, const string&, Logger&) override {}
    void specialAbility(vector<unique_ptr<Unit>>& team, const string& teamName, int round, Logger& logger) override {
        if (healing_charges > 0) {
            for (auto& unit : team) {
                if (unit->hp > 0 && unit->hp < 30 &&
                    dynamic_cast<Wizard*>(unit.get()) == nullptr &&
                    dynamic_cast<GuliayGorodAdapter*>(unit.get()) == nullptr) {
                    unit->hp += 5;
                    healing_charges--;
                    logger.log(teamName + ": " + name + " [" + to_string(position) + "] heals " +
                               unit->name + " [" + to_string(unit->position) + "] for 5 HP. Charges left: " + to_string(healing_charges) + ".", "INFO");
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

// === Factory Method Pattern ===
class UnitFactory {
public:
    virtual unique_ptr<Unit> createUnit(const string& type, int pos, Logger& logger) = 0;
    virtual void createTeam(vector<unique_ptr<Unit>>& team, const string& teamName, int balance, Logger& logger) = 0;
    virtual ~UnitFactory() = default;
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

    void displayTeam(const vector<unique_ptr<Unit>>& team, const string& teamName, Logger& logger) {
        logger.log(teamName + ":", "INFO");
        if (team.empty()) {
            logger.log("No units remaining.", "INFO");
            return;
        }
        for (const auto& unit : team) {
            string unit_info = "[" + to_string(unit->position) + "] " + unit->name + " - " +
                               to_string(unit->hp) + "/" + to_string(unit->max_hp) + " HP";
            if (auto li = dynamic_cast<LightInfantry*>(unit.get())) {
                if (!li->active_buffs.empty()) {
                    unit_info += " (Buffs: ";
                    for (size_t i = 0; i < li->active_buffs.size(); ++i) {
                        auto it = BUFFS.find(li->active_buffs[i]);
                        unit_info += (it != BUFFS.end() ? it->second.name : li->active_buffs[i]);
                        if (i < li->active_buffs.size() - 1) unit_info += ", ";
                    }
                    unit_info += ")";
                }
            }
            logger.log(unit_info, "INFO");
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

    void createTeam(vector<unique_ptr<Unit>>& team, const string& teamName, int balance, UnitFactory& factory, Logger& logger) {
        factory.createTeam(team, teamName, balance, logger);
        displayTeam(team, teamName, logger);
    }

    void simulateRound(vector<unique_ptr<Unit>>& t1, vector<unique_ptr<Unit>>& t2,
                       const string& n1, const string& n2, int round, Logger& logger) {
        logger.log("\nRound " + to_string(round) + ":", "INFO");
        for (auto& u : t1) {
            if (u->hp <= 0) continue;
            u->specialAbility(t1, n1, round, logger);
            if (t2.empty()) break;
            if (dynamic_cast<Archer*>(u.get())) {
                for (auto& tgt : t2) {
                    if (tgt->hp > 0 && abs(u->position - tgt->position) <= 3) {
                        u->attackUnit(tgt.get(), n1, n2, logger);
                        break;
                    }
                }
            } else if (u->position == 1) {
                for (auto& tgt : t2) {
                    if (tgt->hp > 0 && tgt->position == 1) {
                        u->attackUnit(tgt.get(), n1, n2, logger);
                        break;
                    }
                }
            }
        }

        for (auto& u : t2) {
            if (u->hp <= 0) continue;
            u->specialAbility(t2, n2, round, logger);
            if (t1.empty()) break;
            if (dynamic_cast<Archer*>(u.get())) {
                for (auto& tgt : t1) {
                    if (tgt->hp > 0 && abs(u->position - tgt->position) <= 3) {
                        u->attackUnit(tgt.get(), n2, n1, logger);
                        break;
                    }
                }
            } else if (u->position == 1) {
                for (auto& tgt : t1) {
                    if (tgt->hp > 0 && tgt->position == 1) {
                        u->attackUnit(tgt.get(), n2, n1, logger);
                        break;
                    }
                }
            }
        }
    }
};
GameManager* GameManager::instance = nullptr;

// === Command Design Pattern ===
class Command {
public:
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual void redo() { execute(); } // Redo is same as execute by default
    virtual string description() const = 0;
    virtual ~Command() = default;
};

// === Create Team Command ===
class CreateTeamCommand : public Command {
public:
    CreateTeamCommand(vector<unique_ptr<Unit>>& team, const string& teamName, int balance,
                     UnitFactory& factory, GameManager& gameManager, Logger& logger)
        : team_(team), teamName_(teamName), initialBalance_(balance), factory_(factory),
          gameManager_(gameManager), logger_(logger), balance_(balance) {}

    void execute() override {
        team_.clear();
        balance_ = initialBalance_;
        gameManager_.createTeam(team_, teamName_, balance_, factory_, logger_);
        // Store team state
        teamState_.clear();
        for (const auto& unit : team_) {
            teamState_.push_back(unit->clone());
        }
        executedBalance_ = balance_;
    }

    void undo() override {
        team_.clear();
        balance_ = initialBalance_;
        logger_.log("Undoing team creation for " + teamName_, "INFO");
    }

    void redo() override {
        team_.clear();
        balance_ = executedBalance_;
        // Restore team state
        for (const auto& unit : teamState_) {
            team_.push_back(unit->clone());
        }
        logger_.log("Redoing team creation for " + teamName_, "INFO");
    }

    string description() const override {
        return "Team creation for " + teamName_;
    }

private:
    vector<unique_ptr<Unit>>& team_;
    string teamName_;
    int initialBalance_;
    UnitFactory& factory_;
    GameManager& gameManager_;
    Logger& logger_;
    int balance_;
    vector<unique_ptr<Unit>> teamState_;
    int executedBalance_;
};

// === Command Manager ===
class CommandManager {
public:
    CommandManager(Logger& logger) : logger_(logger) {}

    void execute(unique_ptr<Command> command) {
        command->execute();
        executedCommands_.push(std::move(command));
        while (!undoneCommands_.empty()) {
            undoneCommands_.pop();
        }
    }

    bool canUndo() const {
        return !executedCommands_.empty();
    }

    bool canRedo() const {
        return !undoneCommands_.empty();
    }

    void undo() {
        if (!canUndo()) return;
        auto command = std::move(executedCommands_.top());
        logger_.log("Performing undo operation: " + command->description(), "INFO");
        executedCommands_.pop();
        command->undo();
        undoneCommands_.push(std::move(command));
    }

    void redo() {
        if (!canRedo()) return;
        auto command = std::move(undoneCommands_.top());
        logger_.log("Performing redo operation: " + command->description(), "INFO");
        undoneCommands_.pop();
        command->redo();
        executedCommands_.push(std::move(command));
    }

    void clear() {
        while (!executedCommands_.empty()) executedCommands_.pop();
        while (!undoneCommands_.empty()) undoneCommands_.pop();
    }

    string lastCommandDescription() const {
        if (executedCommands_.empty()) return "No commands to undo";
        return executedCommands_.top()->description();
    }

private:
    stack<unique_ptr<Command>> executedCommands_;
    stack<unique_ptr<Command>> undoneCommands_;
    Logger& logger_;
};

// === Factory Method Pattern ===
class ManualUnitFactory : public UnitFactory {
public:
    unique_ptr<Unit> createUnit(const string& type, int pos, Logger& logger) override {
        if (type == "LI" || type == "L") {
            vector<string> buffs;
            cout << "Enter buffs for Light Infantry (Ho, Sp, Sh, He, or none, space-separated): ";
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            string buff_input;
            getline(cin, buff_input);
            logger.log("Input buffs: " + buff_input, "INFO");
            istringstream iss(buff_input);
            string buff_code;
            while (iss >> buff_code) {
                auto it = BUFFS.find(buff_code);
                if (it != BUFFS.end() && find(buffs.begin(), buffs.end(), buff_code) == buffs.end()) {
                    buffs.push_back(buff_code);
                } else {
                    logger.log("Invalid or duplicate buff: " + buff_code, "ERROR");
                }
            }
            return make_unique<LightInfantry>(pos, buffs);
        }
        if (type == "HI" || type == "I") return make_unique<HeavyInfantry>(pos);
        if (type == "A") return make_unique<Archer>(pos);
        if (type == "W") return make_unique<Wizard>(pos);
        if (type == "H") return make_unique<Healer>(pos);
        if (type == "Gu" || type == "G") return make_unique<GuliayGorodAdapter>(pos);
        return nullptr;
    }

    void createTeam(vector<unique_ptr<Unit>>& team, const string& teamName, int balance, Logger& logger) override {
        cout << teamName << " - Starting balance: " << balance << "\n";
        cout << "Units: LI (10), HI (30), A (20), W (30), H (15), Gu (25)\n";
        cout << "Buffs for LI: Horse (5, +5 HP, +2 attacks), Spear (3, +5 attack), Shield (4, +10 armor), Helmet (2, +5 HP)\n";
        int pos = 1;
        while (balance > 0) {
            string type;
            cout << "Enter unit type (or 'done' to finish): ";
            cin >> type;
            logger.log("Input unit type: " + type, "INFO");
            if (type == "done") break;
            int buff_cost = 0;
            vector<string> buffs;
            unique_ptr<Unit> unit;
            if (type == "LI" || type == "L") {
                unit = createUnit(type, pos, logger);
                for (const auto& buff : dynamic_cast<LightInfantry*>(unit.get())->active_buffs) {
                    auto it = BUFFS.find(buff);
                    if (it != BUFFS.end()) buff_cost += it->second.cost;
                }
            } else {
                unit = createUnit(type, pos, logger);
            }
            if (unit && balance >= unit->cost + buff_cost) {
                balance -= unit->cost + buff_cost;
                team.push_back(std::move(unit));
                string buff_list = buffs.empty() ? " (none)" : ": ";
                if (type == "LI" || type == "L") {
                    for (size_t i = 0; i < dynamic_cast<LightInfantry*>(team.back().get())->active_buffs.size(); ++i) {
                        auto it = BUFFS.find(dynamic_cast<LightInfantry*>(team.back().get())->active_buffs[i]);
                        buff_list += (it != BUFFS.end() ? it->second.name : dynamic_cast<LightInfantry*>(team.back().get())->active_buffs[i]);
                        if (i < dynamic_cast<LightInfantry*>(team.back().get())->active_buffs.size() - 1) buff_list += ", ";
                    }
                }
                logger.log("Added " + team.back()->name + (type == "LI" || type == "L" ? " with buffs" + buff_list : "") + ". Remaining balance: " + to_string(balance), "INFO");
                pos++;
            } else {
                logger.log("Invalid unit or insufficient balance.", "ERROR");
            }
        }
        logger.log("------------------", "INFO");
    }
};

class AutomaticUnitFactory : public UnitFactory {
public:
    unique_ptr<Unit> createUnit(const string& type, int pos, Logger& logger) override {
        if (type == "LI" || type == "L") {
            vector<string> buffs;
            int num_buffs = rand() % 3; // 0-2 buffs
            vector<string> available_buffs = {"Ho", "Sp", "Sh", "He"};
            shuffle(available_buffs.begin(), available_buffs.end(), std::default_random_engine(rand()));
            for (int i = 0; i < num_buffs; ++i) {
                buffs.push_back(available_buffs[i]);
            }
            string buff_input = "";
            for (size_t i = 0; i < buffs.size(); ++i) {
                buff_input += buffs[i];
                if (i < buffs.size() - 1) buff_input += " ";
            }
            logger.log("Automatically selected buffs for Light Infantry: " + (buff_input.empty() ? "none" : buff_input), "INFO");
            return make_unique<LightInfantry>(pos, buffs);
        }
        if (type == "HI" || type == "I") return make_unique<HeavyInfantry>(pos);
        if (type == "A") return make_unique<Archer>(pos);
        if (type == "W") return make_unique<Wizard>(pos);
        if (type == "H") return make_unique<Healer>(pos);
        if (type == "Gu" || type == "G") return make_unique<GuliayGorodAdapter>(pos);
        return nullptr;
    }

    void createTeam(vector<unique_ptr<Unit>>& team, const string& teamName, int balance, Logger& logger) override {
        cout << teamName << " - Starting balance: " << balance << "\n";
        cout << "Units: LI (10), HI (30), A (20), W (30), H (15), Gu (25)\n";
        cout << "Buffs for LI: Horse (5, +5 HP, +2 attacks), Spear (3, +5 attack), Shield (4, +10 armor), Helmet (2, +5 HP)\n";
        int pos = 1;
        vector<string> unit_types = {"LI", "HI", "A", "W", "H", "Gu"};
        int max_units = rand() % 6 + 3; // 3-8 units
        while (balance > 0 && team.size() < max_units) {
            shuffle(unit_types.begin(), unit_types.end(), std::default_random_engine(rand()));
            string type = unit_types[0];
            logger.log("Automatically selected unit type: " + type, "INFO");
            int buff_cost = 0;
            vector<string> buffs;
            unique_ptr<Unit> unit;
            if (type == "LI" || type == "L") {
                unit = createUnit(type, pos, logger);
                for (const auto& buff : dynamic_cast<LightInfantry*>(unit.get())->active_buffs) {
                    auto it = BUFFS.find(buff);
                    if (it != BUFFS.end()) buff_cost += it->second.cost;
                }
            } else {
                unit = createUnit(type, pos, logger);
            }
            if (unit && balance >= unit->cost + buff_cost) {
                balance -= unit->cost + buff_cost;
                team.push_back(std::move(unit));
                string buff_list = buffs.empty() ? " (none)" : ": ";
                if (type == "LI" || type == "L") {
                    for (size_t i = 0; i < dynamic_cast<LightInfantry*>(team.back().get())->active_buffs.size(); ++i) {
                        auto it = BUFFS.find(dynamic_cast<LightInfantry*>(team.back().get())->active_buffs[i]);
                        buff_list += (it != BUFFS.end() ? it->second.name : dynamic_cast<LightInfantry*>(team.back().get())->active_buffs[i]);
                        if (i < dynamic_cast<LightInfantry*>(team.back().get())->active_buffs.size() - 1) buff_list += ", ";
                    }
                }
                logger.log("Automatically added " + team.back()->name + (type == "LI" || type == "L" ? " with buffs" + buff_list : "") + ". Remaining balance: " + to_string(balance), "INFO");
                cout << "Added " + team.back()->name + (type == "LI" || type == "L" ? " with buffs" + buff_list : "") + ". Remaining balance: " + to_string(balance) << "\n";
                pos++;
            } else {
                break; // Stop if balance is too low
            }
        }
        logger.log("------------------", "INFO");
    }
};

// === Сохранение и загрузка ===
void saveGame(const string& filename, const string& t1, const string& t2, int round,
              const vector<unique_ptr<Unit>>& team1, const vector<unique_ptr<Unit>>& team2, Logger& logger) {
    ofstream out(filename);
    if (!out) {
        logger.log("Error: Could not save game to " + filename, "ERROR");
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
              vector<unique_ptr<Unit>>& team1, vector<unique_ptr<Unit>>& team2, Logger& logger) {
    ifstream in(filename);
    if (!in) {
        logger.log("Error: Could not open file " + filename, "ERROR");
        return;
    }
    getline(in, t1);
    getline(in, t2);
    string roundStr;
    getline(in, roundStr);
    try {
        round = stoi(roundStr);
    } catch (...) {
        logger.log("Error: Invalid round number in save file", "ERROR");
        in.close();
        return;
    }

    string line;
    while (getline(in, line) && line != "---") {
        istringstream iss(line);
        string type;
        int pos, hp;
        if (iss >> type >> pos >> hp) {
            unique_ptr<Unit> u;
            if (type == "L") u = make_unique<LightInfantry>(pos);
            else if (type == "I") u = make_unique<HeavyInfantry>(pos);
            else if (type == "A") u = make_unique<Archer>(pos);
            else if (type == "W") u = make_unique<Wizard>(pos);
            else if (type == "H") u = make_unique<Healer>(pos);
            else if (type == "G") u = make_unique<GuliayGorodAdapter>(pos);
            if (u) {
                u->hp = hp;
                u->loadExtra(iss);
                team1.push_back(std::move(u));
            } else {
                logger.log("Warning: Invalid unit type '" + type + "' in team1", "ERROR");
            }
        } else {
            logger.log("Warning: Invalid line in team1: " + line, "ERROR");
        }
    }

    while (getline(in, line)) {
        istringstream iss(line);
        string type;
        int pos, hp;
        if (iss >> type >> pos >> hp) {
            unique_ptr<Unit> u;
            if (type == "L") u = make_unique<LightInfantry>(pos);
            else if (type == "I") u = make_unique<HeavyInfantry>(pos);
            else if (type == "A") u = make_unique<Archer>(pos);
            else if (type == "W") u = make_unique<Wizard>(pos);
            else if (type == "H") u = make_unique<Healer>(pos);
            else if (type == "G") u = make_unique<GuliayGorodAdapter>(pos);
            if (u) {
                u->hp = hp;
                u->loadExtra(iss);
                team2.push_back(std::move(u));
            } else {
                logger.log("Warning: Invalid unit type '" + type + "' in team2", "ERROR");
            }
        } else {
            logger.log("Warning: Invalid line in team2: " + line, "ERROR");
        }
    }

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
    LoggerProxy logger("game.log");
    GameManager* gm = GameManager::getInstance();
    CommandManager commandManager(logger);
    vector<unique_ptr<Unit>> team1, team2;
    string t1, t2, input;
    int round = 1;

    cout << "1. Start New Game\n2. Load Game\nChoice: ";
    int choice;
    if (!(cin >> choice)) {
        logger.log("Invalid input. Exiting.", "ERROR");
        cout << "Invalid input. Exiting.\n";
        return 1;
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    if (choice == 2) {
        logger.log("Loading game session from save file", "INFO");
        loadGame("save.txt", t1, t2, round, team1, team2, logger);
        if (t1.empty() || t2.empty()) {
            logger.log("Failed to load game. Starting new game.", "INFO");
            cout << "Failed to load game. Starting new game.\n";
            choice = 1;
        } else {
            srand(time(0));
        }
    }

    if (choice == 1) {
        logger.log("Starting new game session", "INFO");
        cout << "Enter Team 1 name: ";
        getline(cin, t1);
        cout << "Enter Team 2 name: ";
        getline(cin, t2);

        // Team 1 creation
        while (true) {
            cout << "Choose team creation method for " << t1 << ": 1. Manual, 2. Automatic\nChoice: ";
            int team1_choice;
            unique_ptr<UnitFactory> team1_factory;
            if (!(cin >> team1_choice) || (team1_choice != 1 && team1_choice != 2)) {
                logger.log("Invalid team creation choice for " + t1 + ". Defaulting to Manual.", "ERROR");
                team1_factory = make_unique<ManualUnitFactory>();
            } else {
                if (team1_choice == 1) {
                    team1_factory = make_unique<ManualUnitFactory>();
                } else {
                    team1_factory = make_unique<AutomaticUnitFactory>();
                }
            }
            auto command = make_unique<CreateTeamCommand>(team1, t1, 100, *team1_factory, *gm, logger);
            commandManager.execute(std::move(command));

            cout << "Undo team creation for " << t1 << "? (y/n): ";
            cin >> input;
            if (input != "y") {
                break; // Team accepted, proceed to Team 2
            }

            commandManager.undo();
            gm->displayTeam(team1, t1, logger);
            if (commandManager.canRedo()) {
                cout << "Redo last undone team creation for " << t1 << "? (y/n): ";
                cin >> input;
                if (input == "y") {
                    commandManager.redo();
                    gm->displayTeam(team1, t1, logger);
                    cout << "Undo team creation for " << t1 << "? (y/n): ";
                    cin >> input;
                    if (input != "y") {
                        break; // Redone team accepted
                    }
                    commandManager.undo();
                    gm->displayTeam(team1, t1, logger);
                }
            }

            cout << "Create a new team for " << t1 << "? (y/n): ";
            cin >> input;
            if (input == "y") {
                logger.log("Starting new team creation for " + t1, "INFO");
                commandManager.clear(); // Clear undo/redo history
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear input buffer
                continue; // Restart Team 1 creation
            } else {
                break; // Accept empty team and proceed
            }
        }

        // Team 2 creation
        while (true) {
            cout << "Choose team creation method for " << t2 << ": 1. Manual, 2. Automatic\nChoice: ";
            int team2_choice;
            unique_ptr<UnitFactory> team2_factory;
            if (!(cin >> team2_choice) || (team2_choice != 1 && team2_choice != 2)) {
                logger.log("Invalid team creation choice for " + t2 + ". Defaulting to Manual.", "ERROR");
                team2_factory = make_unique<ManualUnitFactory>();
            } else {
                if (team2_choice == 1) {
                    team2_factory = make_unique<ManualUnitFactory>();
                } else {
                    team2_factory = make_unique<AutomaticUnitFactory>();
                }
            }
            auto command = make_unique<CreateTeamCommand>(team2, t2, 100, *team2_factory, *gm, logger);
            commandManager.execute(std::move(command));

            cout << "Undo team creation for " << t2 << "? (y/n): ";
            cin >> input;
            if (input != "y") {
                break; // Team accepted, proceed to game
            }

            commandManager.undo();
            gm->displayTeam(team2, t2, logger);
            if (commandManager.canRedo()) {
                cout << "Redo last undone team creation for " << t2 << "? (y/n): ";
                cin >> input;
                if (input == "y") {
                    commandManager.redo();
                    gm->displayTeam(team2, t2, logger);
                    cout << "Undo team creation for " << t2 << "? (y/n): ";
                    cin >> input;
                    if (input != "y") {
                        break; // Redone team accepted
                    }
                    commandManager.undo();
                    gm->displayTeam(team2, t2, logger);
                }
            }

            cout << "Create a new team for " << t2 << "? (y/n): ";
            cin >> input;
            if (input == "y") {
                logger.log("Starting new team creation for " + t2, "INFO");
                commandManager.clear(); // Clear undo/redo history
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear input buffer
                continue; // Restart Team 2 creation
            } else {
                break; // Accept empty team and proceed
            }
        }

        // Clear command history before starting game
        commandManager.clear();
    }

    cout << "Type 'Start' to begin: ";
    cin >> input;
    if (input != "Start") return 0;

    while (gm->isTeamAlive(team1) && gm->isTeamAlive(team2)) {
        gm->simulateRound(team1, team2, t1, t2, round++, logger);
        gm->cleanAndShift(team1);
        gm->cleanAndShift(team2);
        gm->displayTeam(team1, t1, logger);
        gm->displayTeam(team2, t2, logger);
        cout << "Save game? (y/n): ";
        cin >> input;
        if (input == "y") saveGame("save.txt", t1, t2, round, team1, team2, logger);
        logger.log("------------------", "INFO");
    }

    cout << "\n" << (gm->isTeamAlive(team1) ? t1 : t2) << " wins!\n";
    return 0;
}