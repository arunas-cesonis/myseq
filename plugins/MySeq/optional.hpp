//
// Created by Arunas on 22/05/2024.
//

#ifndef MY_PLUGINS_OPTIONAL_HPP
#define MY_PLUGINS_OPTIONAL_HPP
namespace std1 {
    template<typename T>
    struct optional {
        T *x;

        explicit optional(T x) {
            *(this->x) = x;
        }

        virtual ~optional() {
            if (this->x) {
                delete this->x;
            }
            this->x = nullptr;
        }

        optional() {
            this->x = nullptr;
        }

        bool has_value() const { return this->x; }

        T value() const { return *(this->x); }


    };
}

#endif //MY_PLUGINS_OPTIONAL_HPP
