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
        printf("from_json_string: %s\n", s);
        rapidjson::ParseResult ok = d.Parse(s);
        if (!ok) {
            fprintf(stderr, "JSON parse error: %s (%lu)",
                    rapidjson::GetParseError_En(ok.Code()), ok.Offset());
            exit(EXIT_FAILURE);
        }
        auto arr = d["patterns"].GetArray();
        printf("patterns: %d\n", arr.Size());
        auto selected = d["selected"].GetInt();
        printf("selected: %d\n", selected);
        State state;
        for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
            printf("pattern: %i\n", i);
            auto obj = arr[i].GetObject();
            int index = obj["i"].GetInt();
            printf("pattern: %i index=%d\n", i, index);
            auto pobj = obj["pattern"].GetObject();
            state.patterns[index] = pattern_from_json(pobj);
        }
        printf("parse ok\n");
        return state;
    }


    [[nodiscard]] rapidjson::Document pattern_to_json(const Pattern &pattern) {
        rapidjson::Document d;
        d.SetObject();
        d.GetObject().AddMember("width", pattern.width, d.GetAllocator());
        d.GetObject().AddMember("height", pattern.height, d.GetAllocator());
        //d.GetObject().AddMember("data", height, d.GetAllocator());
        rapidjson::Value data_arr(rapidjson::kArrayType);
        for (int i = 0; i < pattern.data.size(); i++) {
            const auto &cell = pattern.data[i];
            if (cell.velocity > 0) {
                rapidjson::Value o(rapidjson::kObjectType);
                o.GetObject().AddMember("i", (int) i, d.GetAllocator());
                o.GetObject().AddMember("v", (int) cell.velocity, d.GetAllocator());
                data_arr.PushBack(o, d.GetAllocator());
            }
        }
        d.GetObject().AddMember("data", data_arr, d.GetAllocator());
        return d;
    }

    [[nodiscard]] rapidjson::Document state_to_json(const State &state) {
        rapidjson::Document d;
        d.SetObject();
        d.GetObject().AddMember("selected", state.selected, d.GetAllocator());
        rapidjson::Value patterns_arr(rapidjson::kArrayType);
        for (auto &kv: state.patterns) {
            rapidjson::Value o(rapidjson::kObjectType);
            o.GetObject().AddMember("i", (int) kv.first, d.GetAllocator());
            o.GetObject().AddMember("pattern", pattern_to_json(kv.second), d.GetAllocator());
            patterns_arr.PushBack(o, d.GetAllocator());
        }
        d.GetObject().AddMember("patterns", patterns_arr, d.GetAllocator());
        return d;
    }

    [[nodiscard]] std::string State::to_json_string() const {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        state_to_json(*this).Accept(writer);
        return buffer.GetString();
    }

}
