// http://coliru.stacked-crooked.com/a/a9a8b17c796103c2

// Your name: Yann Labou
// Your email: Yann.Labou@gmail.com

// Cpp17 Features
// - std::string_view
// - Initializer in 'if' and 'switch'
// - Filesystem

#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <experimental/iterator> //http://en.cppreference.com/w/cpp/experimental/ostream_joiner
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

template<typename C>
std::vector<std::basic_string<C>> split(const std::basic_string<C>& text, C delimiter = ',')
{
    using bs_t = std::basic_string<C>;
    std::vector<bs_t> rv;
    auto p = std::begin(text);
    for (auto q = std::find(p, std::end(text), delimiter);
         q != std::end(text);
         q = std::find(++p, std::end(text), delimiter))
    {
        rv.emplace_back(p, q);
        p = q;
    }
    if (p != std::end(text))
        rv.emplace_back(p, std::end(text));
    return rv;
}

template<typename Container>
void write_csv_row(Container first, Container last,
                   std::string_view filename,
                   std::string_view delimiter,
                   size_t line_number)
{
    std::ofstream ofs{filename.data(),
                      std::ios::out | (line_number ? std::ios::app : std::ios::trunc) | std::ios::binary};
    std::copy(first, last - 1, std::ostream_iterator<std::string>(ofs, delimiter.data()));
    std::copy(last - 1, last, std::ostream_iterator<std::string>(ofs, "\n"));
    ofs.close();
}

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        std::cout << "Invalid number of arguments!" << '\n';
        std::vector<std::string> args{"Usage:", argv[0], "<csv_filename>",
                "<column_override>", "<text_replacement>", "<output_file>", "\n"};
        std::copy(std::begin(args),
                  std::end(args),
                  std::experimental::make_ostream_joiner(std::cout, " "));
        return EXIT_FAILURE;
    } else if (fs::path const& csv_input{argv[1]}; !fs::exists(csv_input))
    {
        std::cout << "input file not existing on file system" << '\n';
        return EXIT_FAILURE;
    } else if (!fs::file_size(csv_input))
    {
        std::cout << "input file missing" << '\n';
        return EXIT_FAILURE;
    }

    std::string_view csv_input{argv[1]};
    std::string_view column_name{argv[2]};
    std::string_view replacement_text{argv[3]};
    std::string_view csv_output{argv[4]};

    std::string csv_line;
    std::vector<std::string> csv_fields;
    std::vector<std::string>::iterator city_column;
    std::ptrdiff_t index;
    std::size_t line_number = 0;

    std::ifstream ifs{csv_input.data()};
    while(std::getline(ifs, csv_line))
    {
        csv_fields = split(csv_line);
        if (!line_number)
        {
            city_column = std::find(csv_fields.begin(), csv_fields.end(), column_name);
            if (!(city_column != csv_fields.end()))
            {
                std::cerr << "column name doesn't exists in the input file" << '\n';
                return EXIT_FAILURE;
            }
            index = std::distance(csv_fields.begin(), city_column);
        }
        else
        {
            csv_fields.at(index) = replacement_text;
        }
        write_csv_row(csv_fields.begin(), csv_fields.end(), csv_output, ",", line_number);
        ++line_number;
    }
    return EXIT_SUCCESS;
}

