//
// Created by Arunas on 14/06/2024.
//

#include "GenArray.hpp"

void gen_array_test() {
    GenArray<int> test;
    auto a = test.push(1);
    auto b = test.push(2);
    auto c = test.push(3);
    auto d = test.push(4);

    assert(test.exist(b));
    assert(1 == test.get(a));
    assert(2 == test.get(b));
    assert(3 == test.get(c));
    assert(4 == test.get(d));
    test.remove(b);
    assert(!test.exist(b));
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
