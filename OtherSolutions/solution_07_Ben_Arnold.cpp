// http://coliru.stacked-crooked.com/a/95759e40ebd711e1

// Your name: Ben Arnold
// Your email: ben@arnoldfamily.me.uk

// c++17 features
// - std::optional to replace sentinal values in higher abstractions
// - std::string_view to minimise string copies
// - if initializer statements
// - nested namespace definitions
// - structured binding for tuple return
// - fold expressions to concatenate string_views into a string
// - Template argument deduction for class templates
// - Guaranteed copy elision
// - Deprecated deriving from std::iterator
//
// c++14 features
// - auto deduced return type
// - auto deduced lambda type
#include <exception>
#include <iostream>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <string>
#include <numeric>
#include <functional>
#include <algorithm>
#include <istream>
#include <iterator>

// Same as std::ostream_iterator but only emits the delimiter *between* other items.
template<class T,
    class Elem = char,
    class Traits = std::char_traits<Elem> >
    class ostream_infix_iterator
{
public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using pointer = void;
    using reference = void;

    using char_type = Elem;
    using traits_type = Traits;
    using ostream_type = std::basic_ostream<Elem, Traits>;

    ostream_infix_iterator(ostream_type& ostr, const Elem * const delim = 0)
        : delim(delim), ostr(std::addressof(ostr))
    { }

    ostream_infix_iterator& operator=(const T& value)
    {    // insert value into output stream,
        // preceeded by delimiter unless this is the first value
        if (!first && delim)
            *ostr << delim;

        *ostr << value;

        first = false;
        return (*this);
    }

    ostream_infix_iterator& operator*() { return (*this); }
    ostream_infix_iterator& operator++() { return (*this); }
    ostream_infix_iterator& operator++(int) { return (*this); }

protected:
    bool first = true;
    const Elem * delim;
    ostream_type * ostr;
};


namespace replacer::detail
{
    // Lift `find` into an `option`
    template<class Str>
    constexpr auto find(const Str & str, typename Str::value_type ch, const typename Str::size_type offset = 0)
    {
        auto found = str.find(ch, offset);
        return found == std::string::npos
            ? std::nullopt
            : std::make_optional(found);
    }

    auto split(const std::string_view & toSplit)
    {
        // Unfortunately std::string_view doesn't work with std::istream_iterator or std::getline
        // out of the box.  So instead of using WordDelimitedByComma specialisation of std::istream_iterator
        // directly to initialise a vector, do it manually...

        std::vector<std::string_view> out;

        size_t firstComma = 0;
        auto lastComma = std::make_optional(firstComma);

        while (lastComma = find(toSplit, ',', *lastComma))
        {
            out.push_back(toSplit.substr(firstComma, *lastComma - firstComma));
            firstComma = ++(*lastComma);
        }

        out.push_back(toSplit.substr(firstComma));

        return out;
    }

    template<typename InputIterator, typename N, typename UnaryPredicate>
    auto find_nth_if(InputIterator first, InputIterator last, N n, UnaryPredicate predicate)
    {
        if (n <= 0) return last;

        while (n-- > 0 && first != last)
        {
            first = std::find_if(first, last, predicate);
            if (first != last)
                ++first;
        }

        return first;
    }

    template<typename InputIterator, typename N, typename Value>
    auto find_nth(InputIterator first, InputIterator last, N n, Value value)
    {
        return find_nth_if(first, last, n, [value](auto x)
        {
            return x == value;
        });
    }

    template<typename InputIterator, typename N, typename Value>
    auto find_nth_delimited_by(InputIterator first, InputIterator last, N n, Value value)
    {
        auto start = find_nth_if(first, last, n, [value](auto x)
        {
            return x == value;
        });

        auto end = std::find(start, last, value);

        return std::make_tuple(start, end);
    }

    // A not-really but kind of range version.
    template<typename InputIterator, typename N, typename Value>
    auto find_nth_delimited_by(InputIterator seq, N n, Value value)
    {
        return find_nth_delimited_by(std::begin(seq), std::end(seq), n, value);
    }

