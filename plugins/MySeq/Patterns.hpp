//
// Created by Arunas on 22/05/2024.
//

#ifndef MY_PLUGINS_PATTERNS_HPP
#define MY_PLUGINS_PATTERNS_HPP

#include <map>
#include <valarray>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <random>
#include <chrono>

#include <optional>
#include "src/DistrhoDefines.h"

#include "MyAssert.hpp"
#include "Utils.hpp"
#include "TimePositionCalc.hpp"
#include "GenArray.hpp"

namespace myseq {


    struct Opaque;

    void test_serialize();

    namespace utils {

        static uint8_t midi_note_to_row_index(std::size_t note) {
            assert(note >= 0 && note <= 127);
            return 127 - note;
        }

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

        void operator-=(const V2i &other) {
            x -= other.x;
            y -= other.y;
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
        V2i position;
        uint8_t velocity;
        bool selected;
        int length;
    };

    struct Note {
        uint8_t note;
        uint8_t channel;

        Note() : note(0), channel(0) {}

        Note(uint8_t note, uint8_t channel) : note(note), channel(channel) {}


        bool ne(const Note &other) const {
            return note != other.note || channel != other.channel;
        }

        bool operator!=(const Note &other) const {
            return ne(other);
        }

        bool operator==(const Note &other) const {
            return !ne(other);
        }

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
        GenArray<Cell> cells;
        std::valarray<Id> grid;

        [[nodiscard]] V2i index_to_coords(int index) const {
            assert(index < width * height);
            const auto x = index / height;
            const auto y = index % height;
            return {x, y};
        }

        [[nodiscard]] bool is_valid_coords(const V2i &v) const {
            return v.x >= 0 && v.x < width && v.y >= 0 && v.y < height;
        }

        [[nodiscard]] int coords_to_index(const V2i &v) const {
            assert(is_valid_coords(v));
            return v.x * height + v.y;
        }


        Cell &get_cell(const V2i &coords) {
            return cells.get(grid[coords_to_index(coords)]);
        }

        const Cell &get_cell(const V2i &coords) const {
            return cells.get(grid[coords_to_index(coords)]);
        }

