// http://coliru.stacked-crooked.com/a/3ed0d30ffdd29c0a

// Your name: C. Dave Tallman
// Your email: davetallman@msn.com

/*

A program to read a csv file and replace a specified field.

The most expressive part of the code is this loop, where actions are applied
to individual fields. The flexibility gained by using a function per field allows
multiple column replacements or other future modifications.

for (auto&& [field, action]: combine::limited_combine(data_fields, m_actions)) {
   ...
}

Since no boost is allowed, I wrote a custom range combiner. The boost::combine is flawed in a couple of serious ways:
1) It fails when the range lengths don't match.
2) It returns boost::tuple results, which have compatibility problems with std::tuple.

Uses of C++17 features:
1) std::string_view, for parsing the csv lines without creating extra strings.
2) std::variant, to make an efficient function table. Unlike a vector<std::function>, we only pay for erasure when we have to.
3) std::invoke, to use either function pointers or std::function in visit.
4) std::optional, to return an optional replacement string.
5) Nested namespaces, to hold template details for the combine and csv namespaces.
6) Initialization for if, to simplify some tests, such as for the optional replacement string.
7) Extended range so that LimitedCombine can use sentinals, to avoid the problem boost::combine has with mismatched range lengths.
8) std::apply to increment a tuple of iterators for LimitedCombine, and other tuple operations.
9) Structured binding to get data/action tuples in a ranged loop.
10) Template argument deduction for class templates, for building LimitedCombine.
11) Inline variables for template "_v" variables.
12) Inline static constexpr variable in one template, so the compiler will more easily accept a calculated tuple position.


g++ -std=c++1z -O2 -Wall $1 -lstdc++fs
./a.out $2 $3 $4 $5
echo comparing "correct_output.csv" with $5
cmp --silent correct_output.csv $5 || echo "files are different!"

where
  $1 - cpp file name
  $2 - name of input csv
  $3 - column name
  $4 - replacement string
  $5 - output file name

*/

#include <string>
#include <iostream>
#include <fstream>
#include <functional>
#include <string_view>
#include <optional>
#include <variant>
#include <vector>
#include <list>
#include <tuple>
#include <iterator>
#include <type_traits>

namespace combine::detail {

// Since I am not using boost::iterator, I need a bit of ugly metaprogramming
// to find the lowest-level iterator type from a tuple of iterators.

// Template to convert tuple types to numbers for comparison.
template <typename T>
struct itertag2value;

template<>
struct itertag2value<std::output_iterator_tag> {
    enum { value = 0 };
};

template<>
struct itertag2value<std::input_iterator_tag> {
    enum { value = 1 };
};

template<>
struct itertag2value<std::forward_iterator_tag> {
    enum { value = 2 };
};

template<>
struct itertag2value<std::bidirectional_iterator_tag> {
    enum { value = 3 };
};

template<>
struct itertag2value<std::random_access_iterator_tag> {
    enum { value = 4 };
};

using counter_t = size_t;

template< class T >
inline constexpr counter_t itertag2value_v = static_cast<counter_t>(itertag2value<T>::value);

// Helper during debugging. Uses a fold expression.
template <typename... Args>
void argwriter(std::ostream &os, Args&& ... args) {
    (os << ... << args);
}

// Helper during debugging to write out numeric tuples.
template <typename ... TTypes>
void writeNumericTuple(std::ostream &os, const std::tuple<TTypes ...>& tt) {
    std::apply([&os] (auto ... x) { argwriter(os, (std::to_string(x)+" ") ...); }, tt);
}

// True if any of the tuple elements match. Recursive helper.
template <counter_t i, counter_t j, typename T>
struct anyEqualHelper;

template <counter_t i, counter_t j, typename T>
struct anyEqualHelper {
    static bool tupleAnyEqual(const T& lhs, const T& rhs)
    {
        // Use "||": true if any are equal.
        return (std::get<i>(lhs) == std::get<i>(rhs)) ||
            anyEqualHelper<i+1, j, T>::tupleAnyEqual(lhs, rhs);
    }
};

// Partial specialiation to end the recursive for loop.
template <counter_t i, typename T>
struct anyEqualHelper<i, i, T> {
    static bool tupleAnyEqual(const T&, const T&)
    {
        return false;
    }
};

// True if any of the tuple elements match. This one is for std::tuple.
template <typename ... TTypes>
bool AnyEqualCompare(const std::tuple<TTypes ...>& lhs,
                          const std::tuple<TTypes ...>& rhs)
{
    using Tp = std::tuple<TTypes ...>;
    return anyEqualHelper<0, std::tuple_size<Tp>::value, Tp>::tupleAnyEqual(lhs, rhs);
}

// Helper to find the position of the minimum iterator level in a tuple of iterators.
template <counter_t i, counter_t j, counter_t lowest, typename Tp>
struct minIterHelper;

template<counter_t i, counter_t j, counter_t lowest, typename Tp>
inline constexpr counter_t minIterHelper_v = static_cast<counter_t>(minIterHelper<i, j, lowest, Tp>::value);

template <counter_t i, counter_t j, counter_t lowest, typename Tp>
struct minIterHelper {
    using lowest_type = typename std::iterator_traits<std::tuple_element_t<lowest, Tp>>::iterator_category;
    using i_type = typename std::iterator_traits<std::tuple_element_t<i, Tp>>::iterator_category;
    enum { value = (itertag2value_v<i_type> < itertag2value_v<lowest_type>) ?
        minIterHelper_v<i+1, j, i, Tp> : minIterHelper_v<i+1, j, lowest, Tp> };
};

// Partial specialiation to end the recursive for loop.
template <counter_t i, counter_t lowest, typename Tp>
struct minIterHelper<i, i, lowest, Tp> {
    enum { value = lowest };
};

// Find the position in a tuple of interators of the iterator with the lowest level.
template <typename Tp>
struct MiniumLevelIteratorCategory
{
    static constexpr counter_t ipos = minIterHelper_v<0, std::tuple_size_v<Tp>, 0, Tp>;
    using iterator_type = std::tuple_element_t<ipos, Tp>;
    using type = typename std::iterator_traits<iterator_type>::iterator_category;
};

} // end of namespace combine::detail

