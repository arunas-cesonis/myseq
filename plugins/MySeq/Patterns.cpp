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
        auto carr = value["cells"].GetArray();
        int cursor_x = value.HasMember("cursor_x") ?
                       value["cursor_x"].GetInt() : 0;
        int cursor_y = value.HasMember("cursor_y") ?
                       value["cursor_y"].GetInt() : 0;
        Pattern p(id, width, height, first_note, last_note, V2i(cursor_x, cursor_y));
        for (int i = 0; i < (int) carr.Size(); i++) {
            auto cobj = carr[i].GetObject();
            int x = cobj["x"].GetInt();
            int y = cobj["y"].GetInt();
            const auto v = V2i(x, y);
            auto velocity = static_cast<uint8_t>(cobj["v"].GetInt());
            bool selected = cobj.HasMember("s") ? static_cast<uint8_t>(cobj["s"].GetBool()) : false;
            p.set_velocity(v, velocity, "JSON");
            p.set_selected(v, selected);
        }
        return p;
    }

    State State::from_json_string(const char *s) {
        d_debug("A %d", __LINE__);
        rapidjson::Document d;
        d_debug("A %d", __LINE__);
        rapidjson::ParseResult ok = d.Parse(s);
        d_debug("A %d", __LINE__);
        if (!ok) {
            fprintf(stderr, "JSON parse error: %s (%lu)",
                    rapidjson::GetParseError_En(ok.Code()), ok.Offset());
            exit(EXIT_FAILURE);
        }
        d_debug("A %d", __LINE__);
        auto arr = d["patterns"].GetArray();
        State state;
        d_debug("A %d", __LINE__);
        state.selected = d["selected"].GetInt();
        d_debug("A %d", __LINE__);
        state.play_selected = d.HasMember("play_selected") ? d["play_selected"].GetBool() : false;
        d_debug("A %d", __LINE__);
        for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
            auto obj = arr[i].GetObject();
            state.patterns.push_back(pattern_from_json(obj));
            d_debug("A %d", __LINE__);
        }
        d_debug("A %d", __LINE__);
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
                .AddMember("last_note", pattern.last_note, allocator)
                .AddMember("cursor_x", pattern.cursor.x, allocator)
                .AddMember("cursor_y", pattern.cursor.y, allocator);
        //d.GetObject().AddMember("data", height, d.GetAllocator());
        rapidjson::Value data_arr(rapidjson::kArrayType);
        pattern.each_cell([&](const Cell &cell, const V2i &coords) {
            rapidjson::Value o(rapidjson::kObjectType);
            auto ob = o.GetObject();
            assert(cell.velocity > 0);
            ob.AddMember("x", coords.x, allocator)
                    .AddMember("y", coords.y, allocator)
                    .AddMember("v", (int) cell.velocity, allocator);
            if (cell.selected) {
                ob.AddMember("s", (bool) cell.selected, allocator);
            }
            data_arr.PushBack(o, allocator);
        });
        pobj.GetObject().AddMember("cells", data_arr, allocator);
        return pobj;
    }

    struct Opaque {
        rapidjson::Document d;
    };

    [[nodiscard]] Opaque State::to_json() const {
        rapidjson::Document d;
        d.SetObject();
        d.GetObject().AddMember("selected", this->selected, d.GetAllocator());
        d.GetObject().AddMember("play_selected", this->play_selected, d.GetAllocator());
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
