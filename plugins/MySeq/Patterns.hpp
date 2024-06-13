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

#include "TimePositionCalc.hpp"

namespace myseq {

    void test_serialize();

    namespace utils {
        static uint8_t row_index_to_midi_note(std::size_t row) {
            assert(row >= 0 && row <= 127);
            return 127 - row;
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
        bool selected;
    };

    struct Note {
        uint8_t note;
        uint8_t channel;

        Note() : note(0), channel(0) {}

        Note(uint8_t note, uint8_t channel) : note(note), channel(channel) {}

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
        int id;
        int width;
        int height;
        int first_note;
        int last_note;

        explicit Pattern(int id) : id(id), width(32), height(128), first_note(0), last_note(127) {
            data.resize(width * height);
            d_debug("HERE 2 size=%d", (int) data.size());
        }

        Pattern(int id, int width, int height, int first_note, int last_note) : id(id), width(width), height(height),
                                                                                first_note(first_note),
                                                                                last_note(last_note) {
            data.resize(width * height);
            d_debug("HERE 2 size=%d", (int) data.size());
        }

        Cell &get_cell(const V2i &v) {
            assert(v.x >= 0 && v.x < width && v.y >= 0 && v.y < height);
            return data[v.x * height + v.y];
        }

        [[nodiscard]] const Cell &get_cell(const V2i &v) const {
            assert(v.x >= 0 && v.x < width && v.y >= 0 && v.y < height);
            return data[v.x * height + v.y];
        }

        void set_velocity(const V2i &v, uint8_t velocity) {
            get_cell(v).velocity = velocity;
        }

        void set_selected(const V2i &v, bool selected) {
            get_cell(v).selected = selected;
        }

        void deselect_all() {
            for (auto &c: data)
                c.selected = false;
        }

        void set_on(const V2i &v) {
            get_cell(v).velocity = 127;
        }

        void set_off(const V2i &v) {
            get_cell(v).velocity = 0;
        }

        [[nodiscard]] bool is_on(const V2i &v) const {
            return get_velocity(v) > 0;
        }

        [[nodiscard]] uint8_t get_velocity(const V2i &v) const {
            return get_cell(v).velocity;
        }

        [[nodiscard]] bool get_selected(const V2i &v) const {
            return get_cell(v).selected;
        }

        void resize_width(int new_width) {
            if (new_width <= width) {
                width = new_width;
            } else {
                auto new_data = std::valarray<Cell>(new_width * height);
                for (int i = 0; i < width * height; i++) {
                    new_data[i] = data[i];
                }
                data = new_data;
                width = new_width;
            }
        }
    };

    struct Opaque;

    struct State {
        std::vector<Pattern> patterns;

        [[nodiscard]] const Pattern *first_pattern_with_note(Note note) const {
            auto it = patterns.begin();
            while (it != patterns.end() && !(it->first_note <= note.note && note.note <= it->last_note)) {
                ++it;
            }
            if (it == patterns.end()) {
                return nullptr;
            } else {
                auto b = &*it;
                return b;
            }
        }

        [[nodiscard]] int next_unused_id() {
            int max = -1;
            for (auto &p: patterns) {
                if (p.id > max) {
                    max = p.id;
                }
            }
            return max + 1;
        }

        int selected = -1;

        State() = default;

        [[nodiscard]] Opaque to_json() const;

        [[nodiscard]] std::string to_json_string() const;

        [[nodiscard]] auto num_patterns() const {
            return patterns.size();
        }

        // Its safe to delete as player will stop playing missing IDs
        void delete_pattern(const int id) {
            for (auto it = patterns.begin(); it != patterns.end(); it++) {
                if (it->id == id) {
                    it = patterns.erase(it);
                    if (it == patterns.end()) {
                        if (!patterns.empty()) {
                            it--;
                            selected = it->id;
                        } else {
                            selected = -1;
                        }
                    } else {
                        selected = it->id;
                    }
                    return;
                }
            }
        }

        Pattern &create_pattern() {
            return patterns.emplace_back(next_unused_id());
        }

        Pattern &duplicate_pattern(int id) {
            auto pattern = get_pattern(id);
            pattern.id = next_unused_id();
            return patterns.emplace_back(pattern);
        }

        Pattern &get_selected_pattern() {
            return get_pattern(selected);
        }

        Pattern &get_pattern(int id) {
            for (auto &p: patterns) {
                if (p.id == id) {
                    return p;
                }
            }
            return *(patterns.end());
        }

        [[nodiscard]] const Pattern &get_pattern(int id) const {
            for (const auto &p: patterns) {
                if (p.id == id) {
                    return p;
                }
            }
            return *(patterns.end());
        }

        template<typename F>
        void each_pattern(F f) {
            for (auto &p: patterns) {
                f(p);
            }
        }

        static State from_json_string(const char *s);

    };
}

#endif //MY_PLUGINS_PATTERNS_HPP
