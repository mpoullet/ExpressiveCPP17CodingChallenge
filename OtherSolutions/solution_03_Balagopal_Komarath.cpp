// http://coliru.stacked-crooked.com/a/becf46744e41d7b8

/*
 * Author: Balagopal Komarath <baluks@gmail.com>
 *
 * c++17 features used: std::string_view, fold expressions.
 */
#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <fstream>
#include <algorithm>

// A non-owning view of a record in the csv file.
using Fields = std::vector<std::string_view>;

// for empty fields in the CSV file.
using namespace std::literals;
static auto empty_field = ""sv;

void addField(Fields& f, const char* beg, const char* end)
{
    if (beg != end)
        f.emplace_back(beg, std::distance(beg, end));
    else
        f.emplace_back(empty_field);
}

void parseCSV(const std::string& line, Fields& fields)
{
    fields.clear();
    auto cur = line.data();
    auto end = line.data() + line.size();
    auto prev = cur;
    for (/* nothing */; cur != end; ++cur)
    {
        if (*cur == ',') {
            addField(fields, prev, cur);
            prev = cur + 1;
        }
    }
    addField(fields, prev, end);
}

void writeCSV(std::ostream& out, const Fields& fields)
{
    std::size_t i;
    for (i = 0; i < fields.size() - 1; ++i)
    {
        out << fields[i] << ',';
    }
    out << fields[i] << '\n';
}

template<typename... Str>
void ensure(bool condition, Str... msg)
{
    if (!condition)
    {
        (std::cerr << ... << msg) << std::endl;
        exit(0);
    }
}

// Destructors are run for these when calling exit().
static std::ifstream csvin;
static std::ofstream csvout;
static std::string   buffer;
static Fields        fields;

int main(int argc, char **argv)
{
    const auto progname = argv[0];
    ensure(argc == 5,
           "usage: ", progname, " infile column replacementText outfile");
    const auto
        infile = argv[1],
        column = argv[2],
        replaceTo = argv[3],
        outfile = argv[4];

    csvin.open(infile);

    ensure(!csvin.fail(),
           progname,
           ": cannot open input file ", infile, ".");

    getline(csvin, buffer);
    ensure(!csvin.fail(),
           progname, ": input file is empty.");

    parseCSV(buffer, fields);
    const auto total_fields = fields.size();
    auto col = find(fields.begin(), fields.end(), column);
    ensure(col != fields.end(),
           progname,
           ": column name ", column,
           " doesn't exist in input file");

    csvout.open(outfile);
    ensure(!csvout.fail(),
           progname,
           ": cannot open output file ", outfile, ".");

    const auto nfield = std::distance(fields.begin(), col);
    writeCSV(csvout, fields);
    while (getline(csvin, buffer))
    {
        parseCSV(buffer, fields);
        ensure(fields.size() == total_fields,
               progname, ": error in csv file.");
        fields[nfield] = replaceTo;
        writeCSV(csvout, fields);
    }
}

