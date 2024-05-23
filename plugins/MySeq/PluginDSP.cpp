/*
 * Text Editor example
 * Copyright (C) 2023 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */


#include <cassert>
#include "DistrhoPlugin.hpp"
#include "Patterns.hpp"
#include "Player.hpp"
#include "TimePositionCalc.hpp"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

    class MySeqPlugin : public Plugin {
    public:
        /**
           Plugin class constructor.
           You must set all parameter values to their defaults, matching ParameterRanges::def.
         */
        myseq::Player player;
        myseq::State state;

        MySeqPlugin()
                : Plugin(0, 0, 1) // parameters, programs, states
        {
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

        // ----------------------------------------------------------------------------------------------------------------
        // Audio/MIDI Processing

        /**
           Run/process function for plugins without MIDI input.
           @note Some parameters might be null if there are no audio inputs or outputs.
         */

        void run(const float **inputs, float **outputs, uint32_t frames,
                 [[maybe_unused]] const MidiEvent *midiEvents, [[maybe_unused]] uint32_t midiEventCount) override {
            // audio pass-through
            if (inputs[0] != outputs[0])
                std::memcpy(outputs[0], inputs[0], sizeof(float) * frames);

            if (inputs[1] != outputs[1])
                std::memcpy(outputs[1], inputs[1], sizeof(float) * frames);

            const TimePosition &t = getTimePosition();
            const myseq::TimePositionCalc tc(t, getSampleRate());

            const myseq::TimeParams tp = {tc.global_tick(), tc.sixteenth_note_duration_in_ticks(),
                                          ((double) frames) / tc.frames_per_tick(), t.playing};
            if (tp.playing) {
                d_debug("time: ticks=%f frames=%f", tc.global_tick(), tc.global_frame(), tp.playing);
            }

            for (auto i = 0; i < (int) midiEventCount; i++) {
                auto &ev = midiEvents[i];
                std::optional<myseq::NoteMessage> msg = myseq::NoteMessage::parse(ev.data);
                if (msg.has_value()) {
                    const auto &v = msg.value();
                    const auto time = tp.time + (ev.frame / tc.frames_per_tick());
                    d_debug("MidiEvent: type=%s note=%02x velocity=%02x frame=%d time=%f",
                            msg.value().type == myseq::NoteMessage::Type::NoteOn ? "NoteOn" : "NoteOff",
                            (int) msg.value().note.note, (int) msg.value().velocity, ev.frame, time);
                    switch (v.type) {
                        case myseq::NoteMessage::Type::NoteOn:
                            player.start_pattern(v.note, time);
                            break;
                        case myseq::NoteMessage::Type::NoteOff:
                            player.stop_pattern(v.note, time);
                            break;
                        default:
                            break;
                    }
                }
            }

            auto send = [=](uint8_t note, uint8_t velocity, double time) {
                const auto msg = velocity == 0 ? 0x80 : 0x90;
                const MidiEvent evt = {
                        static_cast<uint32_t>(time * tc.frames_per_tick()),
                        3, {
                                (uint8_t) msg,
                                note, velocity, 0},
                        nullptr
                };
                d_debug("msg=0x%02x note=0x%02x velocity=%0x time=%f", msg, note, velocity, time);
                writeMidiEvent(evt);
            };
            player.run(send, state, tp);


            const std::size_t pattern_id = 0;
            const auto &p = state.get_pattern(pattern_id);
            //params.setTimePosition(t);
        }

        void setState(const char *key, const char *value) override {
            // d_debug("PluginDSP: setState: key=%s pattern=[%s] value=<<EOF\n%s\nEOF]\n", key, seq.to_string().c_str(), value != nullptr ? value : "null");
            DISTRHO_SAFE_ASSERT(std::strcmp("pattern", key) == 0);
            state = myseq::State::from_json_string(value);
        }

        String getState(const char *key) const override {
            // d_debug("PluginDSP: getState: key=%s pattern=[%s]\n", key, seq.to_string().c_str());
            DISTRHO_SAFE_ASSERT(std::strcmp("pattern", key) == 0);
            return String(state.to_json_string().c_str());
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
                    st.defaultValue = String(state.to_json_string().c_str());
                    break;
            }
        }


        // ----------------------------------------------------------------------------------------------------------------

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MySeqPlugin)
    };

// --------------------------------------------------------------------------------------------------------------------

    Plugin *createPlugin() {
        return new MySeqPlugin();
    }

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
