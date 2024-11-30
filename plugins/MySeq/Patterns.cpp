//
// Created by Arunas on 23/05/2024.
//

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"

#include "Patterns.hpp"

namespace myseq {

    [[nodiscard]] V2f v2f_from_json(const rapidjson::Value &value) {
        return {value["x"].GetFloat(), value["y"].GetFloat()};
    }

    [[nodiscard]] rapidjson::Value
    v2f_to_json(const V2f &v, rapidjson::Document::AllocatorType &allocator) {
        rapidjson::Value pobj(rapidjson::kObjectType);
        pobj.AddMember("x", v.x, allocator)
                .AddMember("y", v.y, allocator);
        return pobj;
    }

    Pattern pattern_from_json(const rapidjson::Value &value) {
        auto id = value["id"].GetInt();
        auto width = value["width"].GetInt();
        auto height = value["height"].GetInt();
        auto first_note = value["first_note"].GetInt();
        auto last_note = value["last_note"].GetInt();
        auto default_velocity = value.HasMember("default_velocity") ? value["default_velocity"].GetInt() : 127;
        auto speed = value.HasMember("speed") ? value["speed"].GetFloat() : 1.0;
        auto carr = value["cells"].GetArray();
        V2f viewport = value.HasMember("viewport") ?
                       v2f_from_json(value["viewport"]) : V2f(0.0, 0.0);
        int cursor_x = value.HasMember("cursor_x") ?
                       value["cursor_x"].GetInt() : 0;
        int cursor_y = value.HasMember("cursor_y") ?
                       value["cursor_y"].GetInt() : 0;
        Pattern p(id, width, height, first_note, last_note, V2i(cursor_x, cursor_y));
        p.set_speed((float) speed);
        p.set_default_velocity((uint8_t) default_velocity);
        p.set_viewport(viewport);
        for (int i = 0; i < (int) carr.Size(); i++) {
            auto cobj = carr[i].GetObject();
            int x = cobj["x"].GetInt();
            int y = cobj["y"].GetInt();
            const auto v = V2i(x, y);
            auto velocity = static_cast<uint8_t>(cobj["v"].GetInt());
            bool selected = cobj.HasMember("s") ? (cobj["s"].GetBool()) : false;
            int length = cobj.HasMember("n") ? (cobj["n"].GetInt()) : 1;
            p.set_velocity(v, velocity, "JSON");
            p.set_selected(v, selected);
            p.set_length(v, length);
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
        state.set_selected_id(d["selected"].GetInt());
        state.play_selected = d.HasMember("play_selected") ? d["play_selected"].GetBool() : false;
        state.settings = d.HasMember("settings") ? d["settings"].GetString() : "";
        state.play_note_triggered = d.HasMember("play_note_triggered") ? d["play_note_triggered"].GetBool() : false;
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
        state.set_selected_id(a.id);
        state.get_pattern(a.id).set_velocity(V2i(0, 1), 100);
        state.get_pattern(b.id).set_velocity(V2i(2, 3), 99);
        state.get_pattern(b.id).set_velocity(V2i(3, 4), 102);
        state.get_pattern(b.id).set_velocity(V2i(5, 6), 103);

        const auto s = state.to_json_string();
        State state1 = State::from_json_string(s.c_str());

        assert(state.get_selected_id() == state1.get_selected_id());
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
                .AddMember("first_note", pattern.get_first_note(), allocator)
                .AddMember("last_note", pattern.get_last_note(), allocator)
                .AddMember("cursor_x", pattern.cursor.x, allocator)
                .AddMember("cursor_y", pattern.cursor.y, allocator)
                .AddMember("speed", pattern.get_speed(), allocator)
                .AddMember("default_velocity", pattern.get_default_velocity(), allocator)
                .AddMember("viewport", v2f_to_json(pattern.get_viewport(), allocator), allocator);
        //d.GetObject().AddMember("data", height, d.GetAllocator());
        rapidjson::Value data_arr(rapidjson::kArrayType);
        pattern.each_cell([&](const Cell &cell) {
            rapidjson::Value o(rapidjson::kObjectType);
            auto ob = o.GetObject();
            ob.AddMember("x", cell.position.x, allocator)
                    .AddMember("y", cell.position.y, allocator)
                    .AddMember("v", (int) cell.velocity, allocator);
            if (cell.selected) {
                ob.AddMember("s", (bool) cell.selected, allocator);
            }
            if (cell.length > 1) {
                ob.AddMember("n", cell.length, allocator);
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
        d.GetObject().AddMember("play_note_triggered", this->play_note_triggered, d.GetAllocator());
        d.GetObject().AddMember("settings", rapidjson::StringRef(this->settings.c_str()), d.GetAllocator());
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
