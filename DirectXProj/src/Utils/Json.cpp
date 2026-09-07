#include "Core/stdafx.h"
#include "Utils/Json.h"

#include <cstdio>
#include <cmath>
#include <cctype>
#include <sstream>
#include <fstream>

namespace json
{
    const Value Value::s_null;

    // ============================================================
    // Object / Array 접근
    // ============================================================
    Value& Value::operator[](const std::string& key)
    {
        if (m_type != Type::Object)
        {
            m_type = Type::Object;
            m_object.clear();
        }

        for (auto& member : m_object)
        {
            if (member.first == key)
                return member.second;
        }

        m_object.emplace_back(key, Value());
        return m_object.back().second;
    }

    const Value* Value::Find(const std::string& key) const
    {
        if (m_type != Type::Object)
            return nullptr;

        for (const auto& member : m_object)
        {
            if (member.first == key)
                return &member.second;
        }
        return nullptr;
    }

    void Value::Push(Value v)
    {
        if (m_type != Type::Array)
        {
            m_type = Type::Array;
            m_array.clear();
        }
        m_array.push_back(std::move(v));
    }

    size_t Value::Size() const
    {
        if (m_type == Type::Array)  return m_array.size();
        if (m_type == Type::Object) return m_object.size();
        return 0;
    }

    const Value& Value::At(size_t index) const
    {
        if (m_type != Type::Array || index >= m_array.size())
            return s_null;
        return m_array[index];
    }

