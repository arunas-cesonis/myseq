//
// Created by Arunas on 22/05/2024.
//

#ifndef MY_PLUGINS_PATTERNS_HPP
#define MY_PLUGINS_PATTERNS_HPP

#include <valarray>
#include <optional>
#include <map>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace myseq {

    struct V2i {
        int x;
        int y;

        V2i(int x, int y) : x(x), y(y) {}

        V2i() : V2i(0, 0) {}

        bool operator==(const V2i &other) const {
            return x == other.x && y == other.y;
        }
    };

    struct Cell {
        uint8_t velocity;
    };

    struct Pattern {
        std::valarray<Cell> data;
        int width;
        int height;

        Pattern() : width(16), height(16) {
            data.resize(width * height);
        }

        Pattern(int width, int height) : width(width), height(height) {
            data.resize(width * height);
        }

        void set_velocity(const V2i &v, uint8_t velocity) {
            data[v.y * width + v.x].velocity = velocity;
        }

        uint8_t get_velocity(const V2i &v) const {
            return data[v.y * width + v.x].velocity;
        }


        static Pattern from_json(const rapidjson::Value &value) {
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

        [[nodiscard]] rapidjson::Document to_json() const {
            rapidjson::Document d;
            d.SetObject();
            d.GetObject().AddMember("width", width, d.GetAllocator());
            d.GetObject().AddMember("height", height, d.GetAllocator());
            //d.GetObject().AddMember("data", height, d.GetAllocator());
            rapidjson::Value data_arr(rapidjson::kArrayType);
            for (int i = 0; i < data.size(); i++) {
                const auto &cell = data[i];
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
    };

    struct State {
        std::map<int, Pattern> patterns;
        int selected;

        State() {
            selected = 0;
            create_pattern(0);
        }

        Pattern &create_pattern(int id) {
            patterns[id] = Pattern();
            return patterns[id];
        }

        Pattern &get_create_if_not_exists_pattern(int id) {
            auto it = patterns.find(id);
            if (it == patterns.end()) {
                return create_pattern(id);
            } else {
                return it->second;
            }
        }

        Pattern &get_selected_pattern() {
            return get_pattern(selected);
        }

        Pattern &get_pattern(int id) {
            return patterns.find(id)->second;
        }

        static State from_json_string(const char *s) {
            rapidjson::Document d;
            d.Parse(s);
            auto arr = d["patterns"].GetArray();
            auto selected = d["selected"].GetInt();
            State state;
            for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
                auto obj = arr[i].GetObject();
                int index = obj["i"].GetInt();
                auto pobj = obj["pattern"].GetObject();
                state.patterns[index] = Pattern::from_json(pobj);
            }
            return state;
        }

        [[nodiscard]] rapidjson::Document to_json() const {
            rapidjson::Document d;
            d.SetObject();
            d.GetObject().AddMember("selected", selected, d.GetAllocator());
            rapidjson::Value patterns_arr(rapidjson::kArrayType);
            for (auto &kv: patterns) {
                rapidjson::Value o(rapidjson::kObjectType);
                o.GetObject().AddMember("i", (int) kv.first, d.GetAllocator());
                o.GetObject().AddMember("pattern", kv.second.to_json(), d.GetAllocator());
                patterns_arr.PushBack(o, d.GetAllocator());
            }
            d.GetObject().AddMember("patterns", patterns_arr, d.GetAllocator());
            return d;
        }

        [[nodiscard]] std::string to_json_string() const {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            to_json().Accept(writer);
            return buffer.GetString();
        }
    };
}

#endif //MY_PLUGINS_PATTERNS_HPP
