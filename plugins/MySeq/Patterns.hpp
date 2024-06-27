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
#include "GenArray.hpp"

namespace myseq {


    struct Opaque;

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

        bool operator!=(const V2i &other) const {
            return x != other.x || y != other.y;
        }

        V2i operator-(const V2i &other) const {
            return {x - other.x, y - other.y};
        }

        V2i operator+(const V2i &other) const {
            return {x + other.x, y + other.y};
        }
    };

    struct V2iHash {
        std::size_t operator()(const V2i &v) const {
            return std::hash<int>()(v.x) ^ std::hash<int>()(v.y);
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

    class Pattern {
        GenArray<std::pair<V2i, Cell>> cells;
        std::valarray<Id> grid;

        [[nodiscard]] V2i index_to_coords(int index) const {
            assert(index < width * height);
            const auto x = index / height;
            const auto y = index % height;
            return {x, y};
        }

        [[nodiscard]] int coords_to_index(const V2i &v) const {
            assert(v.x >= 0 && v.x < width);
            assert(v.y >= 0 && v.y < height);
            return v.x * height + v.y;
        }

    public:
        int id;
        int width;
        int height;
        int first_note;
        int last_note;
        V2i cursor;

        explicit Pattern(int id) : id(id), width(32), height(128), first_note(0), last_note(127) {
            grid.resize(width * height);
        }

        Pattern(int id, int width, int height, int first_note, int last_note, const V2i &cursor) : id(id), width(width),
                                                                                                   height(height),
                                                                                                   first_note(
                                                                                                           first_note),
                                                                                                   last_note(last_note),
                                                                                                   cursor(cursor) {
            grid.resize(width * height);
        }

        [[nodiscard]] const std::string debug_print() const {
            std::ostringstream oss;
            int n = 0;
            oss << "[";
            for (const auto pair: cells) {
                oss << "(" << pair.first.x << ", " << pair.first.y << ", " << (int) pair.second.velocity << ", "
                    << pair.second.selected << ")";
            }
            oss << "]";
            return oss.str();
        }

        template<typename F>
        void each_cell(F f) const {
            for (const auto &c: cells) {
                const auto &loc = c.first;
                const auto &cell = c.second;
                f(cell, loc);
            }
        }

        template<typename F>
        void each_selected_cell(F f) const {
            each_cell([&f](const Cell &cell, const V2i &loc) {
                if (cell.selected) {
                    f(cell, loc);
                }
            });
        }

        [[nodiscard]] bool exists(const V2i &coords) const {
            return cells.exist(grid[coords_to_index(coords)]);
        }

        Cell &get_create_if_not_exists(const V2i &coords) {
            const auto idx = coords_to_index(coords);
            const auto &cell_id = grid[idx];
            if (cells.exist(cell_id)) {
                return cells.get(cell_id).second;
            } else {
                const auto new_id = cells.push({coords, {}});
                grid[coords_to_index(coords)] = new_id;
                return cells.get(new_id).second;
            }
        }

        void set_cell(const V2i &coords, const Cell &c) {
            get_create_if_not_exists(coords) = c;
        }

        [[nodiscard]] const Cell &get_cell(const V2i &coords) const {
            return cells.get(grid[coords_to_index(coords)]).second;
        }

        Cell &get_cell(const V2i &coords) {
            return cells.get(grid[coords_to_index(coords)]).second;
        }

        void clear_cell(const V2i &coords) {
            cells.remove(grid[coords_to_index(coords)]);
        }

        void move_selected_cells(const V2i &delta) {
            std::vector<std::pair<Cell, V2i>> removed;
            each_selected_cell([&](const Cell &cell, const V2i &v) {
                removed.emplace_back(cell, v);
                clear_cell(v);
            });
            for (const auto &pair: removed) {
                auto new_coords = pair.second + delta;
                // wrap around
                new_coords.x = new_coords.x % width;
                new_coords.x += new_coords.x < 0 ? width : 0;
                new_coords.y = new_coords.y % height;
                new_coords.y += new_coords.y < 0 ? height : 0;
                set_cell(new_coords, pair.first);
            }
        }

        int deselect_all() {
            int count = 0;
            for (auto &c: cells) {
                if (c.second.selected) {
                    c.second.selected = false;
                    count++;
                }

            }
            return count;
        }

        void set_velocity(const V2i &v, uint8_t velocity) {
            if (velocity == 0) {
                if (exists(v))
                    clear_cell(v);
                return;
            }
            get_create_if_not_exists(v).velocity = velocity;
        }

        [[nodiscard]] bool is_active(const V2i &v) const {
            if (exists(v)) {
                return get_cell(v).velocity > 0;
            } else {
                return false;
            }
        }

        [[nodiscard]] uint8_t get_velocity(const V2i &v) const {
            if (exists(v)) {
                return get_cell(v).velocity;
            } else {
                return 0;
            }
        }

        void set_selected(const V2i &v, bool selected) {
            if (exists(v)) {
                get_cell(v).selected = selected;
            }
        }

        void select_all() {
            for (auto &c: cells)
                c.second.selected = true;
        }

        [[nodiscard]] bool get_selected(const V2i &v) const {
            if (exists(v)) {
                return get_cell(v).selected;
            } else {
                return false;
            }
        }


        void resize_width(int new_width) {
            auto new_grid = std::valarray<Id>(new_width * height);
            const auto n = std::min(new_grid.size(), grid.size());
            for (std::size_t i = 0; i < n; i++) {
                new_grid[i] = grid[i];
            }
            grid = new_grid;
            width = new_width;
        }


        [[nodiscard]] int get_last_note() const {
            return last_note;
        }

        [[nodiscard]] int get_id() const {
            return id;
        }

        [[nodiscard]] int get_first_note() const {
            return first_note;
        }

        [[nodiscard]] int get_width() const {
            return width;
        }

        [[nodiscard]] int get_height() const {
            return height;
        }
    };

    struct Opaque;

    struct State {
        std::vector<Pattern> patterns;

        State() = default;

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

        //State() = default;

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

        // chooses a range of 16 notes that start with unused note
        // or just returns the lowest range of 16 notes
        // this allows creation of ~7 patterns that
        // are conveniently placed at the beginning of octaves
        // by far not ideal solution; need to use the sequencer more to
        // understand what is the best way to handle this
        [[nodiscard]] std::pair<int, int> try_find_free_16_range() const {
            int used[128]{};
            for (auto &p: patterns) {
                d_debug("pattern %d %d %d", p.id, p.first_note, p.last_note);
                for (int i = p.first_note; i <= p.last_note; i++) {
                    used[i]++;
                }
            }
            const int convenient_starts[] = {0, 24, 48, 72, 96};
            for (int convenient_start: convenient_starts) {
                if (used[convenient_start] == 0) {
                    return {convenient_start, convenient_start + 15};
                }
            }
            return {0, 15};
        }

        Pattern &create_pattern() {
            Pattern p = Pattern(next_unused_id());
            const auto range = try_find_free_16_range();
            p.first_note = range.first;
            p.last_note = range.second;
            patterns.push_back(p);
            d_debug("credate %d %d", p.first_note, p.last_note);
            return patterns.back();
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