    template<typename Str>
    std::optional<size_t> index_of(const std::vector<Str> & row, const std::string_view & name)
    {
        const auto end = std::end(row);

        if (auto column = std::find(std::begin(row), end, name); column != end)
            return std::distance(std::begin(row), column);

        return -1;
    }

    std::string operator + (std::string const & left, std::string_view right)
    {
        return left + std::string{ right.data(), right.size() };
    }

    template<typename ...Args>
    auto concat_views(Args&&... args)
    {
        return (std::string{} +... + args);
    }
}

namespace replacer::runtime
{
    struct arguments
    {
        static auto parse(int argc, const char * argv[])
        {
            return argc > 4
                ? std::make_optional(arguments(argv[1], argv[2], argv[3], argv[4]))
                : std::nullopt;
        }

        const std::string_view input;
        const std::string_view column;
        const std::string_view replacement;
        const std::string_view output;

    private:
        arguments(std::string_view input, std::string_view column, std::string_view replacement, std::string_view output)
            : input(input)
            , column(column)
            , replacement(replacement)
            , output(output)
        {}
    };
}


namespace replacer::row
{
    template<class InputStream>
    std::optional<std::string> getline(InputStream&& input)
    {
        std::string line;
        return std::getline(input, line)
            ? std::make_optional(line)
            : std::nullopt;
    }

    template<typename Str>
    auto replacement_column_index(std::vector<Str> columns, std::string_view name)
    {
        if (auto index = replacer::detail::index_of(columns, name))
            return *index;
        throw std::runtime_error("column name doesn’t exists in the input file");
    }

    auto find_column_with_text(std::string_view header, std::string_view name)
    {
        auto columns = replacer::detail::split(header);
        return replacement_column_index(columns, name);
    }

    template<typename InputIterator, typename N>
    auto find_column_bounds(InputIterator seq, N n)
    {
        return replacer::detail::find_nth_delimited_by(std::begin(seq), std::end(seq), n, ',');
    }

    auto replace(std::string_view input, std::string_view::iterator begin_replace, std::string_view::iterator end_replace, std::string_view replacement)
    {
        if (begin_replace == end_replace) return std::string(input.data(), input.size());

        const auto begin = std::begin(input);
        const auto end = std::end(input);

        const auto left = input.substr(0, begin_replace - begin);
        const auto right = input.substr(end_replace - begin, end - end_replace);

        return replacer::detail::concat_views(left, replacement, right);
    }
}


namespace replacer
{
    auto open_file(const std::string_view & filename)
    {
        auto file = std::ifstream(std::string{ filename });
        if (!file) throw std::runtime_error("input file missing");
        return file;
    }

    template<class InStream, class OutStream, class Fn>
    auto transform_lines(InStream&& source, OutStream&& dest, Fn fn)
    {
        std::istream_iterator<std::string> begin{ source };
        std::istream_iterator<std::string> end{};

        return std::transform(begin, end, ostream_infix_iterator<std::string>(dest, "\n"), fn);
    }

}



// This is the main algorithm.  Almost everything else exists to try and make this function expressive and easy to read.
template<typename Output>
void convert(std::ifstream&& csv, std::string_view columnHeading, std::string_view replacement, Output&& output)
{
    auto headerRow = replacer::row::getline(csv).value_or("");

    output << headerRow << "\n";

    const auto column_index = replacer::row::find_column_with_text(headerRow, columnHeading);

    replacer::transform_lines(csv, output, [=](std::string_view view)
    {
        auto[start, end] = replacer::row::find_column_bounds(view, column_index);
        return replacer::row::replace(view, start, end, replacement);
    });
}

auto convert(replacer::runtime::arguments args)
{
    convert(
        replacer::open_file( args.input ),
        args.column,
        args.replacement,
        std::ofstream{ std::string{ args.output } });
}

int main(int argc, const char * argv[])
{
    try
    {
        auto args = replacer::runtime::arguments::parse(argc, argv);
        if (!args) throw std::runtime_error("Invalid arguments");

        convert(*args);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << "\n";
        return 1;
    }
}