namespace combine {

template <typename Tp>
class LimitedCombineSentinal;

// Unlike the boost::zip_iterator, which has undefined behaviour when used with
// ranges of unequal length, this zip iterator allows a sentinal flag.
// Sentinals compare equality differently, comparing true when any tuple field matches.
// A tuple of sentinaols can safely be used as the end of a range in a ranged for loop, since it
// will stop the loop when the first end iterator is reached.
// This trick can be accomplished nicely in C++17 with a different sentinal type.
template <typename Tp>
class LimitedCombineIterator {
private:
    Tp zip_iter;

public:
    // Use the minimum category of the tuple of iterators.
    using iterator_category = typename detail::MiniumLevelIteratorCategory<Tp>::type;

    // Use the difference type of the first iterator in the tuple (as boost::zip_iterator does).
    using difference_type = typename std::iterator_traits<std::tuple_element_t<0, Tp>>::iterator_category;

    LimitedCombineIterator(Tp iter_)
        : zip_iter(iter_)
    {}

    // Make a tuple of what each item points to.
    decltype(auto) operator *() const
    {
        return std::apply([] (auto&& ... x) { return std::make_tuple(std::ref(*x) ...); }, zip_iter);
    }

    using reference = decltype(std::declval<LimitedCombineIterator &>().operator *());
    using value_type = reference;

    const LimitedCombineIterator &operator ++() {
      std::apply([] (auto&& ... x) { (++x, ...); }, zip_iter);
      return *this;
    }

    Tp get_iter() const { return zip_iter; }

    bool operator ==(const LimitedCombineIterator &other) const {
        return zip_iter == other.zip_iter;
    }

    bool operator !=(const LimitedCombineIterator &other) const {
        return zip_iter != other.zip_iter;
    }

    // Sentinal comparison overloads
    bool operator ==(const LimitedCombineSentinal<Tp> &other) const {
        return detail::AnyEqualCompare(zip_iter, other.get_iter());
    }

    bool operator !=(const LimitedCombineSentinal<Tp> &other) const {
        return !detail::AnyEqualCompare(zip_iter, other.get_iter());
    }
};

template <typename Tp>
class LimitedCombineSentinal {
private:
    Tp zip_iter;

public:
    LimitedCombineSentinal(Tp iter_)
        : zip_iter(iter_)
    {}

    Tp get_iter() const { return zip_iter; }

    bool operator ==(const LimitedCombineSentinal &other) const {
        return detail::AnyEqualCompare(zip_iter, other.zip_iter);
    }

    bool operator !=(const LimitedCombineSentinal &other) const {
        return !detail::AnyEqualCompare(zip_iter, other.zip_iter);
    }

    bool operator ==(const LimitedCombineIterator<Tp> &other) const {
        return detail::AnyEqualCompare(zip_iter, other.get_iter());
    }

    bool operator !=(const LimitedCombineIterator<Tp> &other) const {
        return !detail::AnyEqualCompare(zip_iter, other.get_iter());
    }

    // The sentinal does not need ++ or dereference operators.
    // It is not an iterator.
};

template <typename T>
class LimitedCombine {
private:
 public:
   using iterator = LimitedCombineIterator<T>;
   using sentinal = LimitedCombineSentinal<T>;

   iterator begin() const { return begin_; }
   sentinal end() const { return end_; }

   LimitedCombine(T beginning, T ending)
   : begin_(beginning)
   , end_(ending)
   {}
private:
   iterator begin_;
   sentinal end_;
};

// Helper function to create a LimitedCombine
// Unlike boost::combine, it will not run past the shortest of its ranges.
template <typename ... TTypes>
inline auto limited_combine(TTypes&& ... a)
{
     return LimitedCombine(std::make_tuple(std::begin(a) ...), std::make_tuple(std::end(a) ...));
}

} // end of namespace combine

namespace csv {
using StringList = std::vector<std::string_view>;
}

