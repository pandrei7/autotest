#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include "windows.h"

#elif __linux__
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>

#else
#error "OS not supported!"
#endif

using namespace std;

#ifdef _WIN32
const string kExecutableName = "supercalifragilistic.exe";
#elif __linux__
const string kExecutableName = "supercalifragilistic";
const int kTestTimeout = 3000;
#endif

// Tests if a string has a certain ending
inline bool ends_with(const string &str, const string &ending)
{
    if (ending.size() > str.size()) {
        return false;
    }
    return equal(ending.rbegin(), ending.rend(), str.rbegin());
}

#ifdef _WIN32
// Runs a process and waits for it to stop before continuing
void RunAndWait(const string &process, const string &parameters = "")
{
    SHELLEXECUTEINFO sh_exec_info = {0};
    sh_exec_info.cbSize = sizeof(SHELLEXECUTEINFO);
    sh_exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    sh_exec_info.hwnd = NULL;
    sh_exec_info.lpVerb = NULL;
    sh_exec_info.lpFile = process.c_str();
    sh_exec_info.lpParameters = parameters.c_str();
    sh_exec_info.lpDirectory = "";
    sh_exec_info.nShow = SW_HIDE;
    sh_exec_info.hInstApp = NULL;

    ShellExecuteEx(&sh_exec_info);
    WaitForSingleObject(sh_exec_info.hProcess, INFINITE);
}
#elif __linux__
// Transforms a string into an array of its tokens as C strings
char** Tokenize(const string &str)
{
    stringstream ss;
    ss << str;

    vector<string> tokens;
    string token;

    while (ss >> token) {
        tokens.push_back(token);
    }

    // The token array will end with a NULL pointer (for exec)
    char **c_tokens = (char**)malloc(sizeof(char*) * (tokens.size() + 1));

    for (size_t i = 0; i < tokens.size(); ++i) {
        c_tokens[i] = (char*)malloc(sizeof(char) * (tokens[i].size() + 3));
	strcpy(c_tokens[i], tokens[i].c_str());
	c_tokens[i][tokens[i].size()] = '\0';
    }

    // Add the null pointer at the end of the tokens array (for exec)
    c_tokens[tokens.size()] = NULL;
    return c_tokens;
}

// Runs a process and waits for it to stop before continuing
void RunAndWait(const string &process, const string &parameters = "")
{
    string complete_command = process + " " + parameters;
    pid_t my_pid;

    if ((my_pid = fork()) == 0) {
	execv(process.c_str(), Tokenize(complete_command));
	perror("child process execv failed");
	return;
    } else if (my_pid > 0) {
	wait(NULL); 
    }
}
#endif

#ifdef _WIN32
// Changes the current working directory of the process
inline void ChangeDirectory(const string &path)
{
    SetCurrentDirectory(path.c_str());
}
#elif __linux__
// Changes the current working directory of the process
inline void ChangeDirectory(const string &path)
{
    chdir(path.c_str());
}
#endif

#ifdef _WIN32
// Compiles the source whose name is given as a parameter
// Decides automatically which compile instruction to use based on file extension
void CompileSource(const string &source_name)
{
    string compile_process = "";
    string compile_parameters = "";

    if (ends_with(source_name, ".c")) {
        compile_process = "gcc";
        compile_parameters = "-Wall -Werror " + source_name + " -o " + kExecutableName;
    } else if (ends_with(source_name, ".cpp") || ends_with(source_name, ".cc")) {
        compile_process = "g++";
        compile_parameters = "-Wall -Werror -std=c++11 " + source_name + " -o " + kExecutableName;
    } else {
        throw invalid_argument(source_name + " does not have a supported file extension");
    }

    // Do the actual compiling
    RunAndWait(compile_process, compile_parameters);
}
#elif __linux__
// Compiles the source whose name is gives as a parameter
// Decides automatically which compile instruction to use based on file extension
void CompileSource(const string &source_name)
{
    string compile_process = "";
    string compile_parameters = "";

    if (ends_with(source_name, ".c")) {
        compile_process = "/usr/bin/gcc";
	compile_parameters = "-Wall -Werror " + source_name + " -o " + kExecutableName;
    } else if (ends_with(source_name, ".cpp") || ends_with(source_name, ".cc")) {
        compile_process = "/usr/bin/g++";
	compile_parameters = "-Wall -Werror -std=c++11 " + source_name + " -o " + kExecutableName;
    } else {
        throw invalid_argument(source_name + " does not have a supported file extension");
    }

    // Do the actual compiling
    RunAndWait(compile_process, compile_parameters);
}
#endif

