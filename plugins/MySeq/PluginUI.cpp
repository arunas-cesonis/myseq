#include <algorithm>
#include <unordered_set>
#include <cassert>
#include <stack>
#include "DistrhoUI.hpp"
#include "PluginDSP.hpp"
#include "Patterns.hpp"
#include "Numbers.hpp"
#include "Notes.hpp"
#include "TimePositionCalc.hpp"

START_NAMESPACE_DISTRHO

// #define SET_DIRTY() { d_debug("dirty: %d", __LINE__);  dirty = true; }
#define SET_DIRTY_PUSH_UNDO(descr) {   dirty = true; PUSH_UNDO(descr); }
#define SET_DIRTY() {   dirty = true; }
#define PUSH_UNDO(descr) {   d_debug("push_undo: %s", descr); push_undo(); }

// --------------------------------------------------------------------------------------------------------------------

    using myseq::V2i;

    static const char *NOTES[] =
            {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", nullptr,};

    struct SelectionRectangle {
        ImVec2 p_min;
        ImVec2 p_max;
        V2i cell_min;
        V2i cell_max;
    };

    template<typename T>
    T clamp(T value, T min, T max) {
        return std::min(std::max(value, min), max);
    }

    ImColor clamp_color(ImColor color) {
        return {
                clamp(color.Value.x, 0.0f, 1.0f),
                clamp(color.Value.y, 0.0f, 1.0f),
                clamp(color.Value.z, 0.0f, 1.0f),
                clamp(color.Value.w, 0.0f, 1.0f)
        };
    }

    ImColor color_rgba_to_rbga(ImColor color) {
        return {
                color.Value.x,
                color.Value.z,
                color.Value.y,
                color.Value.w
        };
    }

    ImColor mono_color(ImColor color) {
        auto avg = (color.Value.x + color.Value.y + color.Value.z) / 3.0f;
        return {
                avg,
                avg,
                avg,
                color.Value.w
        };
    }

    ImColor add_to_rgb(ImColor color, float amount) {
        return clamp_color({
                                   color.Value.x + amount,
                                   color.Value.y + amount,
                                   color.Value.z + amount,
                                   color.Value.w
                           });
    }

    ImColor scale_rgb(ImColor color, float amount) {
        return clamp_color({
                                   color.Value.x * amount,
                                   color.Value.y * amount,
                                   color.Value.z * amount,
                                   color.Value.w
                           });
    }

    namespace ipc = boost::interprocess;

    class MySeqUI : public UI {
    public:
        uint8_t drag_started_velocity = 0;
        bool drag_started_active = false;
        std::vector<std::pair<V2i, uint8_t>> drag_started_velocity_vec;
        std::unordered_set<V2i, myseq::V2iHash> moving_cells_set;
        V2i drag_started_cell;
        V2i previous_move_offset;
        bool drag_started_selected = false;

        ImVec2 drag_started_mpos;
        int count = 0;
        ImVec2 offset;
        static constexpr int visible_rows = 24;
        static constexpr int visible_columns = 32;
        float default_cell_width = 20.0f;
        float default_cell_height = 20.0f;
        std::vector<myseq::Cell> clipboard;
        std::string instance_id = myseq::utils::gen_instance_id();

        bool show_metrics = false;

        std::optional<myseq::Stats> stats;
        myseq::State state;
        std::stack<myseq::State> undo_stack{};

        enum class Interaction {
            None,
            DrawingCells,
            DrawingLongCell,
            AdjustingVelocity,
            AdjustingVelocitySelected,
            KeysSelect,
            DragSelectingCells,
            RectSelectingCells,
            MovingCells,
        };
        Interaction interaction = Interaction::None;

        MySeqUI()
                : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
            const double scaleFactor = getScaleFactor();

            myseq::test_serialize();
            // init_shm();

            offset = ImVec2(0.0f, -default_cell_height * (float) (visible_rows + 72));

            if (d_isEqual(scaleFactor, 1.0)) {
                setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT);
            } else {
                const uint width = DISTRHO_UI_DEFAULT_WIDTH * scaleFactor;
                const uint height = DISTRHO_UI_DEFAULT_HEIGHT * scaleFactor;
                setGeometryConstraints(width, height);
                setSize(width, height);
            }

            gen_array_tests();
            setState("instance_id", String(instance_id.c_str()));
        }

        int publish_count = 0;
        int publish_last_bytes = 0;

        void publish() {
            auto s = state.to_json_string();
            publish_last_bytes = (int) s.length();
            d_debug("PluginUI: setSatte key=pattern value=%s", s.c_str());
            setState("pattern", s.c_str());
// #ifdef DEBUG
            {
                state = myseq::State::from_json_string(s.c_str());
            }
// #endif
            publish_count += 1;
        }

    protected:

        static const char *interaction_to_string(Interaction i) {
            switch (i) {
                case Interaction::None:
                    return "None";
                case Interaction::DrawingCells:
                    return "DrawingCells";
                case Interaction::DrawingLongCell:
                    return "DrawingLongCell";
                case Interaction::AdjustingVelocity:
                    return "AdjustingVelocity";
                case Interaction::AdjustingVelocitySelected:
                    return "AdjustingVelocitySelected";
                case Interaction::KeysSelect:
                    return "KeysSelect";
                case Interaction::DragSelectingCells:
                    return "DragSelectingCells";
                case Interaction::RectSelectingCells:
                    return "RectSelectingCells";
                case Interaction::MovingCells:
                    return "MovingCells";
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

        void push_undo() {
            undo_stack.push(state);
        }

        void pop_undo() {
            if (undo_stack.size() > 1) {
                undo_stack.pop();
                state = undo_stack.top();
            }
        }

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

        void
        grid_copy(const myseq::Pattern &p) {
            clipboard.clear();
            V2i top_left(p.width, p.height);
            p.each_selected_cell([&](const myseq::Cell &cell) {
                top_left.x = std::min(cell.position.x, top_left.x);
                top_left.y = std::min(cell.position.y, top_left.y);
                clipboard.emplace_back(cell);
            });
            for (auto &i: clipboard) {
                i.position -= top_left;
            }
        }

        void
        grid_paste(bool &dirty, myseq::Pattern &p) {
            p.deselect_all();
            p.put_cells(clipboard, p.cursor);
            SET_DIRTY_PUSH_UNDO("paste");
        }

        void
        grid_keyboard_interaction_ctrl(bool &dirty, myseq::Pattern &p) {
            if (key_pressed(ImGuiKey_C)) {
                grid_copy(p);
            } else if (key_pressed(ImGuiKey_V)) {
                grid_paste(dirty, p);
            } else if (key_pressed(ImGuiKey_A)) {
                p.each_cell([&](const myseq::Cell &c) {
                    p.set_selected(c.position, true);
                });
                SET_DIRTY();
            }
        }

        void
        general_keyboard_interaction(bool &dirty) {
            if (key_pressed(ImGuiKey_U)) {
                pop_undo();
                SET_DIRTY();
            }
        }

        void
        grid_keyboard_interaction_no_mod(bool &dirty, myseq::Pattern &p) {
            auto shift_held = ImGui::GetIO().KeyShift;
            if (key_pressed(ImGuiKey_Backspace) || key_pressed(ImGuiKey_Delete)) {
                p.each_selected_cell([&](const myseq::Cell &c) {
                    p.clear_cell(c.position);
                });
                SET_DIRTY_PUSH_UNDO("delete");
            } else if (key_pressed(ImGuiKey_Y)) {
                grid_copy(p);
            } else if (key_pressed(ImGuiKey_P)) {
                grid_paste(dirty, p);
            } else if (key_pressed_re(ImGuiKey_K)) {
                if (p.cursor.y > 0) {
                    p.cursor.y -= 1;
                    SET_DIRTY();
                }
            } else if (key_pressed_re(ImGuiKey_J)) {
                if (p.cursor.y + 1 < p.height) {
                    p.cursor.y += 1;
                    SET_DIRTY();
                }
            } else if (key_pressed_re(ImGuiKey_H)) {
                if (p.cursor.x > 0) {
                    p.cursor.x -= 1;
                    SET_DIRTY();
                }
            } else if (key_pressed_re(ImGuiKey_L)) {
                if (p.cursor.x + 1 < p.width) {
                    p.cursor.x += 1;
                    SET_DIRTY();
                }
            } else {
                const int updown = shift_held ? 12 : 1;
                // For some reason CTRL+A makes A stuck
                const bool no_repeat = false;
                const int dy =
                        +(key_pressed(ImGuiKey_UpArrow) ? -updown : 0)
                        + (key_pressed(ImGuiKey_DownArrow) ? updown : 0);
                const int dx =
                        +(key_pressed(ImGuiKey_LeftArrow) ? -1 : 0)
                        + (key_pressed(ImGuiKey_RightArrow) ? 1 : 0);
                const V2i d(dx, dy);
                if (d != V2i(0, 0)) {
                    p.move_selected_cells(d);
                    SET_DIRTY_PUSH_UNDO("keyboard move");
                }
            }
        }

        void
        grid_keyboard_interaction(bool &dirty, myseq::Pattern &p) {
            if (cmd_held()) {
                grid_keyboard_interaction_ctrl(dirty, p);
            } else {
                grid_keyboard_interaction_no_mod(dirty, p);
            }
        }

        static bool cmd_held() {
            return 0 != (ImGui::GetIO().KeyMods & ImGuiMod_Super);
        }

        static bool key_pressed(ImGuiKey key) {
            const bool repeat = false;
            return ImGui::IsKeyPressed(key, repeat);
        }

        static bool key_pressed_re(ImGuiKey key) {
            const bool repeat = true;
            return ImGui::IsKeyPressed(key, repeat);
        }

        static bool key_down(ImGuiKey key) {
            return ImGui::IsKeyDown(key);
        }

        void
        grid_interaction(bool &dirty, myseq::Pattern &p, const ImVec2 &grid_cpos, const ImVec2 &grid_size,
                         const ImVec2 &cell_size, const V2i &mcell
        ) {
            auto shift_held = ImGui::GetIO().KeyShift;
            auto mpos = ImGui::GetMousePos();
            auto cursor_hovers_grid = mcell.x >= 0 && mcell.y >= 0;
            auto cursor_hovers_active_cell = cursor_hovers_grid && (p.get_velocity(mcell) > 0);
            switch (interaction) {
                case Interaction::DrawingCells: {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            auto active = p.is_active(mcell);
                            if (active == drag_started_active) {
                                p.set_active(mcell, !active);
                            }
                            if (p.cursor != mcell) {
                                p.cursor = mcell;
                                SET_DIRTY();
                            }
                        }
                    } else {
                        interaction = Interaction::None;
                        PUSH_UNDO("DrawingCells");
                    }
                    break;
                }

                case Interaction::DrawingLongCell: {
                    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        const auto x1 = std::min(mcell.x, drag_started_cell.x);
                        const auto x2 = std::max(mcell.x, drag_started_cell.x);
                        const auto y = drag_started_cell.y;
                        const auto c = V2i(x1, y);
                        const auto n = x2 - x1 + 1;
                        d_debug("length %d", n);
                        p.set_velocity(c, 127);
                        p.set_length(c, n);
                        interaction = Interaction::None;
                        SET_DIRTY_PUSH_UNDO("DrawingLongCell");
                    }
                    break;
                }

                case Interaction::AdjustingVelocitySelected: {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            auto delta_y = mpos.y - drag_started_mpos.y;
                            for (auto &pair: drag_started_velocity_vec) {
                                auto new_vel2 = (uint8_t) clamp(
                                        (int) std::round((float) pair.second - (float) delta_y), 0, 127);
                                p.set_velocity(pair.first, new_vel2, "AdjustingVelocity B");
                            }
                            SET_DIRTY();
                        }
                    } else {
                        interaction = Interaction::None;
                        PUSH_UNDO("AdjustingVelocitySelected");
                    }
                    break;
                }

                case Interaction::AdjustingVelocity: {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            auto delta_y = mpos.y - drag_started_mpos.y;
                            auto new_vel = (uint8_t) clamp(
                                    (int) std::round((float) drag_started_velocity - (float) delta_y), 0, 127);
                            p.set_velocity(drag_started_cell, new_vel, "AdjustingVelocity A");
                            SET_DIRTY();
                        }
                    } else {
                        interaction = Interaction::None;
                        PUSH_UNDO("AdjustingVelocity");
                    }
                    break;
                }
                case Interaction::MovingCells:
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            const auto move_offset = mcell - drag_started_cell;
                            if (previous_move_offset != move_offset) {
                                int left = 0;
                                int right = 0;
                                int top = 0;
                                int bottom = 0;
                                for (auto &v: moving_cells_set) {
                                    auto w = v + move_offset;
                                    left = std::min(left, w.x);
                                    top = std::min(top, w.y);
                                    right = std::max(right, w.x);
                                    bottom = std::max(bottom, w.y);
                                }
                                assert(left < 0 ? right < p.get_width() : true);
                                assert(right >= p.get_width() ? left >= 0 : true);
                                assert(top < 0 ? bottom < p.get_height() : true);
                                assert(bottom >= p.get_height() ? top >= 0 : true);
                                V2i adjust = V2i(0, 0);
                                if (left < 0) {
                                    adjust.x = -left;
                                } else if (right >= p.get_width()) {
                                    adjust.x = -(right - p.get_width() + 1);
                                }
                                if (top < 0) {
                                    adjust.y = -top;
                                } else if (bottom >= p.get_height()) {
                                    adjust.y = -(bottom - p.get_height() + 1);
                                }
                                previous_move_offset = move_offset + adjust;
                            }
                        }
                    } else {
                        if (previous_move_offset != V2i(0, 0)) {
                            p.move_selected_cells(previous_move_offset);
                            SET_DIRTY_PUSH_UNDO("MovingCells");
                        }
                        interaction = Interaction::None;
                    }
                    break;
                case Interaction::KeysSelect:
                    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        interaction = Interaction::None;
                    }
                    break;
                case Interaction::RectSelectingCells:
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        //
                    } else {
                        const auto sr = selection_rectangle(p, mpos, grid_cpos, grid_size, cell_size);
                        p.deselect_all();
                        for (int x = sr.cell_min.x; x <= sr.cell_max.x; x++)
                            for (int y = sr.cell_min.y; y <= sr.cell_max.y; y++) {
                                const auto v = V2i(x, y);
                                p.set_selected(v, true);
                            }

                        interaction = Interaction::None;
                        SET_DIRTY();
                    }
                    break;
                case Interaction::DragSelectingCells:
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                            if (cursor_hovers_grid) {
                                p.set_selected(mcell, !drag_started_selected);
                                SET_DIRTY();
                            }
                        }
                    } else {
                        interaction = Interaction::None;
                        SET_DIRTY();
                    }

                case Interaction::None:
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        if (cursor_hovers_grid) {
                            if (cursor_hovers_active_cell) {
                                drag_started_selected = p.get_selected(mcell);
                                p.set_selected(mcell, !drag_started_selected);
                                interaction = Interaction::DragSelectingCells;
                            } else {
                                drag_started_mpos = mpos;
                                interaction = Interaction::RectSelectingCells;
                                p.cursor.x = mcell.x;
                                p.cursor.y = mcell.y;
                            }
                            SET_DIRTY();
                        }
                    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            if (ImGui::IsKeyDown(ImGuiKey_T)) {
                                interaction = Interaction::DrawingLongCell;
                                // state.get_selected_pattern().set_velocity(mcell, 127,
                                //                                         "ImGuiKey_T init DrawingLongCell");
                                drag_started_mpos = mpos;
                                drag_started_cell = mcell;
                            } else if (cmd_held()) {
                                if (p.num_selected() > 0) {
                                    drag_started_velocity_vec.clear();
                                    p.each_selected_cell([&](const myseq::Cell &c) {
                                        drag_started_velocity_vec.push_back({c.position, c.velocity});
                                    });
                                    interaction = Interaction::AdjustingVelocitySelected;
                                } else {

                                    drag_started_velocity = p.get_velocity(mcell);
                                    drag_started_velocity = drag_started_velocity == 0 ? 127 : drag_started_velocity;
                                    interaction = Interaction::AdjustingVelocity;
                                }
                                drag_started_mpos = mpos;
                                drag_started_cell = mcell;
                            } else if (p.get_selected(mcell)) {
                                drag_started_mpos = mpos;
                                drag_started_cell = mcell;
                                previous_move_offset = V2i(0, 0);
                                moving_cells_set.clear();
                                p.each_selected_cell([&](const myseq::Cell &c) {
                                    moving_cells_set.insert(c.position);
                                });
                                interaction = Interaction::MovingCells;
                            } else {
                                interaction = Interaction::DrawingCells;
                                drag_started_active = p.is_active(mcell);
                                p.set_active(mcell, !drag_started_active);
                                p.deselect_all();
                                SET_DIRTY();
                            }
                        }
                    }
            }
        }

        void grid_viewport_pan_to_cursor(const ImVec2 &cell_size, const ImVec2 &grid_size, myseq::Pattern &p) {
            ImVec2 viewport = ImVec2(0.0f, 0.0f) - offset;
            const auto cursor = ImVec2((float) p.cursor.x * cell_size.x, (float) p.cursor.y * cell_size.y);
            if (cursor.y < viewport.y) {
                viewport.y = cursor.y;
            }
            if (cursor.y + cell_size.y > viewport.y + grid_size.y) {
                viewport.y = cursor.y + cell_size.y - grid_size.y;
            }
            if (cursor.x < viewport.x) {
                viewport.x = cursor.x;
            }
            if (cursor.x + cell_size.x > viewport.x + grid_size.x) {
                viewport.x = cursor.x + cell_size.x - grid_size.x;
            }
            offset = ImVec2(0.0f, 0.0f) - viewport;
        }

        void grid_viewport_mouse_pan() {
            if (
                    (ImGui::GetIO().KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
                    || (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))

                    ) {
                offset += ImGui::GetIO().MouseDelta;
            }
        }

        void grid_viewport_limit_panning(const ImVec2 &cell_size, const ImVec2 &grid_size, myseq::Pattern &p) {
            const auto left = std::min(0.0f, 0.0f - ((cell_size.x * (float) p.get_width()) - grid_size.x));
            const auto right = 0.0f;
            const auto top = std::min(0.0f, 0.0f - ((cell_size.y * (float) p.get_height()) - grid_size.y));
            const auto bottom = 0.0f;
            offset.x = std::clamp(offset.x, left, right);
            offset.y = std::clamp(offset.y, top, bottom);
        }

        static bool is_black_key(int note) {
            switch (note % 12) {
                case 1:
                    return true;
                case 3:
                    return true;
                case 6:
                    return true;
                case 8:
                    return true;
                case 10:
                    return true;
                default:
                    return false;
            }
        }

        void show_grid(bool &dirty) {
            auto &p = state.get_selected_pattern();
            const auto &aps = get_pattern_stats(p.get_id());
            float cell_padding = 4.0;
            ImVec2 cell_padding_xy = ImVec2(cell_padding, cell_padding);
            float grid_width = ImGui::CalcItemWidth();
            const auto active_cell = ImColor(0x7a, 0xaa, 0xef);
            const auto inactive_cell = ImColor(0x25, 0x25, 0x25);
            const auto hovered_color = ImColor(IM_COL32_WHITE);
            auto grid_cpos = ImGui::GetCursorPos() - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());

            // fit at least visible_columns horizontally so that
            // most common pattern setup only need to be scrolled vertically
            auto cell_width =
                    grid_width / default_cell_width >= ((float) visible_columns) ? default_cell_width : grid_width /
                                                                                                        (float) visible_columns;

            auto cell_size = ImVec2(cell_width, default_cell_height);
            // auto cell_size = ImVec2(grid_width / (float) p.width, default_cell_height);
            auto grid_height = cell_size.y * (float) visible_rows;
            const auto grid_size = ImVec2(grid_width, grid_height);
            auto mpos = ImGui::GetMousePos();
            auto mcell = calc_cell(p, grid_cpos, mpos, grid_size, cell_size);
            auto *draw_list = ImGui::GetWindowDrawList();
            auto default_border_color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)).operator ImU32();
            auto alt_held = ImGui::GetIO().KeyAlt;

            bool cursor_dirty = false;
            grid_keyboard_interaction(cursor_dirty, p);
            dirty = dirty || cursor_dirty;

            const auto corner = (ImVec2(0.0, 0.0) - offset) / cell_size;
            const auto corner2 = corner + ImVec2(grid_width, grid_height) / cell_size;
            const auto first_visible_row = (int) std::floor(corner.y);
            const auto last_visible_row = std::min(p.height - 1, (int) std::floor(corner2.y));
            const auto first_visible_col = std::max(0, (int) std::floor(corner.x));
            const auto last_visible_col = std::min(p.width - 1, (int) std::floor(corner2.x));

            draw_list->PushClipRect(grid_cpos, grid_cpos + ImVec2(grid_width, grid_height), true);


            int active_column = -1;
            if (aps.has_value()) {
                active_column = (int) std::floor((double) p.width * (aps->time / aps->duration));
            }

            int skip[128]{};

            for (auto j = first_visible_col; j <= last_visible_col; j++) {
                for (auto i = first_visible_row; i <= last_visible_row; i++) {
                    if (skip[i] > 0) {
                        skip[i] -= 1;
                        continue;
                    }
                    auto loop_cell = V2i(j, i);
                    auto len = p.get_length(loop_cell);
                    auto p_min = ImVec2(cell_size.x * (float) loop_cell.x,
                                        cell_size.y * (float) loop_cell.y) +
                                 offset + grid_cpos + cell_padding_xy;
                    auto p_max =
                            p_min + ImVec2(cell_size.x * (float) (len > 1 ? len : 1), cell_size.y) - cell_padding_xy;
                    auto vel = p.get_velocity(loop_cell);
                    auto sel = p.get_selected(loop_cell);
                    skip[i] = len - 1;
                    auto has_cursor = p.cursor == loop_cell;
                    auto has_mouse = mcell == loop_cell;
                    auto velocity_fade = ((float) vel) / 127.0f;
                    auto is_active = p.is_active(loop_cell);
                    ImColor cell_color1;
                    if (is_active) {
                        cell_color1 = scale_rgb(active_cell, 0.5 + velocity_fade * 0.5);
                        if (sel) {
                            //cell_color1 = clamp_color(add_to_rgb(cell_color1, 0.25));
                            auto tmp = cell_color1.Value.y;
                            cell_color1.Value.y = cell_color1.Value.z;
                            cell_color1.Value.z = tmp;
                        }
                    } else {
                        auto quarter_fade = (j % 4 == 0) ? 0.8f : 1.f;
                        cell_color1 = inactive_cell;
                        cell_color1.Value.x *= quarter_fade;
                        cell_color1.Value.y *= quarter_fade;
                        cell_color1.Value.z *= quarter_fade;
                    }

                    auto note = myseq::utils::row_index_to_midi_note(loop_cell.y);
                    //auto c = is_hovered && interaction == Interaction::None ? hovered_color : cell_color;
                    // auto quarter_fade = is_black_key(note) ? 0.8f : 1.f;
                    if (interaction == Interaction::MovingCells) {
                        if (moving_cells_set.end() != moving_cells_set.find(loop_cell - previous_move_offset)) {
                            //auto tmp = cell_color1.Value.y;
                            //cell_color1.Value.y = cell_color1.Value.z;
                            cell_color1 = IM_COL32_WHITE;
                            //cell_color1.Value.z = tmp;
                            // cell_color1 = ImColor(0x5a, 0x8a, 0xcf);
                        }
                    } else if (interaction == Interaction::DrawingLongCell) {
                        if (
                                (drag_started_cell.y == loop_cell.y) &&
                                ((drag_started_cell.x <= loop_cell.x && loop_cell.x <= mcell.x)
                                 || (mcell.x <= loop_cell.x && loop_cell.x <= drag_started_cell.x))) {
                            cell_color1 = active_cell;
                        }
                    }

                    if (j == active_column) {
                        cell_color1 = clamp_color(add_to_rgb(cell_color1, 0.25));
                    }
                    draw_list->AddRectFilled(p_min, p_max, cell_color1);
                    // Might make sense instead of checking were
                    // we are and maybe one of the things we need to draw is here
                    // draw things that are visible and necessary to draw
                    ImColor border_color;
                    if (has_cursor) {
                        border_color = IM_COL32(0xaa, 0xaa, 0xaa, 0xff);
                        draw_list->AddRect(p_min, p_max, border_color);
                    } else if (sel) {
                        border_color = IM_COL32(0xa0, 0xa0, 0xa0, 0xff);
                        draw_list->AddRect(p_min, p_max, border_color);
                    } else {
                        border_color = default_border_color;
                    };
                    if (is_active) {
                        draw_list->AddRect(p_min, p_max, border_color);
                    }

                    if ((alt_held || note % 12 == 0) && loop_cell.x == 0) {
                        draw_list->AddText(p_min, IM_COL32_WHITE, ALL_NOTES[note]);
                    } else if (sel) {
                        draw_list->AddText(p_min, IM_COL32_WHITE, ZERO_TO_256[vel]);
                    }

                    if (is_active && has_mouse && ImGui::BeginTooltip()) {
                        // V2i position;
                        // uint8_t velocity;
                        // bool selected;
                        // int length;
                        std::ostringstream oss;
                        const auto &c = p.get_cell_const_ref(loop_cell);
                        oss << "position: " << c.position.x << ":" << c.position.y << "\n";
                        oss << "velocity: " << (int) c.velocity << "\n";
                        oss << "selected: " << c.selected << "\n";
                        oss << "length: " << c.length << "\n";
                        ImGui::TextUnformatted(oss.str().c_str());
                        //ImGui::PushTextWrapPos("He"
                        //ImGui::TextUnformatted(desc/);
                        //ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }

                }
            }

            if (interaction == Interaction::RectSelectingCells) {
                const auto rect = rect_selecting(mpos);
                draw_list->AddRect(rect.first, rect.second, active_cell);
            }

            draw_list->PopClipRect();
            ImGui::Dummy(ImVec2(grid_width, grid_height));
            //
            grid_viewport_mouse_pan();
            grid_viewport_limit_panning(cell_size, grid_size, p);
            if (cursor_dirty) grid_viewport_pan_to_cursor(cell_size, grid_size, p);
            grid_interaction(dirty, p, grid_cpos, grid_size, cell_size, mcell);
        }

        [[nodiscard]] std::pair<ImVec2, ImVec2> rect_selecting(const ImVec2 &mpos) const {
            auto p_min = ImVec2(std::min(drag_started_mpos.x, mpos.x), std::min(drag_started_mpos.y, mpos.y));
            auto p_max = ImVec2(std::max(drag_started_mpos.x, mpos.x), std::max(drag_started_mpos.y, mpos.y));
            return {p_min, p_max};
        }

        [[nodiscard]] SelectionRectangle
        selection_rectangle(const myseq::Pattern &p, const ImVec2 &mpos, const ImVec2 &grid_cpos,
                            const ImVec2 &grid_size, const ImVec2 &cell_size) const {
            const auto rect = rect_selecting(mpos);
            const auto cell_min = calc_cell(p, grid_cpos, rect.first, grid_size, cell_size);
            const auto cell_max = calc_cell(p, grid_cpos, rect.second, grid_size, cell_size);
            return {rect.first, rect.second, cell_min, cell_max};
        }

        [[nodiscard]] std::optional<myseq::ActivePatternStats> get_pattern_stats(int pattern_id) const {
            std::optional<myseq::ActivePatternStats> result;
            for (const auto &aps: stats->active_patterns) {
                if (aps.pattern_id == pattern_id) {
                    result.emplace(aps);
                    break;
                }
            }
            return result;
        }

        void onImGuiDisplay() override {
            // ImGui::SetNextWindowPos(ImVec2(0, 0));
            //ImGui::SetNextWindowSize(ImVec2(static_cast<float>(getWidth()), static_cast<float>(getHeight())));

            //read_stats();
            stats = ((MySeqPlugin *) (this->getPluginInstancePointer()))->stats;


            int window_flags =
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                    | ImGuiWindowFlags_NoScrollWithMouse
                    | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize
                    | ImGuiWindowFlags_NoNavInputs;

            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);

            bool dirty = false;
            bool create = false;
            bool delete_ = false;
            bool duplicate = false;

            if (state.num_patterns() == 0) {
                state.selected = state.create_pattern().id;
                auto &cur = state.get_selected_pattern().cursor;
                cur.x = 0;
                cur.y = 127 - 24;
                SET_DIRTY_PUSH_UNDO("initial");
            }

            if (ImGui::Begin("MySeq", nullptr, window_flags)) {

                general_keyboard_interaction(dirty);

                ImGui::SetWindowFontScale(2.0);

                show_grid(dirty);

                ImGui::SameLine();
                ImGui::BeginGroup();
                if (ImGui::Button("Add")) {
                    create = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete")) {
                    delete_ = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Duplicate")) {
                    duplicate = true;
                }

                int patterns_table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
                if (ImGui::BeginTable("##patterns_table", 5, patterns_table_flags, ImVec2(0, 200))) {
                    ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_None, 0.0, 0);
                    ImGui::TableSetupColumn("length", ImGuiTableColumnFlags_None, 0.0, 1);
                    ImGui::TableSetupColumn("first note", ImGuiTableColumnFlags_None, 0.0, 2);
                    ImGui::TableSetupColumn("last note", ImGuiTableColumnFlags_None, 0.0, 3);
                    ImGui::TableSetupColumn("playing", ImGuiTableColumnFlags_None, 0.0, 3);
                    ImGui::TableHeadersRow();
                    state.each_pattern([&](const myseq::Pattern &pp) {
                        ImGui::TableNextRow();

                        // id
                        ImGui::TableNextColumn();
                        int flags = ImGuiSelectableFlags_None;
                        const auto id = pp.id;
                        if (ImGui::Selectable(std::to_string(id).c_str(), state.selected == id, flags)) {
                            state.selected = id;
                            SET_DIRTY_PUSH_UNDO("select pattern")
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
                            SET_DIRTY_PUSH_UNDO("first_note")
                        }
                        ImGui::PopID();

                        // last_note
                        ImGui::TableNextColumn();
                        ImGui::PushID(2);
                        const auto last_note = note_select(tmp.last_note);
                        if (tmp.last_note != last_note) {
                            tmp.last_note = last_note;
                            SET_DIRTY_PUSH_UNDO("last_note")
                        }
                        ImGui::PopID();
                        ImGui::PopID();

                        ImGui::TableNextColumn();
                        if (stats.has_value()) {
                            bool found = false;
                            for (const auto &aps: stats->active_patterns) {
                                if (aps.pattern_id == id) {
                                    ImGui::Text("%f / %f", aps.duration, aps.time);
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                ImGui::Text("0");
                            }
                        }
                    });
                    ImGui::EndTable();
                }


                if (ImGui::Checkbox("Play selected pattern", &state.play_selected)) {
                    SET_DIRTY_PUSH_UNDO("play_selected");
                }
                if (ImGui::GetIO().KeyAlt) ImGui::Text("Alt");
                if (ImGui::GetIO().KeyCtrl) ImGui::Text("Ctrl");
                if (cmd_held()) ImGui::Text("Cmd");

                ImGui::EndGroup();

                {
                    auto &p = state.get_selected_pattern();
                    int pattern_width_slider_value = p.width;
                    if (ImGui::SliderInt("##pattern_width", &pattern_width_slider_value, 1, 32, nullptr,
                                         ImGuiSliderFlags_None)) {
                        p.resize_width(pattern_width_slider_value);
                        SET_DIRTY_PUSH_UNDO("resize_width");
                    }
                }

                show_keys(state.get_selected_pattern());

                if (ImGui::Button("Show metrics")) {
                    show_metrics = true;
                }

                //p.first_note = note_select("First note", p.first_note);
                //ImGui::SameLine();
                //p.last_note = note_select("Last note", p.last_note);
                //ImGui::PopID();
                ImGui::BeginGroup();

                const TimePosition &t = ((MySeqPlugin *) getPluginInstancePointer())->last_time_position;
                const double sr = ((MySeqPlugin *) getPluginInstancePointer())->getSampleRate();
                const myseq::TimePositionCalc &tc = myseq::TimePositionCalc(t, sr);
                auto fps = ImGui::GetCurrentContext()->IO.Framerate;
                if (fps > 999.9) {
                    ImGui::Text("FPS=+999.9");
                } else {
                    ImGui::Text("FPS=%.1f", ImGui::GetCurrentContext()->IO.Framerate);
                }


                ImGui::Text("tick=%f", tc.global_tick());
                ImGui::Text("interaction=%s", interaction_to_string(interaction));
                int selected_cells_count = 0;
                state.get_selected_pattern().each_selected_cell(
                        [&selected_cells_count](const myseq::Cell &c) {
                            selected_cells_count += 1;
                        });

                ImGui::Text("undo length: %lu", undo_stack.size());
                ImGui::Text("key a down=%d", ImGui::IsKeyDown(ImGuiKey_A));
                ImGui::Text("key c down=%d", ImGui::IsKeyDown(ImGuiKey_C));
                ImGui::Text("key v down=%d", ImGui::IsKeyDown(ImGuiKey_V));
                ImGui::Text("instance_id=%s", instance_id.c_str());
                ImGui::Text("publish_count=%d", publish_count);
                ImGui::Text("publish_last_bytes=%d", publish_last_bytes);
                ImGui::Text("selected_cells count=%d", selected_cells_count);
                ImGui::Text("clipboard.size=%lu", clipboard.size());
                ImGui::Text("offset=%f %f", offset.x, offset.y);
                ImGui::EndGroup();
                if (stats.has_value()) {
                    ImGui::SameLine();
                    ImGui::BeginGroup();
                    // ImGui::Text("stats=0x%016lx", (unsigned long) stats);
                    ImGui::Text("stats.transport.valid=%d", stats->transport.valid);
                    ImGui::Text("stats.transport.playing=%d", stats->transport.playing);
                    ImGui::Text("stats.transport.frame=%llu", stats->transport.frame);
                    ImGui::Text("stats.transport.bar=%d", stats->transport.bar);
                    ImGui::Text("stats.transport.beat=%d", stats->transport.beat);
                    ImGui::Text("stats.transport.tick=%f", stats->transport.tick);
                    ImGui::Text("stats.transport.barStartTick=%f", stats->transport.bar_start_tick);
                    ImGui::Text("stats.transport.beatsPerBar=%f", stats->transport.beats_per_bar);
                    ImGui::Text("stats.transport.beatType=%f", stats->transport.beat_type);
                    ImGui::Text("stats.transport.ticksPerBeat=%f", stats->transport.ticks_per_beat);
                    ImGui::Text("stats.transport.beatsPerMinute=%f", stats->transport.beats_per_minute);
                    ImGui::EndGroup();
                }

            }


            if (show_metrics) {
                ImGui::ShowMetricsWindow(&show_metrics);
            }

            ImGui::End();
            if (create) {
                state.selected = state.create_pattern().id;
                SET_DIRTY_PUSH_UNDO("create");
            }
            if (delete_) {
                state.delete_pattern(state.selected);
                SET_DIRTY_PUSH_UNDO("delete");
            }
            if (duplicate) {
                state.selected = state.duplicate_pattern(state.selected).id;
                SET_DIRTY_PUSH_UNDO("duplicate");
            }
            if (dirty) {
                publish();
            }
        }

        void idleCallback() override {
            repaint();
        }

        void stateChanged(const char *key, const char *value) override {
            d_debug("PluginUI: stateChanged key=%s value=%s", key, value);
            if (std::strcmp(key, "pattern") == 0) {
                state = myseq::State::from_json_string(value);
            } else if (std::strcmp(key, "instance_id") == 0) {
                d_debug("PluginUI: stateChanged instance_id=%s new=%s", instance_id.c_str(), value);
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
