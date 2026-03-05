#ifndef VECTOR_HPP
#define VECTOR_HPP

#include <stdexcept>
#include <algorithm> 

template<typename T>
class Vector;

template<typename S>
class Stack {
private:
    Vector<S> container;

public:
    void push(const S& item) {
        container.push_back(item);
    }

    void push(S&& item) {
        container.push_back(std::move(item));
    }

    void pop() {
        if (!empty()) {
            container.pop_back();
        }
    }

    const S& top() const {
        if (empty()) {
            throw std::out_of_range("Stack::top() on empty stack");
        }
        return container[container.size() - 1];
    }

    S& top() {
        if (empty()) {
            throw std::out_of_range("Stack::top() on empty stack");
        }
        return container[container.size() - 1];
    }

    bool empty() const {
        return container.empty();
    }

    size_t size() const {
        return container.size();
    }
};

template<typename T>
class Vector {
private:
    T* data_;
    size_t size_;
    size_t capacity_;

    void internal_resize(size_t new_capacity) {
        if (new_capacity == 0) {
            delete[] data_;
            data_ = nullptr;
            capacity_ = 0;
            size_ = 0;
            return;
        }
        T* new_data = new T[new_capacity];

        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = data_[i];
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;

        if (size_ > capacity_) {
            size_ = capacity_;
        }
    }

    void quicksort_iterative() {
        if (size_ <= 1) return;

        Stack<std::pair<size_t, size_t>> stack;
        stack.push({0, size_ - 1});

        while (!stack.empty()) {
            auto [low, high] = stack.top();
            stack.pop();

            if (low >= high) continue;

            T pivot = data_[low + (high - low) / 2];
            size_t i = low, j = high;

            while (i <= j) {
                while (data_[i] < pivot) ++i;
                while (data_[j] > pivot) --j;
                if (i <= j) {
                    std::swap(data_[i++], data_[j--]);
                }
            }

            if (low < j) stack.push({low, j});
            if (i < high) stack.push({i, high});
        }
    }

public:
    explicit Vector(size_t init_capacity = 10) : data_(nullptr), size_(0), capacity_(0) {
        if (init_capacity > 0) {
            internal_resize(init_capacity);
        }
    }

    ~Vector() {
        delete[] data_;
    }

    Vector(const Vector& other) : data_(nullptr), size_(0), capacity_(0) {
        if (this != &other) {
            internal_resize(other.capacity_);
            size_ = other.size_;
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = other.data_[i];
            }
        }
    }

    Vector(Vector&& other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    Vector(const std::vector<T>& other) : size_(other.size()), capacity_(other.size()) {
        data_ = new T[capacity_];
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = other[i];
        }
    }

    Vector& operator=(const Vector& other) {
        if (this != &other) {
            internal_resize(other.capacity_);
            size_ = other.size_;
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = other.data_[i];
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;

            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    void resize(size_t new_size) {
        if (new_size > capacity_) {
            internal_resize(new_size);
        }
        if (new_size > size_) {
            for (size_t i = size_; i < new_size; ++i) {
                data_[i] = T();
            }
        }
        size_ = new_size;
    }

    void push_back(const T& val) {
        if (size_ >= capacity_) {
            size_t new_capacity = (capacity_ == 0) ? 10 : capacity_ * 2;
            internal_resize(new_capacity);
        }
        data_[size_] = val;
        size_++;
    }

    void push_back(T&& val) {
        if (size_ >= capacity_) {
            size_t new_capacity = (capacity_ == 0) ? 10 : capacity_ * 2;
            internal_resize(new_capacity);
        }
        data_[size_] = std::move(val);
        size_++;
    }

    void pop_back() {
        if (size_ > 0) {
            size_--;
        }
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    size_t capacity() const {
        return capacity_;
    }

    T& at(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Vector::at(): index out of range");
        }
        return data_[index];
    }

    const T& at(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Vector::at(): index out of range");
        }
        return data_[index];
    }

    T& operator[](size_t index) {
        return data_[index];
    }

    const T& operator[](size_t index) const{
        return data_[index];
    }

    T* begin() {
        return data_;
    }

    T* end() {
        return data_ + size_;
    }

    const T* begin() const{
        return data_;
    }

    const T* end() const {
        return data_ + size_;
    }

    void clear() {
        size_ = 0;
    }

    void erase(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Vector::erase(): index out of range");
        }
        for (size_t i = index; i < size_ - 1; ++i) {
            data_[i] = std::move(data_[i + 1]);
        }
        size_--;
    }

    void sort() {
        quicksort_iterative();
    }

    void remove_dupl() {
        if (size_ <= 1) return;

        size_t write_idx = 1;
        for (size_t read_idx = 1; read_idx < size_; ++read_idx) {
            if (data_[read_idx] != data_[read_idx - 1]) {
                data_[write_idx] = std::move(data_[read_idx]);
                write_idx++;
            }
        }
        size_ = write_idx;
    }
};

#endif