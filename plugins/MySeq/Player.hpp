//
// Created by Arunas on 22/05/2024.
//

#ifndef MY_PLUGINS_PLAYER_HPP
#define MY_PLUGINS_PLAYER_HPP

#include <optional>
#include <sstream>
#include "MyAssert.hpp"
#include "Patterns.hpp"
#include "TimePositionCalc.hpp"
#include "Stats.hpp"

namespace myseq {
    struct TimeParams {
        double time;
        double step_duration;
        double window;
        bool playing;
        int iteration;
    };

    struct ActiveNoteData {
        double end_time;
    };

    struct ActiveNotes {
        std::map<Note, ActiveNoteData> m;

        template<typename F>
        void
        play_note(F note_event, uint8_t note, uint8_t velocity, double start_time, double end_time) {
            Note note1 = {note, 0};
            auto active = m.find(note1);
            if (active != m.end()) {
                note_event(active->first.note, 0.0, start_time);
                m.erase(active);
            }
            m[note1] = {end_time};
            note_event(note, velocity, start_time);
        }

        template<typename F>
        void handle_note_offs(F note_event, const TimeParams &tp) {
            for (auto it = m.begin(); it != m.end();) {
                double t = it->second.end_time - tp.time;
                if (t < tp.window) {
                    note_event(it->first.note, 0.0, t);
                    it = m.erase(it);
                } else {
                    ++it;
                }
            }
        }

        template<typename F>
        void stop_notes(F note_event) {
            for (auto it: m) {
                note_event(it.first.note, 0.0, 0.0);
            }
            m.clear();
        }
    };

    struct ActivePattern {
        int pattern_id;
        double start_time;
        double end_time;
        bool finished;
        Note note;
        uint8_t velocity;
        double last_elapsed;
        double last_duration;
    };


    struct Player {
        std::vector<ActivePattern> active_patterns;
        std::optional<ActivePattern> selected_active_pattern;
        ActiveNotes an = ActiveNotes();

        Player() = default;

        static double pattern_start_time_offset(const Pattern &p, const Note &note, const TimeParams &tp) {
            const auto total_notes = p.get_last_note() - p.get_first_note() + 1;
            const auto percent_from_start = (float) (note.note - p.get_first_note()) / (float) total_notes;
            const double pattern_duration = tp.step_duration * static_cast<double>(p.get_width());
            return percent_from_start * pattern_duration;
        }

        void play_selected_pattern(const myseq::State &state) {
            auto &p = state.get_selected_pattern();
            const ActivePattern ap = {p.get_id(), 0.0, 0.0, false, Note(p.get_first_note(), 0), 127, 0.0, 0.0};
            selected_active_pattern = {ap};
        }

        void stop_selected_pattern() {
            selected_active_pattern = {};
        }

        void
        start_note_triggered(const State &state, const Note &note, uint8_t velocity, double start_time,
                             const TimeParams &tp) {
            d_debug("start_note_triggered: %d %d %f", note.note, velocity, start_time);
            active_patterns.erase(
                    std::remove_if(active_patterns.begin(), active_patterns.end(), [&note](auto other) -> bool {
                        return other.note == note;
                    }), active_patterns.end());

            for (auto &p: state.patterns) {
                if (!(p.get_first_note() <= note.note && note.note <= p.get_last_note())) {
                    continue;
                }
                const auto new_start_time = start_time - pattern_start_time_offset(p, note, tp);
                active_patterns.push_back({p.get_id(), new_start_time, 0.0, false, note, velocity});
            }
        }

        void stop_patterns(const Note &note, double end_time) {
            d_debug("stop_patterns: %d %f", note.note, end_time);
            for (auto &ap: active_patterns) {
                if (ap.note == note && !ap.finished) {
                    ap.end_time = end_time;
                    ap.finished = true;
                }
            }
        }

        void stop_note_triggered() {
            active_patterns.clear();
        }

        static uint8_t note_out_velocity(const ActivePattern &ap, uint8_t step_velocity) {
            const auto v2 = std::round(static_cast<double>(step_velocity) * (static_cast<double>(ap.velocity) / 127));
            assert(v2 >= 0.0 && v2 <= 127.0);
            return static_cast<uint8_t >(v2);
        }

        static double calc_pattern_elapsed(const ActivePattern &ap, const TimeParams &tp) {
            return tp.time - ap.start_time;
        }

        static double calc_pattern_duration(const myseq::Pattern &p, const TimeParams &tp) {
            return tp.step_duration * static_cast<double>(p.width);
        }