int to_int(const string &str)
{
    int num = 0;
    int sign = 1;

    unsigned start_pos = 0;
    if (str[0] == '-') {
        sign = -1;
        ++start_pos;
    }

    for (unsigned i = start_pos; i < str.size(); ++i) {
        if (str[i] >= '0' && str[i] <= '9') {
            num = num * 10 + str[i] - '0';
        } else {
            throw invalid_argument(str + " is not a number");
        }
    }
    return sign * num;
}

// Returns the name of the file containing input for a certain test
inline string input_filename(const string &prefix, int test_number)
{
    stringstream ss;
    ss << prefix << "." << test_number << ".in";
    return ss.str();
}

// Returns the name of the input file used by the user program
inline string input_filename(const string &prefix)
{
    return prefix + ".in";
}

// Returns the name of the file containing the correct output for a certain test
inline string output_filename(const string &prefix, int test_number)
{
    stringstream ss;
    ss << prefix << "." << test_number << ".out";
    return ss.str();
}

// Returns the name of the output file generated by the user program
inline string output_filename(const string &prefix)
{
    return prefix + ".out";
}

// Checks if a certain test was solved correcly and returns the result as a message
string CheckTest(const string &prefix, int test_number)
{
    ifstream user_fin(output_filename(prefix));
    ifstream correct_fin(output_filename(prefix, test_number));

    if (!user_fin.is_open()) {
        throw runtime_error("Could not open user output file");
    }
    if (!correct_fin.is_open()) {
        throw runtime_error("Could not open correct output file");
    }

    // Compare each line
    int line_number = 0;
    while (!user_fin.eof() && !correct_fin.eof()) {
        string user_line;
        getline(user_fin, user_line);

        string correct_line;
        getline(correct_fin, correct_line);

        ++line_number;
        if (user_line != correct_line) {
            stringstream message_stream;
            message_stream << "wrong\n";
            message_stream << "line #" << line_number << "\n";
            message_stream << "user line:\t" << user_line << "\n";
            message_stream << "correct line:\t" << correct_line << "\n";
            return message_stream.str();
        }
    }

    if (user_fin.eof() && !correct_fin.eof()) {
        return "wrong: user output ended too early\n";
    }

    if (!user_fin.eof() && correct_fin.eof()) {
        return "wrong: user ouput ended too late\n";
    }

    return "CORRECT\n";
}

// Copies the input of a certain test into the user input file
void CopyTest(const string &prefix, int test_number)
{
    ifstream fin(input_filename(prefix, test_number));
    ofstream fout(input_filename(prefix));

    if (!fin.is_open()) {
        throw runtime_error("Could not open test input file");
    }
    if (!fout.is_open()) {
        throw runtime_error("Could not open user input file");
    }

    string line;
    while (getline(fin, line)) {
        fout << line << "\n";
    }
}

// Execute a certain test and return a message describing its result
string PerformTest(const string &test_file_prefix, int test_number)
{
    // Copy the test data into the user input file
    CopyTest(test_file_prefix, test_number);

    // Start the program
    RunAndWait(kExecutableName);

    // Get test result as a message
    auto result = CheckTest(test_file_prefix, test_number);

    stringstream message_stream;
    message_stream << "Test #" << test_number << ": " << result;
    return message_stream.str();
}

int main(int argc, char* argv[])
{
    if (argc != 5) {
        cout << "The program expects these arguments:\n"
                " - [path from this directory to destination]\n"
                " - [source name]\n"
                " - [test-files name]\n"
                " - [number of tests]\n";
        return 0;
    }

    // Navigate to target directory
    ChangeDirectory(argv[1]);

    // Try to compile the source
    try {
        // argv[2] contains the source name
        CompileSource(argv[2]);
    } catch (const invalid_argument &ia) {
        cerr << ia.what() << "\n";
        return 0;
    }

    string test_file_prefix = argv[3];
    int tests;

    // Parse numeric input
    try {
        tests = to_int(argv[4]);
    } catch (const  invalid_argument &ia) {
        cerr << ia.what() << "\n";
        return 0;
    }

    try {
        for (int i = 1; i <= tests; ++i) {
            auto message = PerformTest(test_file_prefix, i);
            cout << message << "\n";
        }
    } catch(const exception &e) {
        cerr << "An error occured during testing\n";
        cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
