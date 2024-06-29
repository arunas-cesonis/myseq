//
// Created by Arunas on 29/06/2024.
//

#include "Stats.hpp"

namespace myseq {
    Transport transport_from_time_position(const TimePosition &tp) {
        return {
                .valid = tp.bbt.valid,
                .playing = tp.playing,
                .frame = tp.frame,
                .bar = tp.bbt.bar,
                .beat = tp.bbt.beat,
                .tick = tp.bbt.tick,
                .bar_start_tick = tp.bbt.barStartTick,
                .beats_per_bar = tp.bbt.beatsPerBar,
                .beat_type = tp.bbt.beatType,
                .ticks_per_beat = tp.bbt.ticksPerBeat,
                .beats_per_minute = tp.bbt.beatsPerMinute
        };
    }

}
