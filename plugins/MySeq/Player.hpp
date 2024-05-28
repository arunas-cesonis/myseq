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
    };

    struct ActiveNotes {
        std::map<Note, double> m;

        template<typename F>
        void play_note(F note_event, uint8_t note, uint8_t velocity, double start_time, double end_time) {
            Note note1 = {note, 0};
            auto active = m.find(note1);
            if (active != m.end()) {
                note_event(active->first.note, 0.0, start_time);
                m.erase(active);
            }
            m[note1] = end_time;
            note_event(note, velocity, start_time);
        }

        template<typename F>
        void handle_note_offs(F note_event, const TimeParams &tp) {
            for (auto it = m.begin(); it != m.end();) {
                double t = it->second - tp.time;
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
        double start_time{};
        double end_time{};
        bool finished{};

        ActivePattern() = default;

        ActivePattern(double start_time, double end_time, bool finished) : start_time(start_time), end_time(end_time),
                                                                           finished(finished) {}
    };


    struct Player {
        std::map<Note, ActivePattern> active_patterns;
        std::map<Note, double> active_notes;
        ActiveNotes an;

        Player() = default;

        void start_pattern(const Note &note, double start_time) {
            d_debug("start pattern note=%02x start_time=%f", note.note, start_time);
            active_patterns[note] = ActivePattern(start_time, 0.0, false);
        }

        void stop_pattern(const Note &note, double end_time) {
            auto &ap = active_patterns[note];
            assert(!ap.finished);
            d_debug("end pattern note=%02x start_time=%f end_time=%f duration=%f", note.note, ap.start_time,
                    ap.end_time,
                    ap.end_time - ap.start_time);
            ap.end_time = end_time;
            ap.finished = true;
        }

        template<typename F>
        void run(F note_event, const myseq::State &state, const TimeParams &tp) {
            if (tp.playing) {
                for (auto it = active_patterns.begin(); it != active_patterns.end();) {
                    auto &ap = *it;
                    const auto &p = state.get_pattern(ap.first.note);
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
                        d_debug("attempt playing step=%zu in pattern", column_index);
                        auto column_time = static_cast<double>(i) * tp.step_duration - pattern_time;
                        for (int row_index = 0; row_index < p.height; row_index++) {
                            const auto v = p.get_velocity(V2i(column_index, row_index));
                            if (v > 0.0) {
                                an.play_note(note_event, utils::row_index_to_midi_note(row_index),
                                             v,
                                             column_time,
                                             window_start + column_time + tp.step_duration
                                );
                            }
                        }
                    }
                }
                an.handle_note_offs(note_event, tp);
            } else {
                an.stop_notes(note_event);
            }
        }
    };

    struct Test {
        static void test_player_run() {
            Player player;
            State seq;

            const auto pp = seq.create_pattern();
            auto &p = seq.get_pattern(pp.id);
            for (int i = 0; i < p.height; i++) {
                for (int j = 0; j < p.width; j++) {
                    p.set_on(V2i(j, i));
                }
            }

            std::ostringstream os;
            for (int i = 0; i < p.height; i++) {
                for (int j = 0; j < p.width; j++) {
                    os << (p.is_on(V2i(j, i)) ? "X" : ".");
                    p.set_on(V2i(j, i));
                }
                os << std::endl;
            }
            //std::cout << os.str() << std::endl;
            //std::cout << seq.to_json_string() << std::endl;
            double time;
            double step_duration;
            double window;
            bool playing;
            TimeParams tp;
            tp.time = 0.0;
            tp.step_duration = 100.0;
            tp.window = 2000.0;
            tp.playing = true;
            player.start_pattern(Note{0, 0}, 0.0);
            player.run([](uint8_t note, uint8_t velocity, double time) {
                //   std::cout << "note=" << (int) note << " velocity=" << (int) velocity << " time=" << time << std::endl;
            }, seq, tp);
            // seq.get_pattern(0).set_cell_active(0, 0, true);
        }

    };
}

#endif //MY_PLUGINS_PLAYER_HPP
