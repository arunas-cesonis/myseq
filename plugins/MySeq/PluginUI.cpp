#include <algorithm>
#include <stack>
#include <unordered_set>
#include "DistrhoUI.hpp"
#include "PluginDSP.hpp"
#include "Patterns.hpp"
#include "Numbers.hpp"
#include "Notes.hpp"
#include "TimePositionCalc.hpp"

START_NAMESPACE_DISTRHO

// #define SET_DIRTY() { d_debug("dirty: %d", __LINE__);  dirty = true; }
#define SET_DIRTY() {   dirty = true; }
#define PUSH {  stack.push(state); }
#define POP {  stack.pop(); }

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

    // ImColor invert_color(ImColor color) {
    //     return {
    //             1.0f - color.Value.x,
    //             1.0f - color.Value.y,
    //             1.0f - color.Value.z,
    //             color.Value.w
    //     };
    // }


    namespace ipc = boost::interprocess;

    class MySeqUI : public UI {
    public:
        std::stack<myseq::State> undo_stack = {};
        uint8_t drag_started_velocity = 0;
        std::vector<std::pair<V2i, uint8_t>> drag_started_velocity_vec;
        std::unordered_set<V2i, myseq::V2iHash> moving_cells_set;
        V2i drag_started_cell;
        V2i previous_move_offset;
        bool drag_started_selected = false;
        ImVec2 drag_started_mpos;
        int count = 0;
        ImVec2 offset;
        static constexpr int visible_rows = 12;
        static constexpr int visible_columns = 32;
        float default_cell_width = 30.0f;
        float default_cell_height = 24.0f;
        std::vector<std::pair<myseq::Cell, V2i>> clipboard;
        std::string instance_id = myseq::utils::gen_instance_id();

        bool show_metrics = false;

        std::optional<myseq::StatsReaderShm> stats_reader_shm;
        std::optional<myseq::Stats> stats;


        enum class Interaction {
            None,
            DrawingCells,
            DrawingLongCell,
            AdjustingVelocity,
            KeysSelect,
            DragSelectingCells,
            RectSelectingCells,
            MovingCells
        };
        Interaction interaction = Interaction::None;

        enum class InputMode {
            Drawing,
            Selecting
        };
        InputMode input_mode = InputMode::Drawing;

        void init_shm() {
            stats_reader_shm.emplace(instance_id.c_str());
        }


        /**
           UI class constructor.
           The UI should be initialized to a default state that matches the plugin side.
         */
        MySeqUI()
                : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {
            const double scaleFactor = getScaleFactor();

            undo_stack.emplace();

            myseq::test_serialize();
            init_shm();

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

        void read_stats() {
            if (stats_reader_shm.has_value()) {
                myseq::Stats *s = stats_reader_shm->read();
                if (nullptr != s) {
                    stats.emplace(*s);
                }
            }
        }

        myseq::State &state() {
            return undo_stack.top();
        }

        void publish() {
            auto s = state().to_json_string();
            publish_last_bytes = (int) s.length();
            setState("pattern", s.c_str());
// #ifdef DEBUG
            state() = myseq::State::from_json_string(s.c_str());
            publish_count += 1;
// #endif
        }

    protected:
        static const char *input_mode_to_string(InputMode i) {
            switch (i) {
                case InputMode::Drawing:
                    return "Drawing";
                case InputMode::Selecting:
                    return "Selecting";
            }
            return "Unknown";
        }

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
            p.each_selected_cell([&](const myseq::Cell &cell, const V2i &coords) {
                top_left.x = std::min(coords.x, top_left.x);
                top_left.y = std::min(coords.y, top_left.y);
                clipboard.emplace_back(cell, coords);
            });
            for (auto &i: clipboard) {
                i.second -= top_left;
            }
        }

        void
        grid_paste(bool &dirty, myseq::Pattern &p) const {
            p.put_cells(clipboard, p.cursor);
            SET_DIRTY();
        }

        void
        grid_keyboard_interaction(bool &dirty, myseq::Pattern &p) {
            auto ctrl_held = ImGui::GetIO().KeyCtrl;
            auto shift_held = ImGui::GetIO().KeyShift;
            // CTRL + key
            if (ctrl_held) {
                if (ImGui::IsKeyPressed(ImGuiKey_U)) {
                    auto amount = std::min(p.cursor.y, 12);
                    if (amount > 0) {
                        p.cursor.y -= amount;
                        SET_DIRTY();
                    }
                } else if (ImGui::IsKeyPressed(ImGuiKey_D)) {
                    auto amount = std::min(p.height - p.cursor.y - 1, 12);
                    if (amount > 0) {
                        p.cursor.y += amount;
                        SET_DIRTY();
                    }
                } else if (ImGui::IsKeyPressed(ImGuiKey_C)) {
                    grid_copy(p);
                } else if (ImGui::IsKeyPressed(ImGuiKey_V)) {
                    grid_paste(dirty, p);
                }
                return;
            }

            // Single key
            if (ImGui::IsKeyPressed(ImGuiKey_K)) {
                if (p.cursor.y > 0) {
                    p.cursor.y -= 1;
                    SET_DIRTY();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_J)) {
                if (p.cursor.y + 1 < p.height) {
                    p.cursor.y += 1;
                    SET_DIRTY();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_H)) {
                if (p.cursor.x > 0) {
                    p.cursor.x -= 1;
                    SET_DIRTY();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_L)) {
                if (p.cursor.x + 1 < p.width) {
                    p.cursor.x += 1;
                    SET_DIRTY();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_0)) {
                if (p.cursor.x != 0) {
                    p.cursor.x = 0;
                    SET_DIRTY();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_4)) {
                if (p.cursor.x != p.width - 1) {
                    p.cursor.x = p.width - 1;
                    SET_DIRTY();
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_1)) {
                input_mode = InputMode::Drawing;
            } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
                input_mode = InputMode::Selecting;
            } else if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
                auto v = p.get_velocity(p.cursor);
                uint8_t new_v = v == 0 ? 127 : 0;
                p.set_velocity(p.cursor, new_v);
                p.cursor.x = p.cursor.x + 1 < p.width ? p.cursor.x + 1 : 0;
                SET_DIRTY();
            } else {
                const int updown = shift_held ? 12 : 1;
                const int dy =
                        (ImGui::IsKeyPressed(ImGuiKey_W) ? -updown : 0)
                        + (ImGui::IsKeyPressed(ImGuiKey_S) ? updown : 0)
                        + (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ? -updown : 0)
                        + (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? updown : 0);
                const int dx =
                        (ImGui::IsKeyPressed(ImGuiKey_A) ? -1 : 0)
                        + (ImGui::IsKeyPressed(ImGuiKey_D) ? 1 : 0)
                        + (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ? -1 : 0)
                        + (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? 1 : 0);
                const V2i d(dx, dy);
                if (d != V2i(0, 0)) {
                    p.move_selected_cells(d);
                    SET_DIRTY();
                }
            }
        }

        void
        grid_interaction(bool &dirty, myseq::Pattern &p, const ImVec2 &grid_cpos, const ImVec2 &grid_size,
                         const ImVec2 &cell_size
        ) {
            auto ctrl_held = ImGui::GetIO().KeyCtrl;
            auto cmd_held = 0 != (ImGui::GetIO().KeyMods & ImGuiMod_Super);
            auto shift_held = ImGui::GetIO().KeyShift;
            auto mpos = ImGui::GetMousePos();
            auto cell = calc_cell(p, grid_cpos, mpos, grid_size, cell_size);
            auto cursor_hovers_grid = cell.x >= 0 && cell.y >= 0;
            switch (interaction) {
                case Interaction::DrawingCells: {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            auto have = p.get_velocity(cell);
                            uint8_t want = drag_started_velocity == 0 ? 127 : 0;
                            if (p.cursor != cell) {
                                p.cursor = cell;
                                SET_DIRTY();
                            }
                            if (have != want) {
                                p.set_velocity(cell, want);
                                SET_DIRTY();
                            }
                        }
                    } else {
                        interaction = Interaction::None;
                    }
                    break;
                }

                case Interaction::DrawingLongCell: {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                            if (cursor_hovers_grid) {
                                // p.set_selected(cell, !drag_started_selected);
                                d_debug("DRAW %d %d", cell.x, cell.y);
                                for (int x = cell.x; x < drag_started_cell.x; x++) {
                                    p.set_velocity(V2i(x, drag_started_cell.y), 127);
                                }
                                SET_DIRTY();
                            }
                        }
                    } else {
                        interaction = Interaction::None;
                    }
                    break;
                }

                case Interaction::AdjustingVelocity: {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            auto delta_y = mpos.y - drag_started_mpos.y;
                            auto new_vel = (uint8_t) clamp(
                                    (int) std::round((float) drag_started_velocity - (float) delta_y), 0, 127);
                            p.set_velocity(drag_started_cell, new_vel);
                            for (auto &pair: drag_started_velocity_vec) {
                                auto new_vel2 = (uint8_t) clamp(
                                        (int) std::round((float) pair.second - (float) delta_y), 0, 127);
                                if (!p.get_selected(pair.first)) {
                                    p.set_selected(pair.first, true);
                                }
                                p.set_velocity(pair.first, new_vel2);
                            }
                            SET_DIRTY();
                        }
                    } else {
                        interaction = Interaction::None;
                    }
                    break;
                }
                case Interaction::MovingCells:
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            const auto move_offset = cell - drag_started_cell;
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
                            SET_DIRTY();
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
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                        (shift_held || input_mode == InputMode::Selecting)) {
                        //
                    } else {
                        const auto sr = selection_rectangle(p, mpos, grid_cpos, grid_size, cell_size);
                        p.deselect_all();
                        for (int x = sr.cell_min.x; x <= sr.cell_max.x; x++)
                            for (int y = sr.cell_min.y; y <= sr.cell_max.y; y++) {
                                const auto v = V2i(x, y);
                                p.set_selected(v, true);
                            }

                        SET_DIRTY()
                        interaction = Interaction::None;
                    }
                    break;
                case Interaction::DragSelectingCells:
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && shift_held) {
                        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                            if (cursor_hovers_grid) {
                                p.set_selected(cell, !drag_started_selected);
                                SET_DIRTY();
                            }
                        }
                    } else {
                        interaction = Interaction::None;
                    }

                case Interaction::None:
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        if (cursor_hovers_grid) {
                            if (ImGui::IsKeyDown(ImGuiKey_T)) {
                                interaction = Interaction::DrawingLongCell;
                                p.set_velocity(cell, 127);
                                drag_started_mpos = mpos;
                                drag_started_cell = cell;
                            } else if (ctrl_held) {
                                interaction = Interaction::AdjustingVelocity;
                                drag_started_velocity = p.get_velocity(cell);
                                drag_started_velocity = drag_started_velocity == 0 ? 127 : drag_started_velocity;
                                drag_started_velocity_vec.clear();
                                p.each_selected_cell([&](const myseq::Cell &c, const V2i &v) {
                                    drag_started_velocity_vec.push_back({v, c.velocity});
                                });
                                drag_started_mpos = mpos;
                                drag_started_cell = cell;
                            } else if (p.get_selected(cell)) {
                                drag_started_mpos = mpos;
                                drag_started_cell = cell;
                                previous_move_offset = V2i(0, 0);
                                moving_cells_set.clear();
                                p.each_selected_cell([&](const myseq::Cell &c, const V2i &v) {
                                    moving_cells_set.insert(v);
                                });
                                interaction = Interaction::MovingCells;
                            } else if (input_mode == InputMode::Selecting || shift_held) {
                                p.cursor = cell;
                                if (p.is_active(cell)) {
                                    drag_started_selected = p.get_selected(cell);
                                    p.set_selected(cell, !drag_started_selected);
                                    interaction = Interaction::DragSelectingCells;
                                } else {
                                    drag_started_mpos = mpos;
                                    interaction = Interaction::RectSelectingCells;
                                }
                                SET_DIRTY();
                            } else {
                                interaction = Interaction::DrawingCells;
                                drag_started_velocity = p.get_velocity(cell);
                                p.set_velocity(cell, drag_started_velocity == 0 ? 127 : 0);
                                p.deselect_all();
                                SET_DIRTY();
                            }
                        } else {
                            // else { SET_DIRTY();}  // <-- why was this here?
                        }
                    } else {
                        if (cmd_held) {
                            if (ImGui::IsKeyPressed(ImGuiKey_A)) {
                                p.each_cell([&](const myseq::Cell &c, const V2i &v) {
                                    p.set_selected(v, true);
                                });
                                SET_DIRTY();
                            }
                        } else if (ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                            p.each_selected_cell([&](const myseq::Cell &c, const V2i &v) {
                                p.clear_cell(v);
                            });
                            SET_DIRTY();
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
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
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

        void show_grid(bool &dirty, myseq::Pattern &p, const std::optional<myseq::ActivePatternStats> &aps) {
            float cell_padding = 4.0;
            ImVec2 cell_padding_xy = ImVec2(cell_padding, cell_padding);
            float grid_width = ImGui::CalcItemWidth();
            const auto active_cell = ImColor(0x5a, 0x8a, 0xcf);
            const auto inactive_cell = ImColor(0x25, 0x25, 0x25);
            const auto hovered_color = ImColor(IM_COL32_WHITE);
            auto cpos = ImGui::GetCursorPos() - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());

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

            draw_list->PushClipRect(cpos, cpos + ImVec2(grid_width, grid_height), true);


            int active_column = -1;
            if (aps.has_value()) {
                active_column = (int) std::floor((double) p.width * (aps->time / aps->duration));
            }

            for (auto j = first_visible_col; j <= last_visible_col; j++) {
                for (auto i = first_visible_row; i <= last_visible_row; i++) {
                    auto loop_cell = V2i(j, i);
                    auto p_min = ImVec2(cell_size.x * (float) loop_cell.x,
                                        cell_size.y * (float) loop_cell.y) +
                                 offset + cpos + cell_padding_xy;
                    auto p_max = p_min + ImVec2(cell_size.x, cell_size.y) - cell_padding_xy;
                    auto vel = p.get_velocity(loop_cell);
                    auto sel = p.get_selected(loop_cell);
                    auto has_cursor = p.cursor == loop_cell;
                    auto velocity_fade = ((float) vel) / 127.0f;
                    ImColor cell_color1;
                    if (vel > 0) {
                        cell_color1 = ImColor(ImLerp(ImColor(IM_COL32_BLACK).Value, active_cell.Value, velocity_fade));
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
                    //draw_list->AddRect(p_min, p_max, border_color);

                    if ((alt_held || note % 12 == 0) && loop_cell.x == 0) {
                        draw_list->AddText(p_min, IM_COL32_WHITE, ALL_NOTES[note]);
                    } else if (vel > 0 && sel) {
                        draw_list->AddText(p_min, IM_COL32_WHITE, ONE_TO_256[vel - 1]);
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
            grid_interaction(dirty, p, cpos, grid_size, cell_size);
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

            read_stats();

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
            if (state().num_patterns() == 0) {
                state().selected = state().create_pattern().id;
                auto &cur = state().get_selected_pattern().cursor;
                cur.x = 0;
                cur.y = 127 - 24;
                SET_DIRTY();
            }
            auto &p = state().get_selected_pattern();

            if (ImGui::Begin("MySeq", nullptr, window_flags)) {
                ImGui::SetWindowFontScale(1.0);

                show_grid(dirty, p, get_pattern_stats(p.id));

                ImGui::SameLine();
                ImGui::BeginGroup();
                if (ImGui::Button("Add")) {
                    SET_DIRTY();
                    create = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete")) {
                    SET_DIRTY();
                    delete_ = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Duplicate")) {
                    SET_DIRTY();
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
                    state().each_pattern([&](const myseq::Pattern &pp) {
                        ImGui::TableNextRow();

                        // id
                        ImGui::TableNextColumn();
                        int flags = ImGuiSelectableFlags_None;
                        const auto id = pp.id;
                        if (ImGui::Selectable(std::to_string(id).c_str(), state().selected == id, flags)) {
                            state().selected = id;
                            SET_DIRTY();
                        }

                        // length
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", std::to_string(pp.width).c_str());

                        // first_note
                        ImGui::TableNextColumn();
                        ImGui::PushID(id);
                        myseq::Pattern &tmp = state().get_pattern(id);
                        ImGui::PushID(1);
                        const auto first_note = note_select(tmp.first_note);
                        if (tmp.first_note != first_note) {
                            tmp.first_note = first_note;
                            SET_DIRTY();
                        }
                        ImGui::PopID();

                        // last_note
                        ImGui::TableNextColumn();
                        ImGui::PushID(2);
                        const auto last_note = note_select(tmp.last_note);
                        if (tmp.last_note != last_note) {
                            tmp.last_note = last_note;
                            SET_DIRTY();
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


                if (ImGui::Checkbox("Play selected pattern", &state().play_selected)) {
                    SET_DIRTY();
                }

                ImGui::EndGroup();

                int pattern_width_slider_value = p.width;
                if (ImGui::SliderInt("##pattern_width", &pattern_width_slider_value, 1, 32, nullptr,
                                     ImGuiSliderFlags_None)) {
                    p.resize_width(pattern_width_slider_value);
                    SET_DIRTY();
                }

                show_keys(p);

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
                ImGui::Text("FPS=%f", ImGui::GetCurrentContext()->IO.Framerate);
                ImGui::Text("tick=%f", tc.global_tick());
                ImGui::Text("interaction=%s", interaction_to_string(interaction));
                int selected_cells_count = 0;
                p.each_selected_cell([&selected_cells_count](const myseq::Cell &c, const V2i &v) {
                    selected_cells_count += 1;
                });
                ImGui::Text("instance_id=%s", instance_id.c_str());
                ImGui::Text("publish_count=%d", publish_count);
                ImGui::Text("publish_last_bytes=%d", publish_last_bytes);
                ImGui::Text("input_mode=%s", input_mode_to_string(input_mode));
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


            if (show_metrics) {
                ImGui::ShowMetricsWindow(&show_metrics);
            }

            ImGui::End();
            if (create) {
                state().selected = state().create_pattern().id;
            }
            if (delete_) {
                state().delete_pattern(state().selected);
            }
            if (duplicate) {
                state().selected = state().duplicate_pattern(state().selected).id;
            }
            if (dirty) {
                publish();
            }
        }

        void idleCallback() override {
            repaint();
        }

        void stateChanged(const char *key, const char *value) override {
            d_debug("PluginUI: stateChanged key=%s", key);
            if (std::strcmp(key, "pattern") == 0) {
                state() = myseq::State::from_json_string(value);
            } else if (std::strcmp(key, "instance_id") == 0) {
                d_debug("PluginUI: stateChanged instance_id=%s new=%s", instance_id.c_str(), value);
                if (std::strcmp(instance_id.c_str(), value) != 0) {
                    d_debug("PluginUI: reinitialzing shm");
                    instance_id = std::string(value);
                    init_shm();
                } else {
                    d_debug("PluginUI: will do nothing for same value");
                }
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
