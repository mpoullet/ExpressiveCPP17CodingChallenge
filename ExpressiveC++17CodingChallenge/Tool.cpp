// https://www.fluentcpp.com/2017/09/25/expressive-cpp17-coding-challenge/

// The task proposed in the challenge is to write a command line tool that:
// 1 - takes in a CSV file
// 2 - overwrites all the data of a given column by a given value
// 3 - outputs the results into a new CSV file
// More specifically, this command line tool should accept the following arguments :
// 1 - the filename of a CSV file
// 2 - the name of the column to overwrite in that file
// 3 - the string that will be used as a replacement for that column
// 4 - the filename where the output will be written.

// For instance, if the CSV file had a column “City” with various values for the entries in the file,
// calling the tool with the name of the input file, City, London and the name of output file would result in a copy of the initial file,
// but with all cities set equal to “London”:

// Tool.exe input.csv City London output.csv

// Here is how to deal with edge cases:
// if the input file is empty, the program should write “input file missing” to the console.
// if the input file does not contain the specified column, the program should write “column name doesn’t exists in the input file” to the console.
// In both cases, there shouldn’t be any output file generated.
// And if the program succeeds but there is already a file having the name specified for output, the program should overwrite this file.

// Tools:
// C++17 only, no external libraries
// VS2017
// snake_case, no CamelCase

// https://stackoverflow.com/a/15595559/496459
// Properties -> Configuration Properties -> Linker -> System -> SubSystem -> Console(/ SUBSYSTEM:CONSOLE)

// CSV parsing:
// https://stackoverflow.com/a/1120224/496459

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

namespace fs = std::experimental::filesystem;

namespace tool {
	const auto NUMBER_OF_PARAMETERS = 4;
	
	enum error_codes {
		NOT_ENOUGH_PARAMETERS = 1,
		NO_CSV_INPUT_FILE = 2,
		NO_COLUMN_NAME = 3
	};

	enum parameter_position {
		CSV_INPUT_FILE = 1,
		COLUMN_NAME = 2,
		REPLACEMENT_STRING = 3,
		CSV_OUTPUT_FILE = 4
	};

	auto split_line_into_tokens(std::istream& str)
	{
		std::vector<std::string> result;
		std::string              line;
		std::getline(str, line);

		std::istringstream line_stream(line);
		std::string       cell;

		while (std::getline(line_stream, cell, ','))
		{
			result.push_back(cell);
		}

		return result;
	}

	auto merge_tokens_into_line(std::vector<std::string>& tokens)
	{
		std::ostringstream oss;
		for (const auto& token : tokens)
		{
			oss << token << ',';
		}

		std::string line = oss.str();
		line.pop_back();
		return line;
	}
}

int main(int argc, char* argv[])
{
	if (argc != tool::NUMBER_OF_PARAMETERS + 1)
	{
		return tool::error_codes::NOT_ENOUGH_PARAMETERS;
	}

	auto input_filename = argv[tool::parameter_position::CSV_INPUT_FILE];
	if (!fs::exists(input_filename))
	{
		std::cerr << "input file missing\n";
		return tool::error_codes::NO_CSV_INPUT_FILE;
	}

	std::ifstream file(input_filename);
	auto column_names = tool::split_line_into_tokens(file);

	for (const auto& token : column_names) std::cout << token << '\n';

	std::string wanted_column = argv[tool::parameter_position::COLUMN_NAME];
	if (std::find(std::begin(column_names), std::end(column_names), wanted_column) == std::end(column_names))
	{
		std::cerr << "column name doesn't exists in the input file\n";
		return tool::error_codes::NO_COLUMN_NAME;
	}

	std::cout << tool::merge_tokens_into_line(column_names) << '\n';
}