//
// Created by Arunas on 22/05/2024.
//

#ifndef MY_PLUGINS_PATTERNS_HPP
#define MY_PLUGINS_PATTERNS_HPP

#include <valarray>
#include <optional>
#include <map>

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

        void set_velocity(const V2i &v, uint8_t velocity) {
            data[v.y * width + v.x].velocity = velocity;
        }

        uint8_t get_velocity(const V2i &v) const {
            return data[v.y * width + v.x].velocity;
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
    };
}

#endif //MY_PLUGINS_PATTERNS_HPP
