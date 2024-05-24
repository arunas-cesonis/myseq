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
#include "Notes.hpp"

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
        ImVec2 offset;
        int pattern_width_slider_value;
        static constexpr int visible_rows = 16;


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


            myseq::test_serialize();
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

        void publish() {
            auto s = state.to_json_string();
            setState("pattern", s.c_str());
#ifdef DEBUG
            state = myseq::State::from_json_string(s.c_str());
#endif
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

        float cell_width = 39.0f;
        float cell_height = 30.0f;

        [[nodiscard]] myseq::V2i
        calc_cell(const myseq::Pattern &p, const ImVec2 &cpos, const ImVec2 &mpos, const ImVec2 &grid_size) const {
            if (
                    mpos.x >= cpos.x && mpos.x < cpos.x + grid_size.x
                    && mpos.y >= cpos.y && mpos.y < cpos.y + grid_size.y
                    ) {
                auto mrow = (int) (std::floor((mpos.y - cpos.y - offset.y) / cell_height));
                auto mcol = (int) (std::floor((mpos.x - cpos.x - offset.x) / cell_width));
                mrow = mrow >= p.height ? -1 : mrow;
                mcol = mcol >= p.width ? -1 : mcol;
                return {mcol, mrow};
            } else {
                return {-1, -1};
            }
        }

        void onImGuiDisplay() override {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(static_cast<float>(getWidth()), static_cast<float>(getHeight())));
            int window_flags = ImGuiWindowFlags_NoDecoration;
            float cell_padding = 4.0;
            bool dirty = false;
            ImVec2 cell_padding_xy = ImVec2(cell_padding, cell_padding);
            auto &p = state.get_selected_pattern();

            pattern_width_slider_value = p.width;
            if (ImGui::Begin("MySeq", nullptr, window_flags)) {
                auto avail = ImGui::GetContentRegionAvail();
                float width = avail.x;
                const auto active_cell = ImColor(0x5a, 0x8a, 0xcf);
                const auto inactive_cell = ImColor(0x45, 0x45, 0x45);
                const auto hovered_color = ImColor(IM_COL32_WHITE);
                auto cpos = ImGui::GetCursorPos();
                auto height = cell_height * (float) visible_rows;
                const auto grid_size = ImVec2(width, height);
                auto mpos = ImGui::GetMousePos();
                auto cell = calc_cell(p, cpos, mpos, grid_size);
                auto valid_cell = cell.x >= 0 && cell.y >= 0;
                auto *draw_list = ImGui::GetWindowDrawList();
                auto border_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)).operator ImU32();
                auto strong_border_color = ImColor(
                        ImGui::GetStyleColorVec4(ImGuiCol_TableBorderStrong)).operator ImU32();
                auto ctrl_held = ImGui::GetIO().KeyCtrl;
                auto alt_held = ImGui::GetIO().KeyAlt;
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

                const auto corner = (ImVec2(0.0, 0.0) - offset) / ImVec2(cell_width, cell_height);
                const auto corner2 = corner + ImVec2(width, height) / ImVec2(cell_width, cell_height);
                const auto first_visible_row = (int) std::floor(corner.y);
                const auto last_visible_row = std::min(p.height - 1, (int) std::floor(corner2.y));
                const auto first_visible_col = std::max(0, (int) std::floor(corner.x));
                const auto last_visible_col = std::min(p.width - 1, (int) std::floor(corner2.x));

                draw_list->PushClipRect(cpos, cpos + ImVec2(width, height), true);

                for (auto i = first_visible_row; i <= last_visible_row; i++) {
                    for (auto j = first_visible_col; j <= last_visible_col; j++) {
                        auto loop_cell = V2i(j, i);
                        auto p_min = ImVec2(cell_width * (float) loop_cell.x,
//                                            cell_height * (float) (p.height - loop_cell.y - 1)) +
                                            cell_height * (float) loop_cell.y)
                                     + cell_padding_xy + cpos +
                                     offset;
                        auto p_max = p_min + ImVec2(cell_width, cell_height) - cell_padding_xy;
                        auto vel = p.get_velocity(loop_cell);
                        auto velocity_fade = ((float) vel) / 127.0f;
                        auto cell_color = ImColor(ImLerp(inactive_cell.Value, active_cell.Value, velocity_fade));
                        //auto c = is_hovered && interaction == Interaction::None ? hovered_color : cell_color;
                        auto fade = (j / 4) % 2 == 0 ? 1.0f : 0.8f;
                        cell_color.Value.x *= fade;
                        cell_color.Value.y *= fade;
                        cell_color.Value.z *= fade;
                        draw_list->AddRectFilled(p_min, p_max, cell_color);
                        draw_list->AddRect(p_min, p_max, border_color);

                        if (alt_held && loop_cell.x == 0) {
                            auto note = myseq::utils::row_index_to_midi_note(loop_cell.y);
                            draw_list->AddText(p_min, IM_COL32_WHITE, ALL_NOTES[note]);
                        } else if (vel > 0) {
                            draw_list->AddText(p_min, IM_COL32_WHITE, ONE_TO_256[vel - 1]);
                        } else {
                            // std::ostringstream os;
                            // os << loop_cell.x << ' ' << loop_cell.y;
                            // draw_list->AddText(p_min, IM_COL32_WHITE, os.str().c_str());
                        }
                    }
                    /// void ImDrawList::AddRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness)
                }
                draw_list->PopClipRect();
                draw_list->AddRect(cpos, cpos + ImVec2(width, height), border_color);
                ImGui::Dummy(ImVec2(width, height));


                const auto left = std::min(0.0f, 0.0f - ((cell_width * (float) p.width) - width));
                const auto right = 0.0f;
                const auto top = std::min(0.0f, 0.0f - ((cell_height * (float) p.height) - height));
                const auto bottom = 0.0f;
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    offset += ImGui::GetIO().MouseDelta;
                    offset.x = std::clamp(offset.x, left, right);
                    offset.y = std::clamp(offset.y, top, bottom);
                }
                if (ImGui::SliderInt("##pattern_width", &pattern_width_slider_value, 1, 32, nullptr,
                                     ImGuiSliderFlags_None)) {
                    p.resize_width(pattern_width_slider_value);
                    dirty = true;
                }

                //ImGui::PopID();

                ImGui::Text("cell %d %d", cell.x, cell.y);
                ImGui::Text("ImGui::IsMouseDown(ImGuiMouseButton_Left)=%d", ImGui::IsMouseDown(ImGuiMouseButton_Left));
                ImGui::Text("ImGui::IsMouseDown(ImGuiMouseButton_Right)=%d",
                            ImGui::IsMouseDown(ImGuiMouseButton_Right));
                ImGui::Text("drag_started_velocity=%d", drag_started_velocity);
                ImGui::Text("first_visible_col=%d", first_visible_col);
                ImGui::Text("last_visible_col=%d", last_visible_col);
                ImGui::Text("first_visible_row=%d", first_visible_row);
                ImGui::Text("last_visible_row=%d", last_visible_row);
                ImGui::Text("corner=%f %f", corner.x, corner.y);
                ImGui::Text("valid_cell=%d", valid_cell);
                ImGui::Text("interaction=%s", interaction_to_string(interaction));
            }


            if (ImGui::Button("Add pattern")) {
                int id = state.next_unused_id(state.selected);
                state.create_pattern(id);
                state.selected = id;
                dirty = true;
            }

            if (ImGui::BeginListBox("##patterns")) {
                state.each_pattern_id([&](int id) {
                    if (ImGui::Selectable(std::to_string(id).c_str(), id == state.selected, ImGuiSelectableFlags_None,
                                          ImVec2(0, 0))) {
                        state.selected = id;
                    }
                });
                ImGui::EndListBox();
            }

            ImGui::End();
            if (dirty) {
                publish();
            }
        }

        void idleCallback() override {
            repaint();
        }

        void stateChanged(const char *key, const char *value) override {
            d_debug("PluginUI: stateChanged value=[%s]\n", value ? value : "null");
            if (std::strcmp(key, "pattern") == 0) {
                state = myseq::State::from_json_string(value);
            }
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
