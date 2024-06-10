//
// Created by Arunas on 22/05/2024.
//

#ifndef MY_PLUGINS_PLAYER_HPP
#define MY_PLUGINS_PLAYER_HPP

#include <optional>
#include <sstream>
#include "Patterns.hpp"
#include "TimePositionCalc.hpp"

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

    struct NotePlayer {
        std::map<Note, double> vector;

    };

    struct ActivePattern {
        int pattern_id{};
        double start_time{};
        double end_time{};
        bool finished{};
        Note note;

        ActivePattern() = default;

        ActivePattern(int pattern_id, double start_time, double end_time, bool finished, Note note) : pattern_id(
                pattern_id),
                                                                                                      start_time(
                                                                                                              start_time),
                                                                                                      end_time(
                                                                                                              end_time),
                                                                                                      finished(
                                                                                                              finished),
                                                                                                      note(note) {}
    };


    struct Player {
        std::map<Note, ActivePattern> active_patterns;
        ActiveNotes an = ActiveNotes();

        Player() = default;

        void start_pattern(const State &state, const Note &note, double start_time, const TimeParams &tp) {
            // 1. Find first pattern that has note in the range
            auto it = state.patterns.begin();
            while (it != state.patterns.end() && !(it->first_note <= note.note && note.note <= it->last_note)) {
                ++it;
            }
            if (it == state.patterns.end()) {
                return;
            }
            const auto total_notes = it->last_note - it->first_note + 1;
            const auto percent_from_start = (float) (note.note - it->first_note) / (float) total_notes;
            const double pattern_duration = tp.step_duration * static_cast<double>(it->width);
            const auto start_time_offset = percent_from_start * pattern_duration;
            const auto new_start_time = start_time - start_time_offset;
            active_patterns[note] = ActivePattern(it->id, new_start_time, 0.0, false, note);
        }

        void stop_pattern(const Note &note, double end_time) {
            auto &ap = active_patterns[note];
            if (ap.finished) {
                return;
            }
            ap.end_time = end_time;
            ap.finished = true;
        }

        template<typename F>
        void run(F note_event, const myseq::State &state, const TimeParams &tp, double frames_per_tick) {
            if (tp.playing) {
                for (auto it = active_patterns.begin(); it != active_patterns.end();) {
                    auto &ap = *it;
                    const auto &p = state.get_pattern(ap.second.pattern_id);
                    double window_start = tp.time;
                    double window_end = ap.second.finished ? std::min(window_start + tp.window, ap.second.end_time) :
                                        window_start + tp.window;

                    if (window_start >= window_end) {
                        it = active_patterns.erase(it);
                        continue;
                    } else {
                        ++it;
                    }

                    double pattern_elapsed = window_start - ap.second.start_time;
                    double pattern_duration = tp.step_duration * static_cast<double>(p.width);
                    double pattern_time = std::fmod(pattern_elapsed, pattern_duration);
                    auto next_column = static_cast<int>(std::ceil(
                            std::fmod(pattern_elapsed, pattern_duration) / tp.step_duration));

                    auto last_column = static_cast<int>(std::floor(
                            (pattern_time + tp.window) / tp.step_duration));

                    if (next_column < 0 || last_column < 0) {
                        continue;
                    }

                    for (auto i = next_column; i <= last_column; i++) {
                        const auto column_index = i % p.width;
                        auto column_time = static_cast<double>(i) * tp.step_duration - pattern_time;
                        for (int row_index = 0; row_index < p.height; row_index++) {
                            const auto v = p.get_velocity(V2i(column_index, row_index));
                            if (v > 0.0) {
                                const auto step_end_time = window_start + column_time + tp.step_duration;
                                const auto note_end_time = ap.second.finished ? std::min(step_end_time,
                                                                                         ap.second.end_time)
                                                                              : step_end_time;

                                // This check prevents 0-length notes being played when input note ends
                                // exactly at the end of the step and just before the beggining of the next step
                                // I believe this is caused by either note starting time calculation or
                                // pattern end time calculation being incorrect (or both)
                                const auto note_length = note_end_time - (window_start + column_time);
                                if (note_length > (ap.second.finished ? 1.0 : 0.0)) {
                                    if (ap.second.finished) {
                                        d_debug("FINISHED NOTE ON iteration=%d note=%d time=%f note_length=%f",
                                                tp.iteration,
                                                ap.second.note.note,
                                                tp.time + column_time,
                                                note_length);
                                    }
                                    an.play_note(note_event, utils::row_index_to_midi_note(row_index),
                                                 v,
                                                 column_time,
                                                 note_end_time
                                    );
                                }
                            }
                        }
                    }
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
            p.first_note = 0;
            p.last_note = 15;
            std::cout << "p.id=" << p.id << std::endl;
            std::cout << "p.first_note=" << p.first_note << std::endl;
            std::cout << "p.last_note=" << p.last_note << std::endl;
            Player player;
            TimeParams tp;
            tp.step_duration = 1.0;
            tp.window = 5.0;
            tp.time = 0.0;
            tp.playing = true;

            player.start_pattern(state, Note{(uint8_t) p.first_note, 0}, 0.0, tp);
            player.start_pattern(state, Note{(uint8_t) 10, 0}, 0.0, tp);

            player.run([](uint8_t note, double velocity, double time) {
                std::cout << "note=" << (int) note << " velocity=" << velocity << " time=" << time << std::endl;
            }, state, tp, 100.0);

            std::cout << "END TEST\n";
        }
    };
}

#endif //MY_PLUGINS_PLAYER_HPP
