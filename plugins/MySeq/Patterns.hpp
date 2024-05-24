//
// Created by Arunas on 22/05/2024.
//

#ifndef MY_PLUGINS_PATTERNS_HPP
#define MY_PLUGINS_PATTERNS_HPP

#include <map>
#include <valarray>
#include <sstream>
#include <iostream>
#include <cassert>

#include <optional>
#include "src/DistrhoDefines.h"

#include "optional.hpp"
#include "TimePositionCalc.hpp"

namespace myseq {

    void test_serialize();

    namespace utils {
        static uint8_t row_index_to_midi_note(std::size_t row) {
            assert(row >= 0 && row <= 127);
            return row;
        }
    }

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

    struct Note {
        uint8_t note;
        uint8_t channel;

        bool operator<(const Note &other) const {
            if (channel < other.channel) {
                return true;
            } else if (channel > other.channel) {
                return false;
            } else {
                return note < other.note;
            }
        }
    };


    struct NoteMessage {
        enum class Type {
            NoteOn,
            NoteOff,
        };
        Type type;
        Note note;
        uint8_t velocity;

        [[nodiscard]] static std::optional<NoteMessage> parse(const uint8_t (&data)[4]) {
            const auto msg_type = static_cast<uint8_t>(data[0] & 0xf0);
            const auto channel = static_cast<uint8_t >(data[0] & 0x0f);
            if (msg_type == 0x90 || msg_type == 0x80) {
                const auto note_num = data[1];
                const auto velocity = data[2];
                const auto note = Note{note_num, channel};
                return std::optional<NoteMessage>(
                        {msg_type == 0x90 && velocity > 0 ? Type::NoteOn : Type::NoteOff, note, velocity});
            }
            return {};
        }
    };

    struct Pattern {
        std::valarray<Cell> data;
        int width;
        int height;

        Pattern() : width(32), height(128) {
            data.resize(width * height);
        }

        Pattern(int width, int height) : width(width), height(height) {
            data.resize(width * height);
        }

        Cell &get_cell(const V2i &v) {
            return data[v.x * height + v.y];
        }

        const Cell &get_cell(const V2i &v) const {
            return data[v.x * height + v.y];
        }

        void set_velocity(const V2i &v, uint8_t velocity) {
            get_cell(v).velocity = velocity;
        }

        void set_on(const V2i &v) {
            get_cell(v).velocity = 127;
        }

        void set_off(const V2i &v) {
            get_cell(v).velocity = 0;
        }

        bool is_on(const V2i &v) const {
            return get_velocity(v) > 0;
        }

        uint8_t get_velocity(const V2i &v) const {
            return get_cell(v).velocity;
        }

        void resize_width(int new_width) {
            data.resize(new_width * height);
            width = new_width;
        }
    };

    struct State {
        std::map<int, Pattern> patterns;
        int selected;

        State() {
            selected = 0;
            create_pattern(0);
        }

        int next_unused_id(int id) const {
            while (patterns.find(++id) != patterns.end());
            return id;
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

        const Pattern &get_pattern(int id) const {
            return patterns.find(id)->second;
        }


        template<typename F>
        void each_pattern_id(F f) {
            for (auto &kv: patterns) {
                f(kv.first);
            }
        }

        static State from_json_string(const char *s);

        [[nodiscard]] std::string to_json_string() const;
    };
}

#endif //MY_PLUGINS_PATTERNS_HPP
