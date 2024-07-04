//
// Created by Arunas on 14/06/2024.
//

#include <cista.h>
#include "GenArray.hpp"


void gen_array_test_const_iterator() {
    struct Test {
        const GenArray<int> arr;

        void check(const int count) const {
            int j = 0;
            for (auto const &x: arr) {
                assert(j++ == x);
            }
            assert(count == j);
        }
    };
    GenArray<int> arr;
    std::vector<Id> ids;
    const int count = 1000;
    for (int i = 0; i < count; i++) {
        ids.push_back(arr.push(i));
    }
    Test test = {arr};
    test.check(count);
}

void gen_array_test_mutate() {
    GenArray<std::optional<int>> arr;
    std::vector<Id> ids;
    const int count = 1000;
    for (int i = 0; i < count; i++) {
        ids.push_back(arr.push({i}));
    }
    for (auto &x: arr) {
        *x *= 2;

    }
    for (int i = 0; i < count; i++) {
        assert(arr.get(ids[i]) == i * 2);
    }
}

void gen_array_test_rand() {
    std::srand(0);
    GenArray<int> arr;
    std::vector<Id> ids;
    const int count = 1000;
    for (int i = 0; i < count; i++) {
        ids.push_back(arr.push(i));
    }
    bool removed[count] = {};
    std::vector<Id> removed_ids;
    std::vector<Id> remaining_ids;
    for (auto id: ids) {
        if (std::rand() % 2) {
            const auto value = arr.get(id);
            arr.remove(id);
            removed[value] = true;
            removed_ids.push_back(id);
        } else {
            remaining_ids.push_back(id);
        }
    }
    int actual_remaining = 0;
    for (const auto x: arr) {
        assert(!removed[x]);
        actual_remaining++;
    }
    assert(actual_remaining == remaining_ids.size());
    assert(arr.size() == remaining_ids.size());
    for (auto id: removed_ids) {
        assert(!arr.exists(id));
    }
    for (auto id: remaining_ids) {
        assert(arr.exists(id));
    }
    for (int i = 0; i < count; i++) {
        if (removed[i]) {
            arr.push(i);
            removed[i] = false;
        }
    }
    assert(arr.size() == count);
}

void gen_array_test_more_elements() {
    std::chrono::high_resolution_clock clock;
    auto start = clock.now();
    const int count = 1000 * 1000;
    std::srand(0);
    std::vector<Id> ids;
    GenArray<int> arr;
    for (int i = 0; i < count; i++) {
        ids.push_back(arr.push(i));
    }
    for (int i = count - 1; i >= 1; i--) {
        int j = std::rand() % (i + 1);
        const auto a = ids[i];
        ids[i] = ids[j];
        ids[j] = a;
    }
    std::vector<int> removed;
    for (int i = 0; i < count / 2; i++) {
        removed.push_back(arr.get(ids[i]));
        arr.remove(ids[i]);
        ids.pop_back();
    }
    assert(arr.size() == count - count / 2);
    for (int i = 0; i < count / 2; i++) {
        arr.push(removed[i]);
    }
    assert(arr.size() == count);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - start).count();
    //printf("Elapsed: %ld\n", elapsed);
}

void gen_array_test_basic() {
    GenArray<int> test;
    auto a = test.push(1);
    auto b = test.push(2);
    auto c = test.push(3);
    auto d = test.push(4);

    assert(test.exists(b));
    assert(1 == test.get(a));
    assert(2 == test.get(b));
    assert(3 == test.get(c));
    assert(4 == test.get(d));
    test.remove(b);
    assert(!test.exists(b));
    auto it = test.begin();
    assert(1 == *it);
    it++;
    assert(3 == *it);
    it++;
    assert(4 == *it);
    it++;
    assert(test.end() == it);

    test.push(100);

    for (auto iter = test.begin(); iter != test.end();) {
        if (*iter == 3) {
            iter = test.erase(iter);
        } else {
            iter++;
        }
    }

    std::set<int> tmp;
    for (auto y: test) {
        tmp.insert(y);
    }
    assert(tmp.find(1) != tmp.end());
    assert(tmp.find(2) == tmp.end());
    assert(tmp.find(3) == tmp.end());
    assert(tmp.find(4) != tmp.end());
    assert(tmp.find(100) != tmp.end());
    assert(tmp.size() == 3);

}


void gen_array_tests() {
    gen_array_test_more_elements();
    gen_array_test_basic();
    gen_array_test_rand();
    gen_array_test_mutate();
    gen_array_test_const_iterator();
}

