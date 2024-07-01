//
// Created by Arunas on 29/06/2024.
//

#ifndef MY_PLUGINS_STATS_HPP
#define MY_PLUGINS_STATS_HPP

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cista.h>
#include "src/DistrhoDefines.h"
#include "DistrhoDetails.hpp"
#include "TimePositionCalc.hpp"

namespace myseq {

    namespace ipc = boost::interprocess;
    namespace data = cista::offset;

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

    struct Stats {
        Transport transport{};
    };

    struct StatsWriterShm {
        ipc::shared_memory_object shm_obj;
        ipc::mapped_region shm_reg;

        StatsWriterShm(const char *name) {
            shm_obj = ipc::shared_memory_object(ipc::open_or_create, name, ipc::read_write);
            shm_obj.truncate(1024);
            shm_reg = ipc::mapped_region(shm_obj, ipc::read_write, 0, 1024);
            std::memset(shm_reg.get_address(), 0, shm_reg.get_size());
        }

        ~StatsWriterShm() {
            ipc::shared_memory_object::remove(shm_obj.get_name());
        }

        void write(const Stats &stats) {
            auto buf = cista::serialize(stats);
            std::memcpy((std::uint8_t *) shm_reg.get_address(), buf.data(), buf.size());
        }
    };

    struct StatsReaderShm {
        ipc::shared_memory_object shm_obj;
        ipc::mapped_region shm_reg;

        StatsReaderShm(const char *name) {
            shm_obj = ipc::shared_memory_object(ipc::open_or_create, name, ipc::read_only);
            shm_reg = ipc::mapped_region(shm_obj, ipc::read_only, 0, 1024);
        }

        ~StatsReaderShm() {
            ipc::shared_memory_object::remove(shm_obj.get_name());
        }

        Stats *read() {
            auto addr = (std::uint8_t *) shm_reg.get_address();
            std::uint8_t *start = addr;
            std::uint8_t *end = start + shm_reg.get_size();
            return cista::deserialize<Stats>(start, end);
        }
    };


}

#endif //MY_PLUGINS_STATS_HPP
