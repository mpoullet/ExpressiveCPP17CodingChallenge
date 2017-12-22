// https://www.fluentcpp.com/2017/09/25/expressive-cpp17-coding-challenge/
// http://www.bfilipek.com/2017/09/the-expressive-cpp17-challenge.html

// The task proposed in the challenge is to write a command line tool that:
// 1 - takes in a CSV file
// 2 - overwrites all the data of a given column by a given value
// 3 - outputs the results into a new CSV file
// More specifically, this command line tool should accept the following arguments :
// 1 - the filename of a CSV file
// 2 - the name of the column to overwrite in that file
// 3 - the string that will be used as a replacement for that column
// 4 - the filename where the output will be written.

// For instance, if the CSV file had a column "City" with various values for the entries in the file,
// calling the tool with the name of the input file, City, London and the name of output file would result in a copy of the initial file,
// but with all cities set equal to "London":

// Tool.exe input.csv City London output.csv

// Here is how to deal with edge cases:
// if the input file is empty, the program should write "input file missing" to the console.
// if the input file does not contain the specified column, the program should write "column name doesn't exists in the input file" to the console.
// In both cases, there shouldn't be any output file generated.
// And if the program succeeds but there is already a file having the name specified for output, the program should overwrite this file.

// Tools:
// C++17 only, no external libraries
// VS2017
// snake_case, no CamelCase

// https://stackoverflow.com/a/15595559/496459
// Properties -> Configuration Properties -> Linker -> System -> SubSystem -> Console(/ SUBSYSTEM:CONSOLE)

// CSV parsing:
// https://stackoverflow.com/a/1120224/496459

// Finding an element and its index in a vector:
// https://stackoverflow.com/a/15099743/496459
// https://stackoverflow.com/q/2152986/496459

// My entry on coliru:
// http://coliru.stacked-crooked.com/a/122f7799b53dfba1

#include <algorithm>
#include <iostream>
#ifdef _WIN32
#include <filesystem>
#else
#include <experimental/filesystem>
#endif
#include <fstream>
#include <gsl/multi_span>
#include <sstream>
#include <string>

namespace fs = std::experimental::filesystem;

namespace tool {
	const auto NUMBER_OF_PARAMETERS{ 4 };
	
	namespace error_codes {
		constexpr auto NOT_ENOUGH_PARAMETERS{ 1 };
		constexpr auto NO_CSV_INPUT_FILE{ 2 };
		constexpr auto NO_COLUMN_NAME{ 3 };
	};

	namespace parameter_position {
		constexpr auto CSV_INPUT_FILE{ 1 };
		constexpr auto COLUMN_NAME{ 2 };
		constexpr auto REPLACEMENT_STRING{ 3 };
		constexpr auto CSV_OUTPUT_FILE{ 4 };
	};

	auto split_line_into_tokens(const std::string& line)
	{
		std::vector<std::string> result;

		std::istringstream line_stream(line);
		std::string       cell;

		while (std::getline(line_stream, cell, ','))
		{
			result.push_back(cell);
		}

		return result;
	}

	auto merge_tokens_into_line(const std::vector<std::string>& tokens)
	{
		std::ostringstream oss;
		for (const auto& token : tokens)
		{
			oss << token;
			oss << ',';
		}

		std::string line{ oss.str() };
		if (!line.empty()) {
			line.pop_back();
		}
		return line;
	}
} // namespace tool

int main(int argc, char* argv[])
{
	if (argc != tool::NUMBER_OF_PARAMETERS + 1)
	{
		return tool::error_codes::NOT_ENOUGH_PARAMETERS;
	}

	const auto args = gsl::multi_span<char*>(argv, argc);

	const auto input_filename{ args[tool::parameter_position::CSV_INPUT_FILE] };
	if (!fs::exists(input_filename))
	{
		std::cerr << "input file missing\n";
		return tool::error_codes::NO_CSV_INPUT_FILE;
	}

	std::ifstream input_file(input_filename);
	std::string column_line;
	std::getline(input_file, column_line);
	const auto column_names{ tool::split_line_into_tokens(column_line) };
	const auto number_of_columns{ column_names.size() };

	const std::string wanted_column{ args[tool::parameter_position::COLUMN_NAME] };

	const auto found_column{ std::find(std::begin(column_names), std::end(column_names), wanted_column) };
	if (found_column == std::end(column_names))
	{
		std::cerr << "column name doesn't exists in the input file\n";
		return tool::error_codes::NO_COLUMN_NAME;
	}
	const auto column_position{ std::distance(std::begin(column_names), found_column) };

	const std::string wanted_value{ args[tool::parameter_position::REPLACEMENT_STRING] };

	const auto output_filename{ args[tool::parameter_position::CSV_OUTPUT_FILE] };
	std::ofstream output_file(output_filename);
	output_file << column_line;
	output_file << '\n';

	std::string line;
	while (std::getline(input_file, line))
	{
		auto tokens{ tool::split_line_into_tokens(line) };
		if (tokens.size() == number_of_columns)
		{
			tokens[column_position] = wanted_value;
			output_file << tool::merge_tokens_into_line(tokens);
			output_file << '\n';
		}
		else {
			std::cout << "skipping line: " << tool::merge_tokens_into_line(tokens) << '\n';
		}
	}
}