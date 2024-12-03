#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

class Type {
public:
    virtual ~Type() = default;
    virtual std::string toString() const = 0; // does nothing
    virtual std::shared_ptr<Type> apply(const std::unordered_map<std::string, std::shared_ptr<Type>>& subst) const = 0;
    virtual void collectFreeTypeVars(std::unordered_set<std::string>& vars) const = 0;
};

class TypeVariable : public Type {
public:
    std::string name;

    TypeVariable(const std::string& name) : name(name) {}

    std::string toString() const override {
        return name;
    }

    std::shared_ptr<Type> apply(const std::unordered_map<std::string, std::shared_ptr<Type>>& subst) const override {
        auto it = subst.find(name);
        if (it != subst.end()) {
            return it->second->apply(subst);
        }
        return std::make_shared<TypeVariable>(name);
    }

    void collectFreeTypeVars(std::unordered_set<std::string>& vars) const override {
        vars.insert(name);
    }
};

class TypeConstant : public Type {
public:
    std::string name;

    TypeConstant(const std::string& name) : name(name) {}

    std::string toString() const override {
        return name;
    }

    std::shared_ptr<Type> apply(const std::unordered_map<std::string, std::shared_ptr<Type>>& subst) const override {
        return std::make_shared<TypeConstant>(name);
    }

    void collectFreeTypeVars(std::unordered_set<std::string>& vars) const override {
    }
};

class TypeFunction : public Type {
public:
    std::shared_ptr<Type> from;
    std::shared_ptr<Type> to;

    TypeFunction(std::shared_ptr<Type> from, std::shared_ptr<Type> to) : from(from), to(to) {}

    std::string toString() const override {
        return "(" + from->toString() + " -> " + to->toString() + ")";
    }

    std::shared_ptr<Type> apply(const std::unordered_map<std::string, std::shared_ptr<Type>>& subst) const override {
        return std::make_shared<TypeFunction>(from->apply(subst), to->apply(subst));
    }

    void collectFreeTypeVars(std::unordered_set<std::string>& vars) const override {
        from->collectFreeTypeVars(vars);
        to->collectFreeTypeVars(vars);
    }
};

// Occurs check to prevent infinite types
bool occursInType(const std::string& varName, std::shared_ptr<Type> type) {
    std::unordered_set<std::string> vars;
    type->collectFreeTypeVars(vars);
    return vars.find(varName) != vars.end();
}

// The unification algorithm
void unify(std::shared_ptr<Type> t1, std::shared_ptr<Type> t2, std::unordered_map<std::string, std::shared_ptr<Type>>& subst) {
    t1 = t1->apply(subst);
    t2 = t2->apply(subst);

    if (auto var1 = std::dynamic_pointer_cast<TypeVariable>(t1)) {
        if (t1->toString() != t2->toString()) {
            if (occursInType(var1->name, t2)) {
                throw std::runtime_error("Occurs check failed: " + var1->name + " occurs in " + t2->toString());
            }
            subst[var1->name] = t2;
        }
    }
    else if (auto var2 = std::dynamic_pointer_cast<TypeVariable>(t2)) {
        unify(t2, t1, subst);
    }
    else if (auto func1 = std::dynamic_pointer_cast<TypeFunction>(t1)) {
        if (auto func2 = std::dynamic_pointer_cast<TypeFunction>(t2)) {
            unify(func1->from, func2->from, subst);
            unify(func1->to, func2->to, subst);
        }
        else {
            throw std::runtime_error("Type mismatch: " + t1->toString() + " vs " + t2->toString());
        }
    }
    else if (t1->toString() == t2->toString()) {
        // Type constants are equal
    }
    else {
        throw std::runtime_error("Type mismatch: " + t1->toString() + " vs " + t2->toString());
    }
}

// Function to print the substitutions
void printSubstitution(const std::unordered_map<std::string, std::shared_ptr<Type>>& subst) {
    std::cout << "Substitutions:\n";
    for (const auto& pair : subst) {
        std::cout << pair.first << " := " << pair.second->toString() << "\n";
    }
}

int main() {
    try {
        // Example Types:
        // t1: (a -> b)
        // t2: (Int -> Bool)

        auto a = std::make_shared<TypeVariable>("a");
        auto b = std::make_shared<TypeVariable>("b");
        auto t1 = std::make_shared<TypeFunction>(a, b);

        auto intType = std::make_shared<TypeConstant>("Int");
        auto boolType = std::make_shared<TypeConstant>("Bool");
        auto t2 = std::make_shared<TypeFunction>(intType, boolType);

        std::unordered_map<std::string, std::shared_ptr<Type>> subst;

        // Perform unification
        unify(t1, t2, subst);

        // Print results
        printSubstitution(subst);

        // Apply substitution to t1 and t2
        auto t1Unified = t1->apply(subst);
        auto t2Unified = t2->apply(subst);

        std::cout << "Unified t1: " << t1Unified->toString() << "\n";
        std::cout << "Unified t2: " << t2Unified->toString() << "\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "Unification failed: " << ex.what() << "\n";
    }

    return 0;
}
