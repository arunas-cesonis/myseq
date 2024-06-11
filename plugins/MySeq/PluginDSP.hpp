//
// Created by Arunas on 07/06/2024.
//

#ifndef MY_PLUGINS_PLUGINDSP_HPP
#define MY_PLUGINS_PLUGINDSP_HPP

#include <cassert>
#include <map>
#include "DistrhoPlugin.hpp"
#include "Patterns.hpp"
#include "Player.hpp"
#include "TimePositionCalc.hpp"

#ifndef DEBUG
#error "why not debug"
#endif

START_NAMESPACE_DISTRHO

    class MySeqPlugin : public Plugin {
    public:
        /**
           Plugin class constructor.
           You must set all parameter values to their defaults, matching ParameterRanges::def.
         */
        myseq::Player player;
        myseq::State state;
        TimePosition last_time_position;
        int iteration = 0;

        MySeqPlugin()
                : Plugin(0, 0, 1) // parameters, programs, states
        {
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

        void run_player1(uint32_t frames, [[maybe_unused]] const MidiEvent *midiEvents,
                         [[maybe_unused]] uint32_t midiEventCount) {
            const TimePosition &t = getTimePosition();
            const myseq::TimePositionCalc tc(t, getSampleRate());
            const myseq::TimeParams tp = {tc.global_tick(), tc.sixteenth_note_duration_in_ticks(),
                                          ((double) frames) / tc.frames_per_tick(), t.playing, iteration};
            for (auto i = 0; i < (int) midiEventCount; i++) {
                auto &ev = midiEvents[i];
                std::optional<myseq::NoteMessage> msg = myseq::NoteMessage::parse(ev.data);
                if (msg.has_value()) {
                    const auto &v = msg.value();
                    const auto time = tp.time + (ev.frame / tc.frames_per_tick());
                    switch (v.type) {
                        case myseq::NoteMessage::Type::NoteOn:
                            d_debug(" IN: NOTE ON  %3d %d:%d", v.note.note, iteration, ev.frame);
                            player.start_pattern(state, v.note, time, tp);
                            break;
                        case myseq::NoteMessage::Type::NoteOff:
                            d_debug(" IN: NOTE OFF %3d %d:%d", v.note.note, iteration, ev.frame);
                            player.stop_pattern(v.note, time);
                            break;
                        default:
                            break;
                    }
                }
            }

            auto send = [=](uint8_t note, uint8_t velocity, double time) {
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
                d_debug("OUT: NOTE %s %3d %d:%d", k, note, iteration, evt.frame);
                writeMidiEvent(evt);
            };

            const auto frames_per_tick = tc.frames_per_tick();
            player.run(send, state, tp, frames_per_tick);
        }

        void run(const float **inputs, float **outputs, uint32_t frames, [[maybe_unused]] const MidiEvent *midiEvents,
                 [[maybe_unused]] uint32_t midiEventCount) override {
            // audio pass-through
            if (inputs[0] != outputs[0])
                std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);

            if (inputs[1] != outputs[1])
                std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);

            run_player1(frames, midiEvents, midiEventCount);
            // run_player2(frames, midiEvents, midiEventCount);
            const TimePosition &t = getTimePosition();
            last_time_position = t;

            // params.setTimePosition(t);

            // int fr = (int) std::round(tc.global_frame());
            // if (previous != -1) {
            //     // d_debug("previous=%d frame=%d -> %d", previous, fr, frame - previous);
            // }
            // uint32_t midi_event = 0;
            // const auto global_tick = tc.global_tick();
            // const auto frames_per_tick = tc.frames_per_tick();
            // myseq::TimeParams2 tp;
            // tp.tick = 0.0;
            // tp.frame = 0;
            // tp.frames_per_tick = tc.frames_per_tick();
            // tp.ticks_per_sixteenth_note = tc.sixteenth_note_duration_in_ticks();
            // tp.playing = t.playing;
            // for (uint32_t x = 0; x < midiEventCount; x++) {
            //     //d_debug("MIDI EVENT %d frame=%d %d %d %d %d", x, midiEvents[x].frame, midiEvents[x].data[0],
            //     //       midiEvents[x].data[1], midiEvents[x].data[2], midiEvents[x].data[3]);
            // }
            // std::vector<MidiEvent> midi_events_out;
            // for (uint32_t frame = 0; frame < frames; frame++) {
            //     std::vector<MidiEvent> midi_events_in;
            //     while (midi_event < midiEventCount && midiEvents[midi_event].frame == frame) {
            //         midi_events_in.push_back(midiEvents[midi_event]);
            //         midi_event++;
            //     }
            //     tp.frame = (int) frame + (t.frame != 0 ? t.frame : (int) tc.global_frame());
            //     tp.tick = global_tick + (double) frame / frames_per_tick;
            //     player2.run(tp, state, midi_events_in, midi_events_out);
            // }
            // for (const auto ev: midi_events_out) {
            //     writeMidiEvent(ev);
            // }
            // if (t.playing) {
            //     previous = fr + (int) frames;
            // } else {
            //     previous = -1;
            // }
            iteration++;
        }

        void setState(const char *key, const char *value) override {
            DISTRHO_SAFE_ASSERT(std::strcmp("pattern", key) == 0);
            state = myseq::State::from_json_string(value);
        }

        String getState(const char *key) const override {
            DISTRHO_SAFE_ASSERT(std::strcmp("pattern", key) == 0);
            return String(state.to_json_string().c_str());
        }

        void activate() override {
            d_debug("ACTIVATE");
        }

        void deactivate() override {
            d_debug("DEACTIVATE");
        }

        void initState(uint32_t index, State &st) override {
            //   state.key = "ticks";
            //    state.defaultValue = "0";
            d_debug("PluginDSP: initState index=%d\n", index);
            DISTRHO_SAFE_ASSERT(index == 0);
            switch (index) {
                case 0:
                    st.key = "pattern";
                    st.label = "Pattern";
                    state = myseq::State();
                    st.defaultValue = String(state.to_json_string().c_str());
                    break;
            }
        }


        // ----------------------------------------------------------------------------------------------------------------

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MySeqPlugin)
    };

END_NAMESPACE_DISTRHO
#endif //MY_PLUGINS_PLUGINDSP_HPP
