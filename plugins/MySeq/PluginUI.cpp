/*
 * Text Editor example
 * Copyright (C) 2023 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include <iostream>
#include <algorithm>
#include "DistrhoUI.hpp"
#include "PluginDSP.hpp"
#include "Patterns.hpp"
#include "Numbers.hpp"
#include "Notes.hpp"
#include "TimePositionCalc.hpp"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

    using myseq::V2i;

    static const char *NOTES[] =
            {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", nullptr,};

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
        int count = 0;
        ImVec2 offset;
        static constexpr int visible_rows = 16;
        float cell_height = 30.0f;

        enum class Interaction {
            None,
            DrawingCells,
            AdjustingVelocity,
            KeysSelect
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

            offset = ImVec2(0.0f, -cell_height * (float) (visible_rows + 48));

            if (d_isEqual(scaleFactor, 1.0)) {
                setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT);
            } else {
                const uint width = DISTRHO_UI_DEFAULT_WIDTH * scaleFactor;
                const uint height = DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor;
                setGeometryConstraints(width, height);
                setSize(width, height);
            }
        }

        int publish_count = 0;

        void publish() {
            auto s = state.to_json_string();
            setState("pattern", s.c_str());
// #ifdef DEBUG
            state = myseq::State::from_json_string(s.c_str());
            publish_count += 1;
// #endif
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
                case Interaction::KeysSelect:
                    return "KeysSelect";
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

        [[nodiscard]] myseq::V2i
        calc_cell(const myseq::Pattern &p, const ImVec2 &cpos, const ImVec2 &mpos, const ImVec2 &grid_size,
                  const ImVec2 &cell_size) const {
            if (
                    mpos.x >= cpos.x && mpos.x < cpos.x + grid_size.x
                    && mpos.y >= cpos.y && mpos.y < cpos.y + grid_size.y
                    ) {
                auto mrow = (int) (std::floor((mpos.y - cpos.y - offset.y) / cell_size.y));
                auto mcol = (int) (std::floor((mpos.x - cpos.x - offset.x) / cell_size.x));
                mrow = mrow >= p.height ? -1 : mrow;
                mcol = mcol >= p.width ? -1 : mcol;
                return {mcol, mrow};
            } else {
                return {-1, -1};
            }
        }

        static int note_select(int note) {
            int selected_octave = note / 12;
            int selected_note = note % 12;
            auto width = ImGui::CalcItemWidth();
            auto notes = 127;
            auto octaves = (notes / 12) + (notes % 12 == 0 ? 0 : 1);
            auto item_width = width / (float) octaves;
            auto g = ImGui::GetCurrentContext();
            static int click_count = 0;

            if (ImGui::BeginPopupContextItem("select")) {
                const int max_note = selected_octave < 10 ? 12 : 7;
                const auto flags =
                        click_count % 2 == 0 ? ImGuiSelectableFlags_DontClosePopups : ImGuiSelectableFlags_None;
                for (int i = 0; i < octaves; i++) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Selectable(std::to_string(i - 1).c_str(), i == selected_octave, flags,
                                          ImVec2(15.0f, 0.0f))) {
                        selected_octave = i;
                        click_count += 1;
                        if (selected_octave >= 10)
                            selected_note = std::min(selected_note, 7);
                    }
                }
                for (int i = 0; i < max_note; i++) {
                    if (i > 0) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Selectable(NOTES[i], i == selected_note, flags,
                                          ImVec2(15.0f, 0.0f))) {
                        selected_note = i;
                        click_count += 1;
                    }
                }
                ImGui::EndPopup();
            }
            const int result = selected_octave * 12 + selected_note;
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
            if (ImGui::SmallButton(ALL_NOTES[result])) {
                ImGui::OpenPopup("select");
            }
            ImGui::PopStyleColor();
            return result;
        }

        void show_keys(myseq::Pattern &p) {
            const auto width = ImGui::GetContentRegionAvail().x;
            const auto height = 70.0f;
            const auto cpos = ImGui::GetCursorPos() - ImVec2(
                    ImGui::GetScrollX(),
                    ImGui::GetScrollY());
            auto dl = ImGui::GetWindowDrawList();
            const auto octaves = 10;
            const auto octave_width = width / (float) octaves;
            const auto key_width = octave_width / 12.0f;
            const auto white_key_width = octave_width / 7.0f;
            dl->AddRectFilled(cpos, cpos + ImVec2(width, height), IM_COL32_WHITE);
            dl->AddRect(cpos, cpos + ImVec2(width, height), IM_COL32_BLACK);
            const auto selected_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TextSelectedBg));
            const auto border_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border));
            const auto border_shadow_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_BorderShadow));
            auto button_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Button));
            button_color.Value.w = 1.0f;
            const auto text_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
            const auto style = ImGui::GetStyle();

            const auto selection_xy = cpos + ImVec2((float) p.first_note * key_width, 0.0);
            const auto selection_zw = cpos + ImVec2((float) p.last_note * key_width, height);

            //ImGui::RenderFrame(selection_xy, selection_zw, ImColor(255, 0, 0), true, 2.0);
            dl->AddRectFilled(selection_xy, selection_zw, selected_color);
            dl->AddRect(selection_xy, selection_zw, IM_COL32_WHITE);

            //dl->AddText(p1, IM_COL32_BLACK, ALL_NOTES[p.first_note]);

            const float black_keys[] = {0.0, 1.0, 3.0, 4.0, 5.0};
            for (int i = 0; i < octaves; i++) {
                const auto xy = cpos + ImVec2((float) i * octave_width, 0.0f);
                for (int j = 0; j < 7; j++) {
                    const auto a = xy + ImVec2((float) j * white_key_width, 0.0f);
                    const auto b = a + ImVec2(0.0f, height);
                    dl->AddLine(a, b, IM_COL32_BLACK);
                }
                const auto t1 = xy + ImVec2(white_key_width * 0.75f, 0.0f);
                const auto t2 = xy + ImVec2(white_key_width * 1.25f, height * 0.75f);
                for (float c: black_keys) {
                    dl->AddRectFilled(t1 + ImVec2(white_key_width * c, 0.0f), t2 + ImVec2(white_key_width * c, 0.0f),
                                      IM_COL32_BLACK);
                }
                const auto nr = std::to_string(i - 1);
                const char *s = nr.c_str();
                const char *e = s + nr.length();
                const auto sz = ImGui::CalcTextSize(s, e);
                dl->AddText(xy + ImVec2(white_key_width * 0.5f - sz.x * 0.5f, height - sz.y - 4.0f), IM_COL32_BLACK, s,
                            e);
            }
            const auto sdz = ImGui::CalcTextSize(ALL_NOTES[p.first_note]);
            const auto sdzp = sdz + style.FramePadding * 2.0f;
            const auto qdz = ImGui::CalcTextSize(ALL_NOTES[p.last_note]);
            const auto qdzp = qdz + style.FramePadding * 2.0f;
            const auto p1 = selection_xy + ImVec2(0.0, height - sdzp.y);
            const auto p2 = p1 + sdzp;
            const auto q1 = selection_xy + ImVec2(selection_zw.x - selection_xy.x - qdzp.x, 0.0f) +
                            ImVec2(0.0, height - qdzp.y);
            const auto q2 = q1 + qdzp;
            dl->AddRectFilled(p1, p2, button_color);
            dl->AddRect(p1, p2, IM_COL32_BLACK);
            dl->AddText((p1 + p2) * 0.5f - sdz * 0.5f, text_color, ALL_NOTES[p.first_note]);
            dl->AddRectFilled(q1, q2, button_color);
            dl->AddRect(q1, q2, IM_COL32_BLACK);
            dl->AddText((q1 + q2) * 0.5f - qdz * 0.5f, text_color, ALL_NOTES[p.last_note]);
            ImGui::Dummy(ImVec2(width, height));

            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    if (interaction == Interaction::None) {
                        interaction = Interaction::KeysSelect;
                        drag_started_mpos = ImGui::GetMousePos();
                    }
                    const auto mpos = ImGui::GetMousePos();
                    const auto note1 = std::max(0, (int) std::floor((mpos.x - cpos.x) / key_width));
                    const auto note2 = std::max(0, (int) std::floor((drag_started_mpos.x - cpos.x) / key_width));
                    if (note1 < note2) {
                        p.first_note = note1;
                        p.last_note = note2;
                    } else {
                        p.first_note = note2;
                        p.last_note = note1;
                    }
                } else if (ImGui::BeginTooltip()) {
                    const auto note = (int) std::floor((ImGui::GetMousePos().x - cpos.x) / key_width);
                    ImGui::Text("Note: %s", ALL_NOTES[note]);
                    ImGui::EndTooltip();
                }
            }
        }

        bool
        grid_interaction(myseq::Pattern &p, const ImVec2 &grid_cpos, const ImVec2 &grid_size, const ImVec2 &cell_size) {
            auto ctrl_held = ImGui::GetIO().KeyCtrl;
            auto mpos = ImGui::GetMousePos();
            auto cell = calc_cell(p, grid_cpos, mpos, grid_size, cell_size);
            auto valid_cell = cell.x >= 0 && cell.y >= 0;
            bool dirty = false;
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
                                    (int) std::round((float) drag_started_velocity - (float) delta_y), 0, 127);
                            p.set_velocity(drag_started_cell, new_vel);
                            dirty = true;
                        }
                    } else {
                        interaction = Interaction::None;
                    }
                    break;
                }
                case Interaction::KeysSelect:
                    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        interaction = Interaction::None;
                    }
                    break;

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
            return dirty;
        }

        void onImGuiDisplay() override {
            // ImGui::SetNextWindowPos(ImVec2(0, 0));
            //ImGui::SetNextWindowSize(ImVec2(static_cast<float>(getWidth()), static_cast<float>(getHeight())));

            int window_flags =
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);

            float cell_padding = 4.0;
            bool dirty = false;
            bool create = false;
            bool delete_ = false;
            ImVec2 cell_padding_xy = ImVec2(cell_padding, cell_padding);
            if (state.num_patterns() == 0) {
                state.selected = state.create_pattern().id;
                dirty = true;
            }
            auto &p = state.get_selected_pattern();

            if (ImGui::Begin("MySeq", nullptr, window_flags)) {

                float width = ImGui::CalcItemWidth();
                const auto active_cell = ImColor(0x5a, 0x8a, 0xcf);
                const auto inactive_cell = ImColor(0x45, 0x45, 0x45);
                const auto hovered_color = ImColor(IM_COL32_WHITE);
                auto cpos = ImGui::GetCursorPos() - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
                auto cell_size = ImVec2(width / (float) p.width, 30.0f);
                auto height = cell_size.y * (float) visible_rows;
                const auto grid_size = ImVec2(width, height);
                auto mpos = ImGui::GetMousePos();
                auto cell = calc_cell(p, cpos, mpos, grid_size, cell_size);
                auto valid_cell = cell.x >= 0 && cell.y >= 0;
                auto *draw_list = ImGui::GetWindowDrawList();
                auto border_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)).operator ImU32();
                auto strong_border_color = ImColor(
                        ImGui::GetStyleColorVec4(ImGuiCol_TableBorderStrong)).operator ImU32();
                auto alt_held = ImGui::GetIO().KeyAlt;

                const auto corner = (ImVec2(0.0, 0.0) - offset) / cell_size;
                const auto corner2 = corner + ImVec2(width, height) / cell_size;
                const auto first_visible_row = (int) std::floor(corner.y);
                const auto last_visible_row = std::min(p.height - 1, (int) std::floor(corner2.y));
                const auto first_visible_col = std::max(0, (int) std::floor(corner.x));
                const auto last_visible_col = std::min(p.width - 1, (int) std::floor(corner2.x));

                draw_list->PushClipRect(cpos, cpos + ImVec2(width, height), true);

                for (auto i = first_visible_row; i <= last_visible_row; i++) {
                    for (auto j = first_visible_col; j <= last_visible_col; j++) {
                        auto loop_cell = V2i(j, i);
                        auto p_min = ImVec2(cell_size.x * (float) loop_cell.x,
                                            cell_size.y * (float) loop_cell.y) +
                                     offset + cpos;
                        auto p_max = p_min + ImVec2(cell_size.x, cell_size.y) - cell_padding_xy;
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

                        auto note = myseq::utils::row_index_to_midi_note(loop_cell.y);
                        if ((alt_held || note % 12 == 0) && loop_cell.x == 0) {
                            draw_list->AddText(p_min, IM_COL32_WHITE, ALL_NOTES[note]);
                        } else if (vel > 0) {
                            draw_list->AddText(p_min, IM_COL32_WHITE, ONE_TO_256[vel - 1]);
                        }
                    }
                }

                draw_list->PopClipRect();
                draw_list->AddRect(cpos, cpos + ImVec2(width, height), border_color);
                ImGui::Dummy(ImVec2(width, height));

                const auto left = std::min(0.0f, 0.0f - ((cell_size.x * (float) p.width) - width));
                const auto right = 0.0f;
                const auto top = std::min(0.0f, 0.0f - ((cell_size.y * (float) p.height) - height));
                const auto bottom = 0.0f;
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    offset += ImGui::GetIO().MouseDelta;
                    offset.x = std::clamp(offset.x, left, right);
                    offset.y = std::clamp(offset.y, top, bottom);
                }

                dirty = dirty | grid_interaction(p, cpos, grid_size, cell_size);

                ImGui::SameLine();
                ImGui::BeginGroup();
                if (ImGui::Button("Add")) {
                    dirty = true;
                    create = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete")) {
                    dirty = true;
                    delete_ = true;
                }

                int patterns_table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
                // static ImGuiTableFlags flags =
                //         ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
                //         | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti
                //         | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
                //         | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
                //         | ImGuiTableFlags_SizingFixedFit;
                if (ImGui::BeginTable("##patterns_table", 4, patterns_table_flags)) {
                    ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_None, 0.0, 0);
                    ImGui::TableSetupColumn("length", ImGuiTableColumnFlags_None, 0.0, 1);
                    ImGui::TableSetupColumn("first note", ImGuiTableColumnFlags_None, 0.0, 2);
                    ImGui::TableSetupColumn("last note", ImGuiTableColumnFlags_None, 0.0, 3);
                    ImGui::TableHeadersRow();
                    state.each_pattern([&](const myseq::Pattern &pp) {
                        ImGui::TableNextRow();

                        // id
                        ImGui::TableNextColumn();
                        int flags = ImGuiSelectableFlags_None;
                        const auto id = pp.id;
                        if (ImGui::Selectable(std::to_string(id).c_str(), state.selected == id, flags)) {
                            state.selected = id;
                            dirty = true;
                        }

                        // length
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", std::to_string(pp.width).c_str());

                        // first_note
                        ImGui::TableNextColumn();
                        ImGui::PushID(id);
                        myseq::Pattern &tmp = state.get_pattern(id);
                        ImGui::PushID(1);
                        const auto first_note = note_select(tmp.first_note);
                        if (tmp.first_note != first_note) {
                            tmp.first_note = first_note;
                            dirty = true;
                        }
                        ImGui::PopID();

                        // last_note
                        ImGui::TableNextColumn();
                        ImGui::PushID(2);
                        const auto last_note = note_select(tmp.last_note);
                        if (tmp.last_note != last_note) {
                            tmp.last_note = last_note;
                            dirty = true;
                        }
                        ImGui::PopID();
                        ImGui::PopID();
                    });
                    ImGui::EndTable();
                }

                ImGui::EndGroup();

                int pattern_width_slider_value = p.width;
                if (ImGui::SliderInt("##pattern_width", &pattern_width_slider_value, 1, 32, nullptr,
                                     ImGuiSliderFlags_None)) {
                    p.resize_width(pattern_width_slider_value);
                    dirty = true;
                }

                show_keys(p);
                //p.first_note = note_select("First note", p.first_note);
                //ImGui::SameLine();
                //p.last_note = note_select("Last note", p.last_note);
                //ImGui::PopID();

                const TimePosition &t = ((MySeqPlugin *) getPluginInstancePointer())->last_time_position;
                const double sr = ((MySeqPlugin *) getPluginInstancePointer())->getSampleRate();
                const myseq::TimePositionCalc &tc = myseq::TimePositionCalc(t, sr);
                const auto ptr = ((MySeqPlugin *) getPluginInstancePointer());
                int i = 0;
                for (const auto kv: ptr->player2.active_patterns) {
                    ImGui::Text("%d: ap id=%d start=%f", i++, kv.second.pattern_id, kv.second.start_tick);
                }

                /*
                ImGui::Text("t.frame=%llu", t.frame);
                ImGui::Text("t.playing=%d", t.playing);
                ImGui::Text("t.bbt.valid=%d", t.bbt.valid);
                ImGui::Text("t.bbt.bar=%d", t.bbt.bar);
                ImGui::Text("t.bbt.beat=%d", t.bbt.beat);
                ImGui::Text("t.bbt.tick=%f", t.bbt.tick);
                ImGui::Text("t.bbt.tick+t.barStartTick=%f", t.bbt.tick + t.bbt.barStartTick);
                ImGui::Text("tc.global_tick()=%f", tc.global_tick());
                ImGui::Text("t.bbt.barStartTick=%f", t.bbt.barStartTick);
                ImGui::Text("t.bbt.beatsPerBar=%f", t.bbt.beatsPerBar);
                ImGui::Text("t.bbt.beatType=%f", t.bbt.beatType);
                ImGui::Text("t.bbt.ticksPerBeat=%f", t.bbt.ticksPerBeat);
                ImGui::Text("t.bbt.beatsPerMinute=%f", t.bbt.beatsPerMinute);
                ImGui::Text("publish count %d", publish_count);
                ImGui::Text("cell %d %d", cell.x, cell.y);
                ImGui::Text("cpos %f %f", cpos.x, cpos.y);
                ImGui::Text("first_note %d %d", p.first_note, p.last_note);
                ImGui::Text("drag_started_velocity=%d", drag_started_velocity);
                ImGui::Text("first_visible_col=%d", first_visible_col);
                ImGui::Text("last_visible_col=%d", last_visible_col);
                ImGui::Text("first_visible_row=%d", first_visible_row);
                ImGui::Text("last_visible_row=%d", last_visible_row);
                ImGui::Text("corner=%f %f", corner.x, corner.y);
                ImGui::Text("valid_cell=%d", valid_cell);
                ImGui::Text("interaction=%s", interaction_to_string(interaction));
                */
            }

            ImGui::End();
            if (create) {
                state.selected = state.create_pattern().id;
            }
            if (delete_) {
                state.delete_pattern(state.selected);
            }
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
