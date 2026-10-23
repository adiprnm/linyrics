#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

const Json &nullJson()
{
    static const Json n;
    return n;
}

struct Parser {
    std::string_view s;
    size_t i = 0;
    bool ok = true;

    void skip()
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
            ++i;
    }

    bool eof() const { return i >= s.size(); }

    void fail()
    {
        ok = false;
        i = s.size();
    }

    static void appendUtf8(std::string &out, unsigned cp)
    {
        if (cp < 0x80) {
            out.push_back(char(cp));
        } else if (cp < 0x800) {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (cp >> 18)));
            out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }

    std::string parseString()
    {
        std::string out;
        if (eof() || s[i] != '"') {
            fail();
            return out;
        }
        ++i;
        while (!eof()) {
            const char c = s[i++];
            if (c == '"')
                return out;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (eof()) {
                fail();
                return out;
            }
            const char e = s[i++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (i + 4 > s.size()) {
                    fail();
                    return out;
                }
                unsigned cp = 0;
                for (int k = 0; k < 4; ++k) {
                    const char h = s[i++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9')
                        cp |= unsigned(h - '0');
                    else if (h >= 'a' && h <= 'f')
                        cp |= unsigned(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F')
                        cp |= unsigned(h - 'A' + 10);
                    else {
                        fail();
                        return out;
                    }
                }
                appendUtf8(out, cp);
                break;
            }
            default: fail(); return out;
            }
        }
        fail();
        return out;
    }

    Json parseValue()
    {
        skip();
        if (eof()) {
            fail();
            return Json();
        }
        const char c = s[i];
        if (c == '{') {
            ++i;
            Json obj = Json::makeObject();
            skip();
            if (!eof() && s[i] == '}') {
                ++i;
                return obj;
            }
            for (;;) {
                skip();
                std::string key = parseString();
                if (!ok)
                    return obj;
                skip();
                if (eof() || s[i] != ':') {
                    fail();
                    return obj;
                }
                ++i;
                Json v = parseValue();
                if (!ok)
                    return obj;
                obj.object.emplace_back(std::move(key), std::move(v));
                skip();
                if (!eof() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (!eof() && s[i] == '}') {
                    ++i;
                    return obj;
                }
                fail();
                return obj;
            }
        }
        if (c == '[') {
            ++i;
            Json arr = Json::makeArray();
            skip();
            if (!eof() && s[i] == ']') {
                ++i;
                return arr;
            }
            for (;;) {
                Json v = parseValue();
                if (!ok)
                    return arr;
                arr.array.push_back(std::move(v));
                skip();
                if (!eof() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (!eof() && s[i] == ']') {
                    ++i;
                    return arr;
                }
                fail();
                return arr;
            }
        }
        if (c == '"') {
            std::string str = parseString();
            if (!ok)
                return Json();
            return Json::makeString(std::move(str));
        }
        if (s.compare(i, 4, "true") == 0) {
            i += 4;
            return Json::makeBool(true);
        }
        if (s.compare(i, 5, "false") == 0) {
            i += 5;
            return Json::makeBool(false);
        }
        if (s.compare(i, 4, "null") == 0) {
            i += 4;
            return Json();
        }
        // number
        const size_t start = i;
        if (!eof() && (s[i] == '-' || s[i] == '+'))
            ++i;
        while (!eof() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                          s[i] == '-' || s[i] == '+'))
            ++i;
        if (i == start) {
            fail();
            return Json();
        }
        const std::string tmp(s.substr(start, i - start));
        return Json::makeNumber(std::strtod(tmp.c_str(), nullptr));
    }
};

void escapeTo(std::string &out, const std::string &s)
{
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(char(c));
            }
        }
    }
    out.push_back('"');
}

} // namespace

Json Json::makeBool(bool v)
{
    Json j;
    j.type = Type::Bool;
    j.boolean = v;
    return j;
}
Json Json::makeNumber(double v)
{
    Json j;
    j.type = Type::Number;
    j.number = v;
    return j;
}
Json Json::makeString(std::string v)
{
    Json j;
    j.type = Type::String;
    j.string_ = std::move(v);
    return j;
}
Json Json::makeArray()
{
    Json j;
    j.type = Type::Array;
    return j;
}
Json Json::makeObject()
{
    Json j;
    j.type = Type::Object;
    return j;
}

Json Json::parse(std::string_view text, bool *ok)
{
    Parser p{text};
    Json v = p.parseValue();
    if (ok)
        *ok = p.ok;
    return v;
}

const Json *Json::find(const std::string &key) const
{
    if (type != Type::Object)
        return nullptr;
    for (const auto &kv : object)
        if (kv.first == key)
            return &kv.second;
    return nullptr;
}

const Json &Json::at(const std::string &key) const
{
    if (const Json *v = find(key))
        return *v;
    return nullJson();
}

Json &Json::set(const std::string &key, Json value)
{
    if (type != Type::Object) {
        type = Type::Object;
        object.clear();
    }
    for (auto &kv : object) {
        if (kv.first == key) {
            kv.second = std::move(value);
            return kv.second;
        }
    }
    object.emplace_back(key, std::move(value));
    return object.back().second;
}

std::string Json::str(const std::string &key, const std::string &def) const
{
    const Json &v = at(key);
    return v.type == Type::String ? v.string_ : def;
}

double Json::num(const std::string &key, double def) const
{
    const Json &v = at(key);
    return v.type == Type::Number ? v.number : def;
}

bool Json::flag(const std::string &key, bool def) const
{
    const Json &v = at(key);
    return v.type == Type::Bool ? v.boolean : def;
}

std::string Json::dump(int indent) const
{
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

void Json::dumpTo(std::string &out, int indent, int depth) const
{
    auto newline = [&](int d) {
        if (indent < 0)
            return;
        out.push_back('\n');
        out.append(size_t(indent * d), ' ');
    };
    switch (type) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += boolean ? "true" : "false"; break;
    case Type::Number: {
        if (std::fabs(number - std::llround(number)) < 1e-9) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%lld", (long long)std::llround(number));
            out += buf;
        } else {
            char buf[48];
            std::snprintf(buf, sizeof buf, "%.10g", number);
            out += buf;
        }
        break;
    }
    case Type::String: escapeTo(out, string_); break;
    case Type::Array: {
        out.push_back('[');
        for (size_t i = 0; i < array.size(); ++i) {
            if (i)
                out.push_back(',');
            newline(depth + 1);
            array[i].dumpTo(out, indent, depth + 1);
        }
        if (!array.empty())
            newline(depth);
        out.push_back(']');
        break;
    }
    case Type::Object: {
        out.push_back('{');
        for (size_t i = 0; i < object.size(); ++i) {
            if (i)
                out.push_back(',');
            newline(depth + 1);
            escapeTo(out, object[i].first);
            out.push_back(':');
            if (indent >= 0)
                out.push_back(' ');
            object[i].second.dumpTo(out, indent, depth + 1);
        }
        if (!object.empty())
            newline(depth);
        out.push_back('}');
        break;
    }
    }
}