        void push_active_pattern_stats(myseq::Stats &stats, const myseq::State &state, const TimeParams &tp) {
            for (const auto &ap: active_patterns) {
                const auto &p = state.get_pattern(ap.pattern_id);
                myseq::ActivePatternStats aps = {
                        .pattern_id = ap.pattern_id,
                        .duration = calc_pattern_duration(p, tp),
                        .time = std::fmod(calc_pattern_elapsed(ap, tp), aps.duration)
                };
                stats.active_patterns.push_back(aps);
            }
            if (selected_active_pattern.has_value()) {
                const auto &ap = *selected_active_pattern;
                const auto &p = state.get_pattern(ap.pattern_id);
                myseq::ActivePatternStats aps = {
                        .pattern_id = ap.pattern_id,
                        .duration = calc_pattern_duration(p, tp),
                        .time = std::fmod(calc_pattern_elapsed(ap, tp), aps.duration)
                };
                stats.active_patterns.push_back(aps);
            }
        }

        template<typename F>
        bool
        run_active_pattern(F note_event, const ActivePattern &ap, const myseq::State &state, const TimeParams &tp) {
            const auto &p = state.get_pattern(ap.pattern_id);
            double window_start = tp.time;
            double window_end = ap.finished ? std::min(window_start + tp.window, ap.end_time) :
                                window_start + tp.window;

            if (window_start >= window_end) {
                return false;
            }
            const auto step_duration = tp.step_duration * p.get_speed();

            double pattern_elapsed = window_start - ap.start_time;
            double pattern_duration = step_duration * static_cast<double>(p.width);
            double pattern_time = std::fmod(pattern_elapsed, pattern_duration);
            auto next_column = static_cast<int>(std::ceil(
                    std::fmod(pattern_elapsed, pattern_duration) / step_duration));

            auto last_column = static_cast<int>(std::floor(
                    (pattern_time + tp.window) / step_duration));

            if (next_column < 0 || last_column < 0) {
                return true;
            }

            for (auto i = next_column; i <= last_column; i++) {
                const auto column_index = i % p.width;
                auto column_time = static_cast<double>(i) * step_duration - pattern_time;
                for (int row_index = 0; row_index < p.height; row_index++) {
                    const auto coords = V2i(column_index, row_index);
                    if (p.is_extension_of_tied(coords)) {
                        continue;
                    }
                    const auto v = p.get_velocity(coords);
                    if (v > 0) {
                        const auto length = static_cast<double>(p.get_length(coords));
                        const auto step_end_time = window_start + column_time + step_duration * length;
                        const auto note_end_time = ap.finished ? std::min(step_end_time,
                                                                          ap.end_time)
                                                               : step_end_time;

                        // This check prevents 0-length notes being played when input note ends
                        // exactly at the end of the step and just before the beginning of the next step
                        // I believe this is caused by either note starting time calculation or
                        // pattern end time calculation being incorrect (or both)
                        // 1.0 here represents 1 MIDI tick which is supposed to be the smallest possible note length
                        // however it seems that <1.0 is also a valid note length (at least in REAPER)
                        const auto note_length = note_end_time - (window_start + column_time);
                        if (note_length > (ap.finished ? 1.0 : 0.0)) {
                            if (ap.finished) {
                                d_debug("FINISHED NOTE ON iteration=%d note=%d time=%f note_length=%f",
                                        tp.iteration,
                                        ap.note.note,
                                        tp.time + column_time,
                                        note_length);
                            }
                            an.play_note(note_event, utils::row_index_to_midi_note(row_index),
                                         note_out_velocity(ap, v),
                                         column_time,
                                         note_end_time
                            );
                        }
                    }
                }
            }
            return true;
        }

        template<typename F>
        void run(F note_event, const myseq::State &state, const TimeParams &tp) {
            if (tp.playing) {
                for (auto it = active_patterns.begin(); it != active_patterns.end();) {
                    const auto &ap = *it;
                    if (!run_active_pattern(note_event, ap, state, tp)) {
                        d_debug("REMOVING ACTIVE PATTERN %d", ap.pattern_id);
                        it = active_patterns.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (selected_active_pattern.has_value()) {
                    run_active_pattern(note_event, *selected_active_pattern, state, tp);
                }
                an.handle_note_offs(note_event, tp);
            } else {
                active_patterns.clear();
                an.stop_notes(note_event);
            }
        }
    };

    struct Test {
        static void test_player_run() {
            State state;
            auto &p = state.create_pattern();
            p.set_note_trigger_range(0, 15);
            std::cout << "p.id=" << p.id << std::endl;
            std::cout << "p.first_note=" << p.get_first_note() << std::endl;
            std::cout << "p.last_note=" << p.get_last_note() << std::endl;
            Player player;
            TimeParams tp;
            tp.step_duration = 1.0;
            tp.window = 5.0;
            tp.time = 0.0;
            tp.playing = true;

            player.start_note_triggered(state, Note{(uint8_t) p.get_first_note(), 0}, 127, 0.0, tp);
            player.start_note_triggered(state, Note{(uint8_t) 10, 0}, 127, 0.0, tp);

            player.run([](uint8_t note, double velocity, double time) {
                std::cout << "note=" << (int) note << " velocity=" << velocity << " time=" << time << std::endl;
            }, state, tp);

            std::cout << "END TEST\n";
        }
    };
}

#endif //MY_PLUGINS_PLAYER_HPP
