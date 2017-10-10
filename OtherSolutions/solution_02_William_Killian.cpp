// http://coliru.stacked-crooked.com/a/aa097931c09266a6

// Your name: William Killian
// Your email: william.killian@gmail.com

// C++17 Library Features
//  - absence of std::iterator (due to deprecation)
//  - string_view (string is only used for getline)
//  - filesystem (verify file exists without opening fstream)
//  - non-member size() and empty()
//  - optional
//  - pair deduction guide (no make_pair)

// C++17 Language Features
//  - [[nodiscard]] on appropriate functions
//  - if-init heavily used for error detection
//  - structured bindings for packed return values
//  - copy elision

// Bonus
//  - make_ostream_joiner() used for CSV printing (Lib Fundamentals V2)
//  - constexpr as much as I could. Unfortunately, many string_view functions are not marked constexpr when they should be
//  - implemented a fancy csv_field "iterator" to simplify the solution
//  - exchange() [from C++14] to advance iterators
//  - IIFE (learned about this through @lefticus [Jason Turner]) to initialize "const" things

// Why I didn't use some features:
//  - nested namespaces -- this problem is too small of a scope for nested namespace declarations
//  - variant/any -- we never have more than an optional type required. variant and any are overkill
//  - constexpr if -- nothing here is done at compile time or makes use of compile-time decisions
//  - fold expressions -- nothing is variadic, so fold expressions are moot
//  - static_assert / typename templates -- really no TMP required here
//  - __has_include -- this was only to be tested on GCC 7.2. I would have added checks for string_view and filesystem (both with and without experimental prefix)

#include <algorithm>
#include <experimental/filesystem>
#include <experimental/iterator>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::experimental::filesystem;
using namespace std::literals::string_view_literals;

constexpr std::string_view DELIM = ","sv;

struct csv_field {
  using iterator_category = std::input_iterator_tag;
  using value_type = std::string_view;
  using pointer = void;
  using reference = void;
  using difference_type = void;

  constexpr csv_field()
  : lineOfText{""},
    delimiter{DELIM},
    startIndex{0lu},
    endIndex{0lu} {}

  csv_field(std::string_view text, std::string_view delim)
  : lineOfText{text},
    delimiter{delim},
    startIndex{0lu},
    endIndex{text.find(delimiter)} {}

  constexpr csv_field(csv_field const &) = default;

  csv_field& operator++() {
    if (endIndex != std::string_view::npos)
      endIndex += std::size(delimiter);
    startIndex = std::exchange(endIndex, lineOfText.find(delimiter, endIndex));
    return *this;
  }

  csv_field operator++(int) {
    csv_field old{*this};
    ++(*this);
    return old;
  }

  constexpr bool operator==(csv_field const & other) {
    return ((startIndex == endIndex)
            && (other.startIndex == other.endIndex))
      || ((startIndex == other.startIndex)
          && (endIndex == other.endIndex)
          && (lineOfText == other.lineOfText));
  }

  bool operator!=(csv_field const & other) {
    return !(operator==(other));
  }

  [[nodiscard]] std::string_view operator*() const {
    return (endIndex == std::string_view::npos)
      ? lineOfText.substr(startIndex)
      : lineOfText.substr(startIndex, endIndex - startIndex);
  }

private:
  std::string_view lineOfText;
  std::string_view delimiter;
  std::size_t startIndex;
  std::size_t endIndex;
};

[[nodiscard]] std::vector<std::string_view>
parse_line(std::string_view text) {
  return {csv_field{text, DELIM}, {}};
}

[[nodiscard]] auto
index_of_header(std::string_view line, std::string_view headerToFind) {
  auto const headers = parse_line(line);
  auto const dist = [&] () -> std::optional<std::size_t> {
    auto const loc = std::find(std::begin(headers), std::end(headers), headerToFind);
    if (loc != std::end(headers))
      return {std::distance(std::begin(headers), loc)};
    return {};
  }();
  return std::pair(dist, headers);
}

int
main(int argc, char* argv[]) try {
  if (argc != 5) {
    throw std::invalid_argument{"invalid argument count"};
  } else if(fs::path inputCSV{argv[1]}; !fs::exists(inputCSV)) {
    throw std::runtime_error{"input file missing"};
  } else if(std::ifstream inputFile{inputCSV}; !inputFile) {
    throw std::runtime_error{"input file missing"};
  } else if(std::string line; !std::getline(inputFile, line) || std::empty(line)) {
    throw std::runtime_error{"input file missing"};
  } else if(auto const [hIndex, header] = index_of_header(line, argv[2]); !hIndex) {
    throw std::invalid_argument{"column name doesn’t exist in the input file"};
  } else {
    std::string_view defaultField{argv[3]};

    fs::path outputCSV{argv[4]};
    std::ofstream outputFile{outputCSV};
    auto printer = std::experimental::make_ostream_joiner(outputFile, DELIM);

    // print header
    std::copy(std::begin(header), std::end(header), printer);
    outputFile << '\n';

    // print each row
    while (std::getline(inputFile, line)) {
      auto const fields = parse_line(line);
      std::transform(std::begin(fields), std::end(fields), printer,
        [=, index = 0lu] (std::string_view field) mutable {
          return (hIndex.value() == index++)
            ? defaultField
            : field;
        });
      outputFile << '\n';
    }
    return EXIT_SUCCESS;
  }
} catch (std::exception const& e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}

