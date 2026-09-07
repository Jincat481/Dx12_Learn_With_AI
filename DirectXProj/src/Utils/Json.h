#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdint>

// =============================================================
// 최소 JSON 라이브러리 (S20)
//  - 외부 의존성 없이 저장/로드에 필요한 만큼만 구현한다.
//  - Object 는 삽입 순서를 유지한다(사람이 읽기 좋은 저장 결과).
//  - 파일 입출력은 UTF-8 로 처리한다.
// =============================================================
namespace json
{
    class Value
    {
    public:
        enum class Type { Null, Bool, Number, String, Array, Object };

        Value() : m_type(Type::Null) {}
        Value(bool v) : m_type(Type::Bool), m_bool(v) {}
        Value(double v) : m_type(Type::Number), m_number(v) {}
        Value(float v) : m_type(Type::Number), m_number(static_cast<double>(v)) {}
        Value(int v) : m_type(Type::Number), m_number(static_cast<double>(v)) {}
        Value(uint64_t v) : m_type(Type::Number), m_number(static_cast<double>(v)) {}
        Value(const char* v) : m_type(Type::String), m_string(v ? v : "") {}
        Value(std::string v) : m_type(Type::String), m_string(std::move(v)) {}

        static Value MakeObject() { Value v; v.m_type = Type::Object; return v; }
        static Value MakeArray()  { Value v; v.m_type = Type::Array;  return v; }

        Type GetType() const { return m_type; }
        bool IsNull()   const { return m_type == Type::Null; }
        bool IsBool()   const { return m_type == Type::Bool; }
        bool IsNumber() const { return m_type == Type::Number; }
        bool IsString() const { return m_type == Type::String; }
        bool IsArray()  const { return m_type == Type::Array; }
        bool IsObject() const { return m_type == Type::Object; }

        // ---- 읽기 : 타입이 다르면 기본값을 돌려주어 로드 실패를 막는다 ----
        bool        AsBool(bool def = false) const   { return m_type == Type::Bool ? m_bool : def; }
        double      AsDouble(double def = 0.0) const { return m_type == Type::Number ? m_number : def; }
        float       AsFloat(float def = 0.0f) const  { return static_cast<float>(AsDouble(def)); }
        int         AsInt(int def = 0) const         { return static_cast<int>(AsDouble(def)); }
        uint64_t    AsUInt64(uint64_t def = 0) const { return static_cast<uint64_t>(AsDouble(static_cast<double>(def))); }
        std::string AsString(const std::string& def = std::string()) const { return m_type == Type::String ? m_string : def; }

        // ---- Object ----
        Value&       operator[](const std::string& key);            // 없으면 만든다
        const Value* Find(const std::string& key) const;            // 없으면 nullptr
        bool         Has(const std::string& key) const { return Find(key) != nullptr; }
        const std::vector<std::pair<std::string, Value>>& Members() const { return m_object; }

        // ---- Array ----
        void         Push(Value v);
        size_t       Size() const;
        const Value& At(size_t index) const;
        const std::vector<Value>& Elements() const { return m_array; }

        // ---- 직렬화 / 역직렬화 ----
        std::string Dump(int indent = 2) const;
        static bool Parse(const std::string& text, Value& out, std::string& error);

        bool SaveToFile(const std::wstring& path, int indent = 2) const;
        static bool LoadFromFile(const std::wstring& path, Value& out, std::string& error);

    private:
        void DumpTo(std::string& out, int indent, int depth) const;

        Type        m_type;
        bool        m_bool = false;
        double      m_number = 0.0;
        std::string m_string;

        std::vector<Value> m_array;
        std::vector<std::pair<std::string, Value>> m_object;

        static const Value s_null;
    };
}
