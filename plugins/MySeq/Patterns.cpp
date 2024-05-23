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
        auto width = value["width"].GetInt();
        auto height = value["height"].GetInt();
        auto carr = value["data"].GetArray();
        Pattern p(width, height);
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
            int index = obj["i"].GetInt();
            auto pobj = obj["pattern"].GetObject();
            state.patterns[index] = pattern_from_json(pobj);
        }
        return state;
    }

    void test_serialize() {
        State state;
        state.create_pattern(0);
        state.create_pattern(1);
        state.selected = 1;
        state.patterns[0].set_velocity(V2i(0, 1), 100);
        state.patterns[1].set_velocity(V2i(2, 3), 99);
        state.patterns[1].set_velocity(V2i(3, 4), 102);
        state.patterns[1].set_velocity(V2i(5, 6), 103);

        const auto s = state.to_json_string();
        State state1 = State::from_json_string(s.c_str());

        assert(state.selected == state1.selected);
        assert(state.patterns.size() == state1.patterns.size());
        assert(state.patterns[0].get_velocity(V2i(0, 1)) == state1.patterns[0].get_velocity(V2i(0, 1)));
        assert(state.patterns[1].get_velocity(V2i(2, 3)) == state1.patterns[1].get_velocity(V2i(2, 3)));
    }


    [[nodiscard]] rapidjson::Value
    pattern_to_json(const Pattern &pattern, rapidjson::Document::AllocatorType &allocator) {
        rapidjson::Value pobj(rapidjson::kObjectType);
        pobj.GetObject().AddMember("width", pattern.width, allocator);
        pobj.GetObject().AddMember("height", pattern.height, allocator);
        //d.GetObject().AddMember("data", height, d.GetAllocator());
        rapidjson::Value data_arr(rapidjson::kArrayType);
        for (int i = 0; i < pattern.data.size(); i++) {
            const auto &cell = pattern.data[i];
            if (cell.velocity > 0) {
                rapidjson::Value o(rapidjson::kObjectType);
                o.GetObject().AddMember("i", (int) i, allocator);
                o.GetObject().AddMember("v", (int) cell.velocity, allocator);
                data_arr.PushBack(o, allocator);
            }
        }
        pobj.GetObject().AddMember("data", data_arr, allocator);
        return pobj;
    }

    [[nodiscard]] rapidjson::Document state_to_json(const State &state) {
        rapidjson::Document d;
        d.SetObject();
        d.GetObject().AddMember("selected", state.selected, d.GetAllocator());
        rapidjson::Value patterns_arr(rapidjson::kArrayType);
        for (auto &kv: state.patterns) {
            rapidjson::Value o(rapidjson::kObjectType);
            o.GetObject().AddMember("i", (int) kv.first, d.GetAllocator());
            o.GetObject().AddMember("pattern", pattern_to_json(kv.second, d.GetAllocator()), d.GetAllocator());
            patterns_arr.PushBack(o, d.GetAllocator());
        }
        d.GetObject().AddMember("patterns", patterns_arr, d.GetAllocator());
        return d;
    }

    [[nodiscard]] std::string State::to_json_string() const {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        state_to_json(*this).Accept(writer);
        const auto s = buffer.GetString();
        return s;
    }

}
