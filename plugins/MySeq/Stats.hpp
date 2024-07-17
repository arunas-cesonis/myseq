//
// Created by Arunas on 29/06/2024.
//

#ifndef MY_PLUGINS_STATS_HPP
#define MY_PLUGINS_STATS_HPP

#include <vector>
#include "src/DistrhoDefines.h"
#include "DistrhoDetails.hpp"
#include "TimePositionCalc.hpp"

namespace myseq {

    struct Transport {
        bool valid;
        bool playing;
        uint64_t frame;
        int32_t bar;
        int32_t beat;
        double tick;
        double bar_start_tick;
        float beats_per_bar;
        float beat_type;
        double ticks_per_beat;
        double beats_per_minute;
    };

    Transport transport_from_time_position(const TimePosition &tp);

    struct ActivePatternStats {
        int32_t pattern_id;
        double duration;
        double time;
    };
    struct Stats {
        Transport transport{};
        std::vector<ActivePatternStats> active_patterns;
    };

}

#endif //MY_PLUGINS_STATS_HPP