        Cell &get_create_if_not_exists(const V2i &coords) {
            const auto idx = coords_to_index(coords);
            const auto &cell_id = grid[idx];
            if (cells.exists(cell_id)) {
                return cells.get(cell_id);
            } else {
                const Cell cell = {coords, 127, false, 1};
                const auto new_id = cells.push(cell);
                grid[coords_to_index(coords)] = new_id;
                return cells.get(new_id);
            }
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

        template<typename F>
        void each_cell(F f) const {
            for (const auto &c: cells) {
                f(c);
            }
        }

        template<typename F>
        void each_selected_cell(F f) const {
            each_cell([&f](const Cell &cell) {
                if (cell.selected) {
                    f(cell);
                }
            });
        }

        [[nodiscard]] bool exists(const V2i &coords) const {
            if (!is_valid_coords(coords)) {
                return false;
            }
            return cells.exists(grid[coords_to_index(coords)]);
        }

        void set_cell(const Cell &c) {
            get_create_if_not_exists(c.position);
            set_length(c.position, c.length);
            set_selected(c.position, c.length);
            set_velocity(c.position, c.velocity);
        }

        [[nodiscard]] const Cell &get_cell_const_ref(const V2i &coords) const {
            return cells.get(grid[coords_to_index(coords)]);
        }

        void clear_cell(const V2i &coords) {
            if (exists(coords)) {
                set_length(coords, 1);
                cells.remove(grid[coords_to_index(coords)]);
            }
        }

        void put_cells(const std::vector<Cell> &cells, const V2i &at) {
            for (auto cell: cells) {
                auto wrapped = cell.position + at;
                wrapped.x = wrapped.x % width;
                wrapped.x += wrapped.x < 0 ? width : 0;
                wrapped.y = wrapped.y % height;
                wrapped.y += wrapped.y < 0 ? height : 0;
                cell.position = wrapped;
                set_cell(cell);
            }
        }

        void move_selected_cells(const V2i &delta) {
            std::vector<Cell> removed;
            each_selected_cell([&](const Cell &cell) {
                removed.emplace_back(cell);
                clear_cell(cell.position);
            });
            put_cells(removed, delta);
        }

        int deselect_all() {
            int count = 0;
            for (auto &c: cells) {
                if (c.selected) {
                    c.selected = false;
                    count++;
                }

            }
            return count;
        }

        void set_velocity(const V2i &v, uint8_t velocity, const char *caller_name = nullptr) {
            if (caller_name != nullptr) {
                // d_debug("set_velocity %d %d %d %s", v.x, v.y, velocity, caller_name);
            }
            get_create_if_not_exists(v).velocity = velocity;
        }

        [[nodiscard]] bool is_active(const V2i &v) const {
            return exists(v);
        }

        void set_active(const V2i &v, bool active) {
            if (active) {
                get_create_if_not_exists(v);
            } else {
                clear_cell(v);
            }
        }

        [[nodiscard]] uint8_t get_velocity(const V2i &v) const {
            if (exists(v)) {
                return get_cell(v).velocity;
            } else {
                return 0;
            }
        }

        [[nodiscard]] bool is_extension_of_tied(const V2i &v) const {
            return exists(v) && get_cell(v).position != v;
        }

        void set_selected(const V2i &v, bool selected) {
            if (exists(v)) {
                get_cell(v).selected = selected;
            }
        }

        void set_length(const V2i &v, int length) {
            assert(length >= 1);
            if (exists(v)) {
                Cell &cell = get_cell(v);
                const auto &p = cell.position;
                assert(p.x + length - 1 < this->width);
                while (cell.length > length) {
                    const auto index = coords_to_index(V2i(p.x + cell.length - 1, p.y));
                    grid[index] = Id::null();
                    cell.length--;
                }
                const auto cell_id = grid[coords_to_index(p)];
                while (cell.length < length) {
                    cell.length++;
                    const auto coords = V2i(p.x + cell.length - 1, p.y);
                    clear_cell(coords);
                    grid[coords_to_index(coords)] = cell_id;
                }
            }
        }

        void select_all() {
            for (auto &c: cells)
                c.selected = true;
        }

        [[nodiscard]] int num_selected() const {
            int count = 0;
            for (const auto &c: cells) {
                if (c.selected) count++;
            }
            return count;
        }

        [[nodiscard]] bool get_selected(const V2i &v) const {
            if (exists(v)) {
                return get_cell(v).selected;
            } else {
                return false;
            }
        }

        [[nodiscard]] int get_length(const V2i &v) const {
            if (exists(v)) {
                return get_cell(v).length;
            } else {
                return 0;
            }
        }

        void resize_width(int new_width) {
            auto new_grid = std::valarray<Id>(new_width * height);
            const auto n = std::min(new_grid.size(), grid.size());
            for (std::size_t i = 0; i < n; i++) {
                new_grid[i] = grid[i];
            }

            if (new_width < width) {
                cells.erase(std::remove_if(cells.begin(), cells.end(), [new_width](const auto &cell) -> bool {
                    return cell.position.x >= new_width;
                }), cells.end());
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

        void move_cursor_to_lowest_note() {
            int lowest = 127;
            for (const auto &c: cells) {
                const auto note = utils::row_index_to_midi_note(c.position.y);
                if (note < lowest) {
                    lowest = note;
                }
            }
            cursor.x = 0;
            cursor.y = utils::midi_note_to_row_index(lowest);
        }
    };

    struct Opaque;

    struct State {
    private:
        int selected = -1;
    public:
        std::vector<Pattern> patterns;
        bool play_selected = false;
        bool play_note_triggered = false;

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
            return patterns.back();
        }

        Pattern &duplicate_pattern(int id) {
            auto pattern = get_pattern(id);
            pattern.id = next_unused_id();
            return patterns.emplace_back(pattern);
        }

        void set_selected_id(int id) {
            selected = id;
        }

        [[nodiscard]] int get_selected_id() const {
            return selected;
        }

        Pattern &get_selected_pattern() {
            return get_pattern(selected);
        }

        [[nodiscard]] const Pattern &get_selected_pattern() const {
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

        void write_to_file(const char *state_file) const {
            d_debug("write_file %s", state_file);
            const auto value = to_json_string();
            write_file(state_file, value.c_str(), value.size());
        }

        static std::optional<State> read_from_file(const char *state_file) {
            d_debug("read_file %s", state_file);
            const auto content = read_file(state_file);
            if (content.has_value()) {
                return {myseq::State::from_json_string(content.value().c_str())};
            }
            return {};
        }


    };
}

#endif //MY_PLUGINS_PATTERNS_HPP
