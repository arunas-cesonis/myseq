//
// Created by Arunas on 07/06/2024.
//

#ifndef MY_PLUGINS_PLUGINDSP_HPP
#define MY_PLUGINS_PLUGINDSP_HPP

#include <cassert>
#include <random>
#include <chrono>
#include <map>
#include <cista.h>
#include <iomanip>
#include "DistrhoPlugin.hpp"
#include "Patterns.hpp"
#include "Stats.hpp"
#include "Player.hpp"
#include "TimePositionCalc.hpp"

namespace ipc = boost::interprocess;

START_NAMESPACE_DISTRHO

    class MySeqPlugin : public Plugin {
    public:
        /**
           Plugin class constructor.
           You must set all parameter values to their defaults, matching ParameterRanges::def.
         */
        myseq::Player player;
        myseq::State state;
        std::string instance_id = "";
        TimePosition last_time_position;
        int iteration = 0;
        bool prev_play_selected = false;
        enum class ShmStatus {
            Unavailable,
            Configuring,
            Ready
        } shm_status = ShmStatus::Unavailable;

        myseq::Stats stats;
        std::optional<myseq::StatsWriterShm> stats_writer_shm;

        MySeqPlugin()
                : Plugin(0, 0, 2) {
            myseq::Test::test_player_run();
        }

    protected:
        // ----------------------------------------------------------------------------------------------------------------
        // Information

        /**
           Get the plugin label.@n
           This label is a short restricted name consisting of only _, a-z, A-Z and 0-9 characters.
         */
        const char *getLabel() const noexcept override {
            return "MySeq";
        }

        /**
           Get the plugin author/maker.
         */
        const char *getMaker() const noexcept override {
            return "seunje";
        }

        /**
           Get the plugin license (a single line of text or a URL).@n
           For commercial plugins this should return some short copyright information.
         */
        const char *getLicense() const noexcept override {
            return "ISC";
        }

        /**
           Get the plugin version, in hexadecimal.
           @see d_version()
         */
        uint32_t getVersion() const noexcept override {
            return d_version(1, 0, 0);
        }

        /**
           Get the plugin unique Id.@n
           This value is used by LADSPA, DSSI and VST plugin formats.
           @see d_cconst()
         */
        int64_t getUniqueId() const noexcept override {
            return d_cconst('d', 'T', 'x', 't');
        }

        bool is_playing_only_selected() {
            return player.active_patterns.size() == 1
                   && player.active_patterns[0].pattern_id == state.selected
                   && player.active_patterns[0].note == myseq::Note{127, 127};
        }

        void run_player1(uint32_t frames, [[maybe_unused]] const MidiEvent *midiEvents,
                         [[maybe_unused]] uint32_t midiEventCount, const myseq::TimePositionCalc &tc,
                         const myseq::TimeParams &tp) {

            if (state.play_selected != prev_play_selected) {
                if (state.play_selected) {
                    player.start_selected_pattern(state);
                } else {
                    player.stop_selected_pattern(state);
                }
            } else if (state.play_selected) {
                player.update_selected_pattern(state);
            }

            if (!state.play_selected) {
                for (auto i = 0; i < (int) midiEventCount; i++) {
                    auto &ev = midiEvents[i];

                    std::optional<myseq::NoteMessage> msg = myseq::NoteMessage::parse(ev.data);
                    if (msg.has_value()) {
                        const auto &v = msg.value();
                        const auto time = tp.time + (ev.frame / tc.frames_per_tick());
                        switch (v.type) {
                            case myseq::NoteMessage::Type::NoteOn:
                                d_debug("PluginDSP: IN: NOTE ON  %3d %d:%d", v.note.note, iteration, ev.frame);
                                player.start_patterns(state, v.note, v.velocity, time, tp);
                                break;
                            case myseq::NoteMessage::Type::NoteOff:
                                d_debug("PluginDSP: IN: NOTE OFF %3d %d:%d", v.note.note, iteration, ev.frame);
                                player.stop_patterns(v.note, time);
                                break;
                            default:
                                break;
                        }
                    }
                }
            }

            auto send = [&](uint8_t note, uint8_t velocity, double time) {
                const auto msg = velocity == 0 ? 0x80 : 0x90;
                const auto note_on = msg == 0x90;
                const auto absolute_time = tp.time + time;
                const auto frame = static_cast<uint32_t>(time * tc.frames_per_tick());
                const MidiEvent evt = {
                        frame,
                        3, {
                                (uint8_t) msg,
                                note, velocity, 0},
                        nullptr
                };
                const auto k = msg == 0x90 ? "ON " : "OFF";
                d_debug("PluginDSP: OUT: NOTE %s %3d %d:%d", k, note, iteration, evt.frame);
                writeMidiEvent(evt);
            };

            prev_play_selected = state.play_selected;
            player.run(send, state, tp);
        }

        void run(const float **inputs, float **outputs, uint32_t frames, [[maybe_unused]] const MidiEvent *midiEvents,
                 [[maybe_unused]] uint32_t midiEventCount) override {
            // audio pass-through
            if (inputs[0] != outputs[0])
                std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);

            if (inputs[1] != outputs[1])
                std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);

            const TimePosition &t = getTimePosition();
            const myseq::TimePositionCalc tc(t, getSampleRate());
            const myseq::TimeParams tp = {tc.global_tick(), tc.sixteenth_note_duration_in_ticks(),
                                          ((double) frames) / tc.frames_per_tick(), t.playing, iteration};

            run_player1(frames, midiEvents, midiEventCount, tc, tp);
            // run_player2(frames, midiEvents, midiEventCount);

            if (stats_writer_shm.has_value()) {
                stats.transport = myseq::transport_from_time_position(t);
                stats.active_patterns.clear();
                player.push_active_pattern_stats(stats, state, tp);
                stats_writer_shm->write(stats);
            }

            last_time_position = t;
            iteration++;
        }

        //void init_shm() {
        //    stats_writer_shm.emplace(instance_id.c_str());
        //    d_debug("PluginDSP: init_shm: %s", instance_id.c_str());
        //    d_debug("PluginDSP: init_shm: creating shm_obj");
        //    shm_obj = ipc::shared_memory_object(ipc::open_or_create, instance_id.c_str(), ipc::read_write);
        //    d_debug("PluginDSP: init_shm: created shm_obj");
        //    d_debug("PluginDSP: init_shm: resizing shm_obj");
        //    shm_obj.truncate(1024);
        //    d_debug("PluginDSP: init_shm: resized shm_obj");
        //    d_debug("PluginDSP: init_shm: creating shm_reg");
        //    shm_reg = ipc::mapped_region(shm_obj, ipc::read_write, 0, 1024);
        //    d_debug("PluginDSP: init_shm: created shm_reg");
        //    shm_status = ShmStatus::Ready;
        //    d_debug("PluginDSP: init_shm: %s ready", instance_id.c_str());
        //}

        void setState(const char *key, const char *value) override {
            d_debug("PluginDSP: setState: key=%s", key);
            if (std::strcmp(key, "pattern") == 0) {
                state = myseq::State::from_json_string(value);
            } else if (std::strcmp(key, "instance_id") == 0) {
                if (!stats_writer_shm.has_value()) {
                    d_debug("PluginDSP: init_shm: configuring");
                    assert(shm_status != ShmStatus::Ready);
                    shm_status = ShmStatus::Configuring;
                    instance_id = value;
                    stats_writer_shm.emplace(instance_id.c_str());
                    shm_status = ShmStatus::Ready;
                    // init_shm();
                }
            } else {
                assert(false);
            }
        }

        String getState(const char *key) const override {
            if (std::strcmp(key, "pattern") == 0) {
                return String(state.to_json_string().c_str());
            } else if (std::strcmp(key, "instance_id") == 0) {
                return String(instance_id.c_str());
            } else {
                assert(false);
            }
        }

        void activate() override {
            d_debug("PluginDSP: activate");
        }

        void deactivate() override {
            d_debug("PluginDSP: deactivate");
        }

        void initState(uint32_t index, State &st) override {
            //   state.key = "ticks";
            //    state.defaultValue = "0";
            d_debug("PluginDSP: initState index=%d\n", index);
            DISTRHO_SAFE_ASSERT(index <= 1);
            switch (index) {
                case 0:
                    st.key = "pattern";
                    st.label = "Pattern";
                    state = myseq::State();
                    //{"selected":0,"patterns":[{"width":32,"id":0,"height":128,"first_note":0,"last_note":15,"cursor_x":0,"cursor_y":91," cells":[{"x":0,"y":91,"v":127},{"x":4,"y":91,"v":127},{"x":8,"y":91,"v":127},{"x":16,"y":91,"v":127},{"x":20,"y":91,"v":127},{"x":12,"y":91,"v":127},{"x":24,"y":91,"v":127}]}]}
                    st.defaultValue = String(state.to_json_string().c_str());
                    break;
                case 1:
                    st.key = "instance_id";
                    st.label = "instance_id";
                    st.defaultValue = String(myseq::utils::gen_instance_id().c_str());
                    break;
            }
        }


        /* ---------------------------------------------------------------------------------------------------------------- */ DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
            MySeqPlugin)
    };

END_NAMESPACE_DISTRHO
#endif //MY_PLUGINS_PLUGINDSP_HPP
