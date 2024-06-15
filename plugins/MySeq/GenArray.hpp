// Created by Arunas on 13/06/2024.
//

#ifndef MY_PLUGINS_GENARRAY_HPP
#define MY_PLUGINS_GENARRAY_HPP

#include <vector>
#include <optional>
#include <set>
#include <cassert>
#include <algorithm>

struct Id {
    int index;
    int gen;
};


template<typename T>
struct GenArray {
    std::vector<std::optional<T>> data;
    std::vector<int> data_gen;
    std::vector<int> free;

    GenArray() = default;

    GenArray(const GenArray<T> &other) = delete;

    void operator=(const GenArray<T> &other) = delete;

    class iterator {
        GenArray<T> *ga;
    protected:
        int index;

        void seek_valid() {
            while (index < ga->data.size() && !ga->data[index].has_value()) {
                index++;
            }
        }

    public:
        explicit iterator(GenArray<T> *ga, int _index = 0) : ga(ga), index(_index) {
            seek_valid();
        }

        iterator &operator++() {
            index++;
            seek_valid();
            return *this;
        }

        iterator operator++(int) {
            iterator result = *this;
            ++(*this);
            return result;
        }

        bool operator==(const iterator &other) const {
            return ga == other.ga && index == other.index;
        }

        bool operator!=(const iterator &other) const {
            return !(*this == other);
        }

        T &operator*() {
            return ga->data[index].value();
        }

        using difference_type = int;
        using value_type = T;
        using pointer = T *;
        using reference = T &;
        using iterator_category = std::forward_iterator_tag;

        friend class GenArray<T>;
    };

    void verify_id(const Id &id) {
        if (!exist(id)) {
            throw std::runtime_error("Invalid id");
        }
    }

    void verify_index(const int index) {
        if (index >= data.size() || index < 0 || !data[index].has_value()) {
            throw std::runtime_error("Invalid id");
        }
    }

    iterator begin() {
        return iterator(this, 0);
    }

    iterator end() {
        return iterator(this, data.size());
    }

    [[nodiscard]] bool exist(const Id &id) const {
        return id.index >= 0 && id.index < data.size()
               && data[id.index].has_value()
               && data_gen[id.index] == id.gen;
    }

    void remove(const Id &id) {
        verify_id(id);
        remove_at_index(id.index);
    }

    void remove_at_index(int index) {
        verify_index(index);
        data[index].reset();
        free.push_back(index);
    }

    iterator erase(const iterator &it) {
        remove_at_index(it.index);
        iterator it2 = it;
        it2.seek_valid();
        return it2;
    }

    Id push(T x) {
        if (!free.empty()) {
            int index = free.back();
            int gen = data_gen[index] + 1;
            free.pop_back();
            data[index] = x;
            data_gen[index] = gen;
            return {index, gen};
        } else {
            data.push_back({x});
            data_gen.push_back(0);
            return {static_cast<int>(data.size()) - 1, 0};
        }
    }

    T &get(const Id &id) {
        verify_id(id);
        return data[id.index].value();
    }

    T &operator[](const Id &id) {
        return get(id);
    }

    const T &operator[](const Id &id) const {
        return get(id);
    }

};

void gen_array_test();

#endif //MY_PLUGINS_GENARRAY_HPP
