//
// Created by Arunas on 23/05/2024.
//

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"

#include "Patterns.hpp"

namespace myseq {

    Pattern pattern_from_json(const rapidjson::Value &value) {
        auto id = value["id"].GetInt();
        auto width = value["width"].GetInt();
        auto height = value["height"].GetInt();
        auto first_note = value["first_note"].GetInt();
        auto last_note = value["last_note"].GetInt();
        auto carr = value["data"].GetArray();
        Pattern p(id, width, height, first_note, last_note);
        for (int i = 0; i < carr.Size(); i++) {
            auto cobj = carr[i].GetObject();
            std::size_t cell_index = cobj["i"].GetInt();
            auto velocity = static_cast<uint8_t>(cobj["v"].GetInt());
            p.data[cell_index].velocity = velocity;
        }
        return p;
    }

    State State::from_json_string(const char *s) {
        rapidjson::Document d;
        rapidjson::ParseResult ok = d.Parse(s);
        if (!ok) {
            fprintf(stderr, "JSON parse error: %s (%lu)",
                    rapidjson::GetParseError_En(ok.Code()), ok.Offset());
            exit(EXIT_FAILURE);
        }
        auto arr = d["patterns"].GetArray();
        State state;
        state.selected = d["selected"].GetInt();
        for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
            auto obj = arr[i].GetObject();
            state.patterns.push_back(pattern_from_json(obj));
        }
        return state;
    }

    void test_serialize() {
        State state;
        const auto a = state.create_pattern();
        const auto b = state.create_pattern();
        state.selected = a.id;
        state.get_pattern(a.id).set_velocity(V2i(0, 1), 100);
        state.get_pattern(b.id).set_velocity(V2i(2, 3), 99);
        state.get_pattern(b.id).set_velocity(V2i(3, 4), 102);
        state.get_pattern(b.id).set_velocity(V2i(5, 6), 103);

        const auto s = state.to_json_string();
        State state1 = State::from_json_string(s.c_str());

        assert(state.selected == state1.selected);
        assert(state.num_patterns() == state1.num_patterns());
        assert(state.get_pattern(a.id).get_velocity(V2i(0, 1)) == state1.get_pattern(a.id).get_velocity(V2i(0, 1)));
        assert(state.get_pattern(b.id).get_velocity(V2i(2, 3)) == state1.get_pattern(b.id).get_velocity(V2i(2, 3)));
    }


    [[nodiscard]] rapidjson::Value
    pattern_to_json(const Pattern &pattern, rapidjson::Document::AllocatorType &allocator) {
        rapidjson::Value pobj(rapidjson::kObjectType);
        pobj.AddMember("width", pattern.width, allocator)
                .AddMember("id", pattern.id, allocator)
                .AddMember("height", pattern.height, allocator)
                .AddMember("first_note", pattern.first_note, allocator)
                .AddMember("last_note", pattern.last_note, allocator);
        //d.GetObject().AddMember("data", height, d.GetAllocator());
        rapidjson::Value data_arr(rapidjson::kArrayType);
        for (int i = 0; i < pattern.data.size(); i++) {
            const auto &cell = pattern.data[i];
            if (cell.velocity > 0) {
                rapidjson::Value o(rapidjson::kObjectType);
                auto ob = o.GetObject();
                ob.AddMember("i", (int) i, allocator)
                        .AddMember("v", (int) cell.velocity, allocator);
                data_arr.PushBack(o, allocator);
            }
        }
        pobj.GetObject().AddMember("data", data_arr, allocator);
        return pobj;
    }

    struct Opaque {
        rapidjson::Document d;
    };

    [[nodiscard]] Opaque State::to_json() const {
        rapidjson::Document d;
        d.SetObject();
        d.GetObject().AddMember("selected", this->selected, d.GetAllocator());
        rapidjson::Value patterns_arr(rapidjson::kArrayType);
        for (auto p: this->patterns) {
            rapidjson::Value o = pattern_to_json(p, d.GetAllocator());
            patterns_arr.PushBack(o, d.GetAllocator());
        }
        d.GetObject().AddMember("patterns", patterns_arr, d.GetAllocator());
        return Opaque{std::move(d)};
    }

    [[nodiscard]] std::string State::to_json_string() const {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        to_json().d.Accept(writer);
        const auto s = buffer.GetString();
        return s;
    }

}