namespace csv::detail {

//  Creates a list of iterators that split the input wherever the given predicate is true.
template<typename InputIterator, typename UnaryPredicate>
auto split_if_all(InputIterator first, InputIterator last, UnaryPredicate pred) -> std::vector<InputIterator>
{
    std::vector<InputIterator> result;
    if (first == last) return result;

    result.push_back(first);
    while (first != last) {
        if (pred(*first)) {
            result.push_back(first);
        }
        ++first;
    }
    result.push_back(last);
    return result;
}

// Evaluates a function on adjacent pairs between two iterators.
template <typename II, typename Fun>
void for_each_adjacent(II first, II last, Fun fun)
{
    if (first == last) {
        return;
    }
    II prev = first;
    for (;;) {
        if (++first == last) break;
        fun(*prev, *first);
        prev = first;
    }
}

// Splits a string into sub-fields based on a predicate applied to
// individual characters. Using string_view so no extra copying is required.
template <typename T>
StringList split_into_fields(std::string_view str, T predicate) {
    StringList fields;

    auto splits = detail::split_if_all(std::begin(str), std::end(str), predicate);
    fields.reserve(splits.size());
    const char *start = *std::begin(splits);
    detail::for_each_adjacent(std::begin(splits), std::end(splits),
        [&] (const char *a, const char* b) {
            if (a == b) {
                fields.emplace_back();
                return;
            }
            // Skip the separator.
            if (a != start) ++a;
            fields.emplace_back(a, b - a);
        });
    return fields;
}

} // end of namespace csv::detail

namespace csv {

class CsvChanger {
    using OptString = std::optional<std::string_view>;

    static OptString no_replace() {
        return {};
    }

    using FptrType = decltype(&no_replace);
    using LambdaType = std::function<OptString()>;
    using FuncVariant = std::variant<FptrType, LambdaType>;
    using ActionList = std::vector<FuncVariant>;

public:
    CsvChanger(std::istream &input, char sep = ',')
    : m_input(input)
    , m_separator(sep)
    {
        std::getline(m_input, m_titles);
        m_fields = split(m_titles);
        if (m_fields.size() == 0) {
            throw(std::out_of_range("No fields in csv file"));
        }

        // Build a vector of actions to take on the fields.
        // Use cheap-to-copy function pointers everywhere
        // except where we need replacement.
        m_actions = ActionList{m_fields.size(), no_replace};
    }

    bool set_replacement_field(std::string_view column_name,
          std::string_view replacement)
    {
        size_t selected_pos = 0;
        if (auto it = std::find(std::begin(m_fields), std::end(m_fields), column_name);
            it == std::end(m_fields))
        {
                return false;
        }
        else {
            selected_pos = it - std::begin(m_fields);
        }

       std::string replacement_string(replacement.data(), replacement.size());
       LambdaType do_replace =
            [replace = replacement_string] () {
            return OptString{replace};
        };

        // Store the action to be done for this column.
        m_actions[selected_pos] = do_replace;
        return true;
    }

    void output_modified_csv(std::ostream &out)
    {
        out << m_titles << '\n';

        while (m_input.good()) {
            std::string data_line;
            std::getline(m_input, data_line);
            StringList data_fields = split(data_line);
            if (data_fields.size() == 0) {
                // Deals with a possible empty line at the end.
                continue;
            }

            // The zip iterator brings the fields and actions together.
            // If the fields in a line in the file are less than in the title,
            // or vice-versa, use the shorter of the two lengths.
            // Replacements will only happen when the input is long enough.
            bool first = true;
            for (auto&& [field, action]: combine::limited_combine(data_fields, m_actions)) {
                if (!first) {
                    out << m_separator;
                }
                first = false;

                if (auto maybe_replaced = get_possibly_replaced_field(action); maybe_replaced)
                {
                    out << *maybe_replaced;
                } else {
                    out << field;
                }
            }
            out << '\n';
        }
    }

private:
    OptString get_possibly_replaced_field(const FuncVariant& action) {
        return std::visit(
            // Uniform invoke syntax allows a simple visit.
            [] (auto && act) { return std::invoke(act); }
            , action);
    }

    StringList split(std::string_view str) {
        return detail::split_into_fields(str,
            [sep = m_separator] (const char a) { return a == sep; });
    }

private:
    std::istream  &m_input;
    const char m_separator;
    std::string m_titles;
    StringList m_fields;
    ActionList m_actions;
};

} // end of namespace csv

int main(int argc, char* argv[])
{
    if (argc < 5) {
        std::cout << "usage: a.out input column_name replacement output\n";
        return 1;
    }

    std::ifstream ifs(argv[1]);
    if (!ifs) {
        std::cout << "input file missing\n";
        return 1;
    }

    try {
        // Reads the header only. The rest will be read during the output loop.
        // It can throw on an empty file or an empty first line.
        csv::CsvChanger splitter(ifs);

        // We could set more than one replacement.
        if (!splitter.set_replacement_field(argv[2], argv[3])) {
        std::cout << "column name doesn’t exist in the input file\n";
            return 1;
        }

        std::ofstream out(argv[4]);
        if (!out) {
            std::cout << "cannot open output file\n";
            return 1;
        }

        splitter.output_modified_csv(out);
    }
    catch (const std::exception& e) {
        std::cout << "Failure with exception: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

