/*
 * Text Editor example
 * Copyright (C) 2023 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include <iostream>
#include <algorithm>
#include "DistrhoUI.hpp"
#include "Patterns.hpp"
#include "Numbers.hpp"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

    using myseq::V2i;

    template<typename T>
    T clamp(T value, T min, T max) {
        return std::min(std::max(value, min), max);
    }

    class MySeqUI : public UI {
    public:
        myseq::State state;
        uint8_t drag_started_velocity = 0;
        V2i drag_started_cell;
        ImVec2 drag_started_mpos;

        enum class Interaction {
            None,
            DrawingCells,
            AdjustingVelocity
        };
        Interaction interaction = Interaction::None;

        /**
           UI class constructor.
           The UI should be initialized to a default state that matches the plugin side.
         */
        MySeqUI()
                : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
            const double scaleFactor = getScaleFactor();

            state.create_pattern(0);

            if (d_isEqual(scaleFactor, 1.0)) {
                setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT);
            } else {
                const uint width = DISTRHO_UI_DEFAULT_WIDTH * scaleFactor;
                const uint height = DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor;
                setGeometryConstraints(width, height);
                setSize(width, height);
            }
        }


    protected:
        static const char *interaction_to_string(Interaction i) {
            switch (i) {
                case Interaction::None:
                    return "None";
                case Interaction::DrawingCells:
                    return "DrawingCells";
                case Interaction::AdjustingVelocity:
                    return "AdjustingVelocity";
            }
            return "Unknown";
        }
        // ----------------------------------------------------------------------------------------------------------------
        // DSP/Plugin Callbacks

        /**
           A parameter has changed on the plugin side.@n
           This is called by the host to inform the UI about parameter changes.
         */
        void parameterChanged(uint32_t, float) override {}

        void onImGuiDisplay() override {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(getWidth(), getHeight()));
            int window_flags = ImGuiWindowFlags_NoDecoration;
            float cell_padding = 4.0;
            bool dirty = false;
            ImVec2 cell_padding_xy = ImVec2(cell_padding, cell_padding);
            auto &p = state.get_selected_pattern();
            if (ImGui::Begin("MySeq", nullptr, window_flags)) {

                auto avail = ImGui::GetContentRegionAvail();
                float width = avail.x;

                float cell_width = width / (float) p.width;
                float cell_height = cell_width;
                cell_width = 40.0;
                cell_height = 30.0;
                const auto active_cell = ImColor(0x5a, 0x8a, 0xcf);
                const auto inactive_cell = ImColor(0x45, 0x45, 0x45);
                const auto hovered_color = ImColor(IM_COL32_WHITE);
                auto cpos = ImGui::GetCursorPos();
                auto mpos = ImGui::GetMousePos();
                auto mrow = (int) (std::floor((mpos.y - cpos.y) / cell_height));
                auto height = cell_height * (float) p.height;
                auto mcol = (int) (std::floor((mpos.x - cpos.x) / cell_width));
                mrow = mrow >= p.height ? -1 : mrow;
                mcol = mcol >= p.width ? -1 : mcol;
                auto cell = V2i(mcol, mrow);
                auto valid_cell = cell.x >= 0 && cell.y >= 0;
                auto *draw_list = ImGui::GetWindowDrawList();
                auto border_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)).operator ImU32();
                auto ctrl_held = ImGui::GetIO().KeyCtrl;
                switch (interaction) {
                    case Interaction::DrawingCells: {
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            if (valid_cell) {
                                p.set_velocity(cell, drag_started_velocity == 0 ? 127 : 0);
                                dirty = true;
                            }
                        } else {
                            interaction = Interaction::None;
                        }
                        break;
                    }
                    case Interaction::AdjustingVelocity: {
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            if (valid_cell) {
                                auto delta_y = mpos.y - drag_started_mpos.y;
                                auto new_vel = (uint8_t) clamp(
                                        (int) std::round((float) drag_started_velocity - (float) delta_y), 1, 127);
                                p.set_velocity(drag_started_cell, new_vel);
                                dirty = true;
                            }
                        } else {
                            interaction = Interaction::None;
                        }
                        break;
                    }
                    case Interaction::None:
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            if (valid_cell) {
                                if (ctrl_held) {
                                    interaction = Interaction::AdjustingVelocity;
                                    drag_started_velocity = p.get_velocity(cell);
                                    drag_started_mpos = mpos;
                                    drag_started_cell = cell;
                                } else {
                                    interaction = Interaction::DrawingCells;
                                    drag_started_velocity = p.get_velocity(cell);
                                    p.set_velocity(cell, drag_started_velocity == 0 ? 127 : 0);
                                    dirty = true;
                                }
                            }
                        }
                }
                for (auto i = 0; i < p.height; i++) {
                    for (auto j = 0; j < p.width; j++) {
                        auto loop_cell = V2i(j, i);
                        auto is_hovered = loop_cell == cell;
                        auto index = i * p.width + j;
                        auto active = p.get_velocity(loop_cell) > 0;
                        auto p_min = ImVec2(cell_width * (float) j, cell_height * (float) i) + cell_padding_xy + cpos;
                        auto p_max = p_min + ImVec2(cell_width, cell_height) - cell_padding_xy;
                        auto vel = p.get_velocity(loop_cell);
                        auto velocity_fade = ((float) vel) / 127.0f;
                        auto cell_color = ImColor(ImLerp(inactive_cell.Value, active_cell.Value, velocity_fade));
                        //auto c = is_hovered && interaction == Interaction::None ? hovered_color : cell_color;
                        draw_list->AddRectFilled(p_min, p_max, cell_color);
                        draw_list->AddRect(p_min, p_max, border_color);
                        if (vel > 0) {
                            draw_list->AddText(p_min, IM_COL32_WHITE, ONE_TO_256[vel - 1]);
                        }
                    }
                    /// void ImDrawList::AddRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness)
                }

                ImGui::Dummy(ImVec2(0, height));
                ImGui::Text("%d %d", mrow, mcol);
                ImGui::Text("drag_started_velocity=%d", drag_started_velocity);
                ImGui::Text("valid_cell=%d", valid_cell);
                ImGui::Text("interaction=%s", interaction_to_string(interaction));
            }
            ImGui::End();
            if (dirty) {
                auto tmp = state.to_json_string();
                state = myseq::State::from_json_string(tmp.c_str());
                d_debug("%s", state.to_json_string().c_str());
            }

        }

        void stateChanged(const char *key, const char *value) override {

        }
        // ----------------------------------------------------------------------------------------------------------------

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MySeqUI)
    };

// --------------------------------------------------------------------------------------------------------------------

    UI *createUI() {
        return new MySeqUI();
    }

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