    // ============================================================
    // 직렬화
    // ============================================================
    static void EscapeString(const std::string& in, std::string& out)
    {
        out += '"';
        for (unsigned char c : in)
        {
            switch (c)
            {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    // UTF-8 바이트는 그대로 통과시킨다.
                    out += static_cast<char>(c);
                }
                break;
            }
        }
        out += '"';
    }

    static void NumberToString(double value, std::string& out)
    {
        if (std::isnan(value) || std::isinf(value))
        {
            out += "0";
            return;
        }

        // 정수로 떨어지면 정수 표기로 남긴다(ID 가 1.0 으로 저장되지 않게 한다).
        if (value == std::floor(value) && std::fabs(value) < 1.0e15)
        {
            char buf[32];
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%lld", static_cast<long long>(value));
            out += buf;
            return;
        }

        char buf[64];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.9g", value);
        out += buf;
    }

    void Value::DumpTo(std::string& out, int indent, int depth) const
    {
        const bool pretty = indent > 0;
        const std::string pad(pretty ? static_cast<size_t>(indent) * (depth + 1) : 0, ' ');
        const std::string padEnd(pretty ? static_cast<size_t>(indent) * depth : 0, ' ');
        const char* newline = pretty ? "\n" : "";

        switch (m_type)
        {
        case Type::Null:   out += "null"; break;
        case Type::Bool:   out += (m_bool ? "true" : "false"); break;
        case Type::Number: NumberToString(m_number, out); break;
        case Type::String: EscapeString(m_string, out); break;

        case Type::Array:
        {
            if (m_array.empty()) { out += "[]"; break; }

            // 숫자만 든 짧은 배열(위치/회전/스케일)은 한 줄로 남긴다.
            bool allNumber = true;
            for (const auto& e : m_array)
                allNumber = allNumber && e.IsNumber();

            if (allNumber && m_array.size() <= 4)
            {
                out += "[";
                for (size_t i = 0; i < m_array.size(); ++i)
                {
                    if (i) out += ", ";
                    NumberToString(m_array[i].m_number, out);
                }
                out += "]";
                break;
            }

            out += "["; out += newline;
            for (size_t i = 0; i < m_array.size(); ++i)
            {
                out += pad;
                m_array[i].DumpTo(out, indent, depth + 1);
                if (i + 1 < m_array.size()) out += ",";
                out += newline;
            }
            out += padEnd; out += "]";
            break;
        }

        case Type::Object:
        {
            if (m_object.empty()) { out += "{}"; break; }

            out += "{"; out += newline;
            for (size_t i = 0; i < m_object.size(); ++i)
            {
                out += pad;
                EscapeString(m_object[i].first, out);
                out += pretty ? ": " : ":";
                m_object[i].second.DumpTo(out, indent, depth + 1);
                if (i + 1 < m_object.size()) out += ",";
                out += newline;
            }
            out += padEnd; out += "}";
            break;
        }
        }
    }

    std::string Value::Dump(int indent) const
    {
        std::string out;
        DumpTo(out, indent, 0);
        return out;
    }

    // ============================================================
    // 파싱
    // ============================================================
    namespace
    {
        struct Parser
        {
            const std::string& text;
            size_t pos = 0;
            std::string error;

            explicit Parser(const std::string& t) : text(t) {}

            void SkipWhitespace()
            {
                while (pos < text.size())
                {
                    const char c = text[pos];
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos; continue; }

                    // 편의를 위해 한 줄 주석 정도는 허용한다.
                    if (c == '/' && pos + 1 < text.size() && text[pos + 1] == '/')
                    {
                        while (pos < text.size() && text[pos] != '\n') ++pos;
                        continue;
                    }
                    break;
                }
            }

            bool Fail(const char* message)
            {
                if (error.empty())
                {
                    std::ostringstream oss;
                    oss << message << " (offset " << pos << ")";
                    error = oss.str();
                }
                return false;
            }

            bool ParseValue(Value& out)
            {
                SkipWhitespace();
                if (pos >= text.size())
                    return Fail("unexpected end of input");

                switch (text[pos])
                {
                case '{': return ParseObject(out);
                case '[': return ParseArray(out);
                case '"':
                {
                    std::string s;
                    if (!ParseString(s)) return false;
                    out = Value(std::move(s));
                    return true;
                }
                case 't':
                    if (text.compare(pos, 4, "true") == 0) { pos += 4; out = Value(true); return true; }
                    return Fail("invalid literal");
                case 'f':
                    if (text.compare(pos, 5, "false") == 0) { pos += 5; out = Value(false); return true; }
                    return Fail("invalid literal");
                case 'n':
                    if (text.compare(pos, 4, "null") == 0) { pos += 4; out = Value(); return true; }
                    return Fail("invalid literal");
                default:
                    return ParseNumber(out);
                }
            }

            bool ParseObject(Value& out)
            {
                out = Value::MakeObject();
                ++pos;  // '{'
                SkipWhitespace();

                if (pos < text.size() && text[pos] == '}') { ++pos; return true; }

                while (true)
                {
                    SkipWhitespace();
                    std::string key;
                    if (!ParseString(key)) return Fail("object key expected");

                    SkipWhitespace();
                    if (pos >= text.size() || text[pos] != ':') return Fail("':' expected");
                    ++pos;

                    Value child;
                    if (!ParseValue(child)) return false;
                    out[key] = std::move(child);

                    SkipWhitespace();
                    if (pos >= text.size()) return Fail("unterminated object");
                    if (text[pos] == ',') { ++pos; continue; }
                    if (text[pos] == '}') { ++pos; return true; }
                    return Fail("',' or '}' expected in object");
                }
            }

            bool ParseArray(Value& out)
            {
                out = Value::MakeArray();
                ++pos;  // '['
                SkipWhitespace();

                if (pos < text.size() && text[pos] == ']') { ++pos; return true; }

                while (true)
                {
                    Value child;
                    if (!ParseValue(child)) return false;
                    out.Push(std::move(child));

                    SkipWhitespace();
                    if (pos >= text.size()) return Fail("unterminated array");
                    if (text[pos] == ',') { ++pos; continue; }
                    if (text[pos] == ']') { ++pos; return true; }
                    return Fail("',' or ']' expected in array");
                }
            }

            static void AppendUtf8(std::string& out, unsigned int codepoint)
            {
                if (codepoint < 0x80)
                {
                    out += static_cast<char>(codepoint);
                }
                else if (codepoint < 0x800)
                {
                    out += static_cast<char>(0xC0 | (codepoint >> 6));
                    out += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                else if (codepoint < 0x10000)
                {
                    out += static_cast<char>(0xE0 | (codepoint >> 12));
                    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                else
                {
                    out += static_cast<char>(0xF0 | (codepoint >> 18));
                    out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
            }

            bool ParseHex4(unsigned int& out)
            {
                if (pos + 4 > text.size()) return false;
                out = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char c = text[pos++];
                    out <<= 4;
                    if (c >= '0' && c <= '9') out |= static_cast<unsigned>(c - '0');
                    else if (c >= 'a' && c <= 'f') out |= static_cast<unsigned>(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') out |= static_cast<unsigned>(c - 'A' + 10);
                    else return false;
                }
                return true;
            }

            bool ParseString(std::string& out)
            {
                SkipWhitespace();
                if (pos >= text.size() || text[pos] != '"')
                    return Fail("string expected");
                ++pos;

                out.clear();
                while (pos < text.size())
                {
                    const char c = text[pos++];
                    if (c == '"')
                        return true;

                    if (c != '\\')
                    {
                        out += c;
                        continue;
                    }

                    if (pos >= text.size()) break;
                    const char esc = text[pos++];
                    switch (esc)
                    {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u':
                    {
                        unsigned int cp = 0;
                        if (!ParseHex4(cp)) return Fail("bad \\u escape");

                        // 서러게이트 쌍 처리
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos + 1 < text.size() &&
                            text[pos] == '\\' && text[pos + 1] == 'u')
                        {
                            pos += 2;
                            unsigned int low = 0;
                            if (!ParseHex4(low)) return Fail("bad surrogate pair");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        }
                        AppendUtf8(out, cp);
                        break;
                    }
                    default:
                        return Fail("unknown escape");
                    }
                }
                return Fail("unterminated string");
            }

            bool ParseNumber(Value& out)
            {
                const size_t start = pos;
                if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) ++pos;
                while (pos < text.size() && (std::isdigit(static_cast<unsigned char>(text[pos])) ||
                                             text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' ||
                                             text[pos] == '+' || text[pos] == '-'))
                {
                    ++pos;
                }

                if (start == pos)
                    return Fail("number expected");

                try
                {
                    out = Value(std::stod(text.substr(start, pos - start)));
                }
                catch (...)
                {
                    return Fail("number conversion failed");
                }
                return true;
            }
        };
    }

    bool Value::Parse(const std::string& text, Value& out, std::string& error)
    {
        Parser parser(text);
        if (!parser.ParseValue(out))
        {
            error = parser.error;
            return false;
        }

        parser.SkipWhitespace();
        if (parser.pos != text.size())
        {
            error = "trailing characters after JSON value";
            return false;
        }
        return true;
    }

    // ============================================================
    // 파일 입출력 (UTF-8)
    // ============================================================
    bool Value::SaveToFile(const std::wstring& path, int indent) const
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
            return false;

        const std::string text = Dump(indent);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        return file.good();
    }

    bool Value::LoadFromFile(const std::wstring& path, Value& out, std::string& error)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            error = "file not found";
            return false;
        }

        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // UTF-8 BOM 제거
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            text.erase(0, 3);
        }

        return Parse(text, out, error);
    }
}
