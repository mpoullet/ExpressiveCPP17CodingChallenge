// http://coliru.stacked-crooked.com/a/52f0c373a2bcbb17

// Your name: Sai Jagannath
// Your email: jagan.sai@outlook.com

/*
 ** C++17 features:
 **    1) Structured Bindings. This helps in deconstruct the return types.
 **    2) std::string_view. This helps in not creating copies of std::string.
 *
*/

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <iterator>
#include <map>

namespace UtilItems {
    // enum class to hold the types of input.
    enum class InputItems : int
    {
        input = 1,
        column_name = 2,
        column_value_to_be_changed = 3,
        output = 4
    };

    std::ostream& operator<<(std::ostream& os, const InputItems& other) {
        os << static_cast<int>(other);
        return os;
    }

    // utility value class to Parse the csv line.
    // This value class is written to remove the code duplication
    // of parsing the line in 2 different functions.

    struct ParseLine {
        std::string_view line_view;
        std::string_view::iterator beg;
        std::string_view::iterator cur;

        ParseLine(std::string_view line) : line_view(line) {
            beg = line_view.begin();
            cur = line_view.begin();
        }

        ParseLine(const ParseLine&) = delete;
        ParseLine() = delete;
        ParseLine& operator= (const ParseLine&) = delete;
        ParseLine(ParseLine&&) = delete;
        ParseLine& operator=(ParseLine&&) = delete;

        bool hasNext() const {
            return cur != line_view.end();
        }

        std::string_view getWord(const char token) {
            cur = std::find(cur, line_view.end(), token);
            auto word = std::string_view{ &*beg, static_cast<std::string_view::size_type>(std::distance(beg, cur)) };
            if (cur != line_view.end()) {
                beg = ++cur;
            }
            return word;
        }

        size_t currOffSet() const {
            return std::distance(line_view.begin(), cur);
        }

        std::string_view::iterator end() const {
            return line_view.end();
        }

        std::string_view::iterator begin() const {
            return line_view.begin();
        }
    };

    std::ostream& usage(std::ostream& os) {
        os << "Usage:Tool.exe input.csv <ColumnName> <Replacement String> output.csv\n";
        return os;
    }

}

using namespace UtilItems;

std::map<InputItems, std::string> parseInput(int argc, char* argv[]) {

    std::map<InputItems, std::string> inputItemsMap;

    for (int index = 1; index < argc; ++index) {
        switch (static_cast<InputItems>(index))
        {
        case InputItems::input: inputItemsMap.insert(std::make_pair(InputItems::input, argv[index])); break;
        case InputItems::column_name: inputItemsMap.insert(std::make_pair(InputItems::column_name, argv[index])); break;
        case InputItems::column_value_to_be_changed: inputItemsMap.insert(std::make_pair(InputItems::column_value_to_be_changed, argv[index])); break;
        case InputItems::output: inputItemsMap.insert(std::make_pair(InputItems::output, argv[index])); break;
        default:
            break;
        }
    }
    return inputItemsMap;
}

// We just need to know at which inddex the column_name
// is present in the header.
std::tuple<std::string, int> getIndexOfColumnName(std::ifstream& inpfile, const std::string& column_name) {
    std::string line;
    getline(inpfile, line);
    ParseLine parseline{ line.data() };
    int index = 0;
    while (parseline.hasNext()) {
        auto word = parseline.getWord(',');
        if (word == column_name)
            return std::make_tuple(line, index);
        ++index;
    }
    // did not find the column_name.
    return std::make_tuple(line, -1);
}

// based on that index, we only need to parse the line till we reach that index.
// No need to parse till the end of the line.
// This is useful if the lines are big
std::tuple<size_t, std::string_view> getFieldValueAtIndex(const std::string& line, const int index) {
    ParseLine parseLine{ line.data() };
    int currIndex = -1;
    while (parseLine.hasNext()) {
        auto word = parseLine.getWord(',');
        ++currIndex;
        if (currIndex == index) {
            auto offset = parseLine.currOffSet();
            auto offsetIndex = offset == line.size() ? parseLine.currOffSet() - word.size() : parseLine.currOffSet() - word.size() -1;
            return std::make_tuple(offsetIndex, word);
        }
    }
    throw std::runtime_error("could not find the index in the line.Invalid data.");
}

int main(int argc, char* argv[])
{
    if (argc != 5) {
        usage(std::cout);
        return -1;
    }
    try {
        auto inputItemsMap = parseInput(argc, argv);
        // open input file.
        std::ifstream inpfile(inputItemsMap[InputItems::input], std::ios::in);
        if (!inpfile.is_open()) {
            std::cerr << "Input file:" << inputItemsMap[InputItems::input] << " missig\n";
            return -1;
        }

        auto[header, index] = getIndexOfColumnName(inpfile, inputItemsMap[InputItems::column_name]);
        if (index == -1) {
            std::cerr << "column name(" << inputItemsMap[InputItems::column_name] << ") doesn’t exist in the input file\n";
            return -1;
        }

        std::ofstream outfile(inputItemsMap[InputItems::output], std::ios::out);
        if (!outfile.is_open()) {
            std::cerr << "Invalid output file:" << inputItemsMap[InputItems::output] << '\n';
            return -1;
        }

        outfile << header << "\n";

        std::string line;
        while (getline(inpfile, line)) {
            auto[startIndex, value] = getFieldValueAtIndex(line, index);
            line.replace(startIndex, value.size(), inputItemsMap[InputItems::column_value_to_be_changed]);
            outfile << line << "\n";
        }
    } catch (std::runtime_error& err) {
        std::cerr << err.what() << '\n';
    }
}
