
#ifndef VOXLIFE_WRAPPING_ITERATOR_H
#define VOXLIFE_WRAPPING_ITERATOR_H

#include <iterator>


template<typename Iterator>
class forward_wrapping_view {
public:
    forward_wrapping_view(Iterator start, Iterator stop, Iterator underlying_begin, Iterator underlying_end)
            : start(start), stop(stop),
              underlying_begin(underlying_begin), underlying_end(underlying_end) {}

    class iterator {
    public:
        using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;
        using value_type        = typename std::iterator_traits<Iterator>::value_type;
        using difference_type   = typename std::iterator_traits<Iterator>::difference_type;
        using pointer           = typename std::iterator_traits<Iterator>::pointer;
        using reference         = typename std::iterator_traits<Iterator>::reference;

        iterator(Iterator current, Iterator underlying_begin, Iterator underlying_end)
                : current(current),
                  underlying_begin(underlying_begin), underlying_end(underlying_end) {}

        reference operator*() const {
            return *current;
        }

        iterator& operator++() {
            ++current;

            if (current == underlying_end) {
                current = underlying_begin;
            }

            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return current == other.current;
        }

        bool operator!=(const iterator& other) const {
            return *this != other;
        }

    private:
        Iterator current;
        Iterator underlying_begin;
        Iterator underlying_end;
    };

    iterator begin() const {
        return iterator(start, underlying_begin, underlying_end);
    }

    iterator end() const {
        return iterator(stop, underlying_begin, underlying_end);
    }

private:
    Iterator start;
    Iterator stop;
    Iterator underlying_begin;
    Iterator underlying_end;
};


template<typename Iterator>
class reverse_wrapping_view {
public:
    reverse_wrapping_view(Iterator start, Iterator stop, Iterator underlying_begin, Iterator underlying_end)
            : start(start), stop(stop),
              underlying_begin(underlying_begin), underlying_end(underlying_end) {}

    class iterator {
    public:
        using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;
        using value_type        = typename std::iterator_traits<Iterator>::value_type;
        using difference_type   = typename std::iterator_traits<Iterator>::difference_type;
        using pointer           = typename std::iterator_traits<Iterator>::pointer;
        using reference         = typename std::iterator_traits<Iterator>::reference;

        iterator(Iterator current, Iterator underlying_begin, Iterator underlying_end)
                : current(current),
                  underlying_begin(underlying_begin), underlying_end(underlying_end) {}

        reference operator*() const {
            return *current;
        }

        iterator& operator++() {
            if (current == underlying_begin)
                current = underlying_end; // Wrap to one past the last element

            --current;

            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return current == other.current;
        }

        bool operator!=(const iterator& other) const {
            return *this != other;
        }

    private:
        Iterator current;
        Iterator underlying_begin;
        Iterator underlying_end;
    };

    iterator begin() const {
        return iterator(start, underlying_begin, underlying_end);
    }

    iterator end() const {
        return iterator(stop, underlying_begin, underlying_end);
    }

private:
    Iterator start;
    Iterator stop;
    Iterator underlying_begin;
    Iterator underlying_end;
};


template<typename Iterator>
class bidirectional_wrapping_view {
public:
    bidirectional_wrapping_view(Iterator start, Iterator stop, Iterator underlying_begin, Iterator underlying_end, bool reverse = false)
            : start(start), stop(stop),
              underlying_begin(underlying_begin), underlying_end(underlying_end),
              reverse(reverse) {}

    class iterator {
    public:
        using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;
        using value_type        = typename std::iterator_traits<Iterator>::value_type;
        using difference_type   = typename std::iterator_traits<Iterator>::difference_type;
        using pointer           = typename std::iterator_traits<Iterator>::pointer;
        using reference         = typename std::iterator_traits<Iterator>::reference;

        iterator(Iterator current, Iterator underlying_begin, Iterator underlying_end, bool reverse)
                : current(current),
                  underlying_begin(underlying_begin), underlying_end(underlying_end),
                  reverse(reverse) {}

        reference operator*() const {
            return *current;
        }

        iterator& operator++() {
            if (reverse) {
                // Reverse iteration
                if (current == underlying_begin)
                    current = underlying_end; // Wrap to one past the last element

                --current;
            } else {
                // Forward iteration
                ++current;

                if (current == underlying_end)
                    current = underlying_begin; // Wrap to the first element
            }

            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return current == other.current;
        }

        bool operator!=(const iterator& other) const {
            return *this != other;
        }

    private:
        Iterator current;
        Iterator underlying_begin;
        Iterator underlying_end;
        bool reverse;
    };

    iterator begin() const {
        return iterator(start, underlying_begin, underlying_end, reverse);
    }

    iterator end() const {
        return iterator(stop, underlying_begin, underlying_end, reverse);
    }

private:
    Iterator start;
    Iterator stop;
    Iterator underlying_begin;
    Iterator underlying_end;
    bool reverse;
};


#endif //VOXLIFE_WRAPPING_ITERATOR_H
