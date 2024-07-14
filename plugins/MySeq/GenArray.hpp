// Created by Arunas on 13/06/2024.
//

#ifndef MY_PLUGINS_GENARRAY_HPP
#define MY_PLUGINS_GENARRAY_HPP

#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include "MyAssert.hpp"

struct Id {
    int index;
    int gen;

    Id() : index(-1), gen(0) {}

    Id(int index, int gen) : index(index), gen(gen) {}

    bool operator==(const Id &other) const {
        return index == other.index && gen == other.gen;
    }

    [[nodiscard]] bool is_null() const {
        return index == -1;
    }

    static Id null() {
        return Id();
    }
};

template<typename T>
struct GenArray {
    std::vector<std::optional<T>> data;
    std::vector<int> data_gen;
    std::vector<int> free;

    //GenArray() = default;

    // deleted for debugging, there is no reason for copying to not work
    //GenArray(const GenArray<T> &other) = delete;

    //void operator=(const GenArray<T> &other) = delete;

    class const_iterator {
        const GenArray<T> *ga;
    protected:
        int index;

        void seek_valid() {
            while (index < ga->data.size() && !ga->data[index].has_value()) {
                index++;
            }
        }

    public:
        explicit const_iterator(const GenArray<T> *ga, int _index = 0) : ga(ga), index(_index) {
            seek_valid();
        }

        const_iterator &operator++() {
            index++;
            seek_valid();
            return *this;
        }

        const_iterator operator++(int) {
            iterator result = *this;
            ++(*this);
            return result;
        }

        bool operator==(const const_iterator &other) const {
            return ga == other.ga && index == other.index;
        }

        bool operator!=(const const_iterator &other) const {
            return !(*this == other);
        }

        const T &operator*() const {
            return ga->data[index].value();
        }

        using difference_type = int;
        using value_type = T;
        using pointer = T *;
        using reference = T &;
        using iterator_category = std::forward_iterator_tag;
    };

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

        friend class GenArray<T>;

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
            return *(ga->data[index]);
        }

        using difference_type = int;
        using value_type = T;
        using pointer = T *;
        using reference = T &;
        using iterator_category = std::forward_iterator_tag;

        friend class GenArray<T>;
    };

    void verify_id(const Id &id) const {
        assert(exists(id));
    }

    void verify_index(const int index) const {
        if (index >= data.size() || index < 0 || !data[index].has_value()) {
            throw std::runtime_error("verify_index: does not exist");
        }
    }

    const_iterator begin() const {
        return const_iterator(this, 0);
    }

    const_iterator end() const {
        return const_iterator(this, (int) data.size());
    }

    iterator begin() {
        return iterator(this, 0);
    }

    iterator end() {
        return iterator(this, data.size());
    }

    [[nodiscard]] std::size_t size() const {
        const auto ret = data.size() - free.size();
#ifdef DEBUG
        const auto actual = std::count_if(data.begin(), data.end(), [](const std::optional<T> &x) {
            return x.has_value();
        });
        assert(ret == actual);
#endif // DEBUG
        return ret;
    }

    [[nodiscard]] bool exists(const Id &id) const {
        return id.index >= 0 && id.index < (int) data.size()
               && data[id.index].has_value()
               && data_gen[id.index] == id.gen;
    }

    void remove(const Id &id) {
        verify_id(id);
        remove_at_index(id.index);
    }

    void remove_if_exits(const Id &id) {
        if (exists(id)) {
            remove(id);
        }
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

    iterator erase(const iterator &first, const iterator &last) {
        if (first == last) {
            return first;
        }
        auto it = first;
        while (it != last) it = erase(it);
        return it;
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

    const T &get(const Id &id) const {
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

void gen_array_tests();

#endif //MY_PLUGINS_GENARRAY_HPP
