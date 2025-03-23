#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <iomanip>
#include <sstream>
#include <getopt.h>  // For getopt_long

void print_usage() {
	std::cerr << "\nUsage: pmatches [OPTIONS] REGEX FILENAME FORMAT\n";
	std::cerr << "Options:\n";
	std::cerr << "  -h, --help      Show this help message and exit\n";
	std::cerr << "  -v, --verbose   Enable verbose output\n";
	std::cerr << std::endl << NAMESTRING << std::endl;
	std::cerr << "report bugs to <aempirei@gmail.com>" << std::endl << std::endl;
}

std::string to_base_string(int number, int base, int padding) {
    std::string result;
    bool is_negative = number < 0;
    number = std::abs(number);

    do {
        int remainder = number % base;
        // For bases greater than 10, use alphabetic characters
        if (remainder < 10) {
            result += (remainder + '0');
        } else {
            result += (remainder - 10 + 'a'); // Use lowercase
        }
        number /= base;
    } while (number > 0);

    if (is_negative) {
        result += '-'; // Handle negative numbers if needed
    }

    std::reverse(result.begin(), result.end()); // Reverse to correct digit order

    // Apply padding
    if (padding > 0) {
        result = std::string(padding - result.size(), '0') + result; // Prepend zeros as necessary
    }

    return result;
}

void create_and_write_file(const std::string& format, int index, const std::string& content, bool verbose) {
    std::stringstream ss;
    std::string filename = format;

    std::regex re("%(\\d+)?#(\\d+)");
    std::smatch match;
    int padding = 0; // default padding is none
    int base = 10;   // default to decimal if not specified

    while (std::regex_search(filename, match, re)) {
        if (match[1].length() > 0) {
            padding = std::stoi(match[1].str());
        }
        
        base = std::stoi(match[2].str());
        if (base < 2 || base > 36) {
            throw std::invalid_argument("Base must be between 2 and 36.");
        }

        // Format the index into the specified base
        ss.str(""); // Clear stringstream
        ss << to_base_string(index, base, padding);
        filename = match.prefix().str() + ss.str() + match.suffix().str();
    }

    // Create the directory if it does not exist
    std::filesystem::path dir = std::filesystem::path(filename).parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir))
        std::filesystem::create_directories(dir);
    
    // Write content to the file
    std::ofstream outfile(filename);
    
    // Verbose file creation output when opening
    if (verbose) {
        std::cout << "Creating file: " << filename;
        std::cout.flush(); // Flush the stream to immediately show output
    }

    if (outfile.is_open()) {
        outfile << content;
        outfile.close();
        
        // Print file size after closing the file
        if (verbose)
            std::cout << " (" << std::filesystem::file_size(filename) << " bytes)\n";
    } else {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
    }
}

void match_file(const std::string& regex_str, std::istream& input_stream, const std::string& format, bool verbose) {
    std::ostringstream buffer;
    buffer << input_stream.rdbuf(); // Read entire content into buffer
    std::string content = buffer.str();

    // Use regex to match the entire content in multiline mode
    std::regex re(regex_str, std::regex_constants::ECMAScript | std::regex_constants::multiline);
    std::sregex_iterator iter(content.begin(), content.end(), re);
    std::sregex_iterator end;

    for (int index = 0; iter != end; ++iter, ++index) 
        create_and_write_file(format, index, (*iter)[0].str(), verbose);
}

int main(int argc, char* argv[]) {
    int opt;
    bool verbose = false;
    std::string regex_str, filename, format;

    // Define the long options
    struct option long_options[] = {
        {"help",    no_argument,       nullptr, 'h'},
        {"verbose", no_argument,       nullptr, 'v'},
        {nullptr,   0,                 nullptr,  0 }
    };

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "hv", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'v':
                verbose = true;
                break;
            case '?':
            default:
                print_usage();
                return 1;
        }
    }

    // Get positional arguments
    if (optind + 3 > argc) {
        print_usage();
        return 1;
    }

    regex_str = argv[optind++];
    filename = argv[optind++];
    format = argv[optind++];

    // Determine input source
    if (filename == "-") {
        match_file(regex_str, std::cin, format, verbose); // Read from standard input
    } else {
        std::ifstream infile(filename);
        if (!infile.is_open()) {
            std::cerr << "Error: Unable to open file " << filename << std::endl;
            return 1;
        }
        match_file(regex_str, infile, format, verbose); // Read from the specified file
    }

    return 0;
}
