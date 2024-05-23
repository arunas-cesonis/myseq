//
// Created by Arunas on 02/04/2024.
//

#ifndef AUDIO_SAMPLER_DPF_TIMEPOSITIONCALC_HPP
#define AUDIO_SAMPLER_DPF_TIMEPOSITIONCALC_HPP

#include <string>
#include "src/DistrhoDefines.h"
#include "DistrhoDetails.hpp"

namespace myseq {

    struct TimePositionCalc {
        const TimePosition t;
        const double sample_rate;

        [[nodiscard]] double beats_per_bar() const {
            return t.bbt.beatsPerBar > 0.0 ? t.bbt.beatsPerBar : 1.0;
        }

        [[nodiscard]] double beats_per_second() const {
            return t.bbt.beatsPerMinute / 60.0;
        }

        [[nodiscard]] double global_beat() const {
            return (t.bbt.bar - 1.0) * beats_per_bar() + t.bbt.beat - 1.0 + (t.bbt.tick / t.bbt.ticksPerBeat);
        }

        [[nodiscard]] double sixteenth_note_duration_in_seconds() const {
            return 1.0 / (sixteenth_notes_per_beat() * beats_per_second());
        }

        [[nodiscard]] double sixteenth_note_duration_in_frames() const {
            return sample_rate * sixteenth_note_duration_in_seconds();
        }

        [[nodiscard]] double sixteenth_note_duration_in_ticks() const {
            return ticks_per_second() * sixteenth_note_duration_in_seconds();
        }

        [[nodiscard]] double global_second() const {
            return global_beat() / beats_per_second();
        }

        [[nodiscard]] double global_frame() const {
            return global_second() * sample_rate;
        }

        [[nodiscard]] double global_tick() const {
            return global_beat() * t.bbt.ticksPerBeat;
        }

        [[nodiscard]] double global_sixteenth_note() const {
            return sixteenth_notes_per_beat() * global_beat();
        }

        [[nodiscard]] double sixteenth_notes_per_beat() const {
            return 16.0 / beats_per_bar();
        }

        [[nodiscard]] double frames_per_tick() const {
            return sample_rate / ticks_per_second();
        }

        [[nodiscard]] double ticks_per_second() const {
            return beats_per_second() * t.bbt.ticksPerBeat;
        }

        TimePositionCalc(const TimePosition &t, const double &sample_rate) : t(t), sample_rate(sample_rate) {
        }
    };
}

#endif //AUDIO_SAMPLER_DPF_TIMEPOSITIONCALC_HPP
