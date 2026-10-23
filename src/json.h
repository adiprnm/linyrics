/* ------------------------------------------------------------------
   Linamp — minimal JSON (parse + dump) for settings & playlist.
   Only what the app needs: objects, arrays, strings, numbers, bools.
   ------------------------------------------------------------------ */
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string string_;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    Json() = default;
    static Json makeNull() { return Json(); }
    static Json makeBool(bool v);
    static Json makeNumber(double v);
    static Json makeString(std::string v);
    static Json makeArray();
    static Json makeObject();

    static Json parse(std::string_view text, bool *ok = nullptr);
    std::string dump(int indent = -1) const;

    /* object helpers */
    const Json *find(const std::string &key) const;
    Json &set(const std::string &key, Json value);
    std::string str(const std::string &key, const std::string &def = {}) const;
    double num(const std::string &key, double def = 0) const;
    bool flag(const std::string &key, bool def = false) const;
    const Json &at(const std::string &key) const;

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    size_t size() const { return type == Type::Array ? array.size() : object.size(); }

private:
    void dumpTo(std::string &out, int indent, int depth) const;
};
