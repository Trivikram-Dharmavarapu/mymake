#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <set>
#include <fcntl.h>
#include <regex>
#include <string.h>

using namespace std;

struct Target
{
    string name;
    vector<string> prerequisites;
};

struct Rule
{
    string target;
    vector<string> prerequisites;
    vector<string> commands;
};

struct GenericRule
{
    string target;
    vector<string> ext;
    vector<string> prerequisites;
    vector<string> commands;
};

string fileName = "mymake3.mk";
string target = "";
bool buildRulesDatabase = false;
bool continueOnError = false;
bool printDebugInfo = false;
bool blockSIGINT = false;
int timeoutSeconds = -1;
bool timeoutFlag = false;
vector<Target> targets;
vector<Rule> rules;
unordered_map<string, string> variables;
set<string> visited;
vector<pid_t> childProcesses;
vector<pid_t> childProcesses2;
vector<GenericRule> genericRules;

void printGenericRules()
{
    for (GenericRule rule : genericRules)
    {
        cout << "Target: " << rule.target << endl;

        cout << "Extensions:";
        for (const string &extension : rule.ext)
        {
            cout << " " << extension;
        }
        cout << endl;

        cout << "Prerequisites:";
        for (const string &prereq : rule.prerequisites)
        {
            cout << " " << prereq;
        }
        cout << endl;

        cout << "Commands:";
        for (const string &command : rule.commands)
        {
            cout << " " << command;
        }
        cout << endl;
    }
}

void printVariables()
{
    for (const auto &entry : variables)
    {
        cout << entry.first << " = " << entry.second << endl;
    }
}

void printTargets()
{
    cout << "Targets:" << endl;
    for (const Target &target : targets)
    {
        cout << "Target: " << target.name << endl;
        cout << "Prerequisites:" << endl;
        for (const string &prerequisite : target.prerequisites)
        {
            cout << " " << prerequisite << endl;
        }
        cout << endl;
    }
}

void printRules()
{

    if (rules.empty())
    {
        cout << "No rules found." << endl;
    }
    else
    {
        for (const Rule &rule : rules)
        {
            cout << rule.target << " :";

            for (const string &prerequisite : rule.prerequisites)
            {
                cout << " " << prerequisite;
            }
            cout << endl;
            for (const string &command : rule.commands)
            {
                cout << '\t' << command << endl;
            }
        }
    }
}

void printdebugInfoLine(string target, string line, int level)
{
    for (int i = 0; i < level; i++)
    {
        cout << '\t';
    }
    cout << "[" << target << "] " << line << endl;
}

void printRuleForTarget(const Rule &rule, int level)
{
    printdebugInfoLine(rule.target, "Target for " + rule.target, level);
    if (rule.prerequisites.size() > 0)
    {
        string prereq = "Prerequisites:";
        for (const string &prerequisite : rule.prerequisites)
        {
            prereq = prereq + " " + prerequisite;
        }
        printdebugInfoLine(rule.target, prereq, level);
    }

    // cout << "Commands:" << endl;
    // for (const string &command : rule.commands)
    // {
    //     cout << command << endl;
    // }

    cout << endl;
}

void printCommand(const vector<char *> &cmdArgs)
{
    for (char *arg : cmdArgs)
    {
        if (arg != nullptr)
        {
            while (*arg != '\0')
            {
                cout << *arg;
                arg++;
            }
            cout << " ";
        }
    }
    cout << endl;
    cout.flush();
}

void printDebugStats()
{
    cout << "Makefile: " << fileName << endl;
    cout << "Target: " << target << endl;
    cout << "Build Rules Database: " << (buildRulesDatabase ? "Yes" : "No") << endl;
    cout << "Continue on Error: " << (continueOnError ? "Yes" : "No") << endl;
    cout << "Print Debug Info: " << (printDebugInfo ? "Yes" : "No") << endl;
    cout << "Block SIGINT: " << (blockSIGINT ? "Yes" : "No") << endl;
    cout << "Timeout Seconds: " << (timeoutSeconds == -1 ? "None" : to_string(timeoutSeconds)) << endl
         << endl;
}

bool validate(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-f") == 0)
        {
            if (i + 1 < argc)
            {
                fileName = argv[i + 1];
                ++i;
            }
            else
            {
                cerr << "Error: -f option requires a makefile name." << endl;
                return false;
            }
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            buildRulesDatabase = true;
        }
        else if (strcmp(argv[i], "-k") == 0)
        {
            continueOnError = true;
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            printDebugInfo = true;
        }
        else if (strcmp(argv[i], "-i") == 0)
        {
            blockSIGINT = true;
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (i + 1 < argc)
            {
                timeoutSeconds = atoi(argv[i + 1]);
                ++i;
            }
            else
            {
                cerr << "Error: -t option requires a timeout value." << endl;
                return false;
            }
        }
        else if (argv[i][0] != '-')
        {
            if (target.empty())
            {
                target = argv[i];
            }
            else
            {
                cerr << "Error: Multiple targets specified." << endl;
                return false;
            }
        }
        else
        {
            cerr << "Error: Unknown option " << argv[i] << endl;
            return false;
        }
    }

    return true;
}

string removeLeadingTrailingSpaces(const string &input)
{
    if (input.empty())
    {
        return "";
    }
    size_t startPos = 0;
    while (startPos < input.length() && isspace(input[startPos]))
    {
        startPos++;
    }
    size_t endPos = input.length();
    while (endPos > startPos && isspace(input[endPos - 1]))
    {
        endPos--;
    }

    return input.substr(startPos, endPos - startPos);
}

void readMakefile(string fileName)
{
    ifstream makefile(fileName);
    string line;

    Rule currentRule;

    while (getline(makefile, line))
    {
        line = removeLeadingTrailingSpaces(line);
        if (line.empty() || (line.size() > 0 && line[0] == '#'))
        {
            continue;
        }
        size_t pos = line.find('=');

        if (pos != string::npos)
        {
            string variableName = line.substr(0, pos);
            string variableValue = line.substr(pos + 1);
            variables[variableName] = variableValue;
        }
        else if (line.find(":") != string::npos)
        {
            if (!currentRule.target.empty())
            {
                rules.push_back(currentRule);
            }
            currentRule = Rule();
            currentRule.target = line.substr(0, line.find(":"));
            string prerequisite;
            istringstream iss(line.substr(line.find(":") + 1));
            while (iss >> prerequisite)
            {
                currentRule.prerequisites.push_back(prerequisite);
            }
        }
        else
        {
            if (!line.empty())
            {
                line = line.substr(line.find_first_not_of(" \t"));
                line = line.erase(line.find_last_not_of(" \t") + 1);
                currentRule.commands.push_back(line);
            }
        }
    }
    if (!currentRule.target.empty())
    {
        rules.push_back(currentRule);
    }

    makefile.close();

    for (const Rule &rule : rules)
    {
        Target target;
        target.name = rule.target;
        target.prerequisites = rule.prerequisites;
        targets.push_back(target);
    }
}

vector<char *> buildCommandArgs(const string &command, const string &target, const string &prerequisite)
{
    vector<char *> cmdArgs;
    istringstream iss(command);
    string arg;

    while (iss >> arg)
    {
        size_t pos;
        string modifiedArg = arg;
        if ((pos = modifiedArg.find('$')) != string::npos)
        {
            size_t start = pos + 1;
            size_t end = modifiedArg.find(')', start);
            if (end == string::npos)
            {
                end = modifiedArg.size();
            }

            string varName;
            if (modifiedArg[start] == '(')
            {
                varName = modifiedArg.substr(start + 1, end - start - 1);
            }
            else
            {
                varName = modifiedArg.substr(start, end - start);
            }

            if (varName == "@")
            {
                modifiedArg.replace(pos, end - pos + 1, target);
            }
            else if (varName == "<")
            {
                modifiedArg.replace(pos, end - pos + 1, prerequisite);
            }
            else if (variables.find(varName) != variables.end())
            {
                modifiedArg.replace(pos, end - pos + 1, variables[varName]);
            }
        }

        if (!modifiedArg.empty())
        {
            char *argChar = new char[modifiedArg.size() + 1];
            strcpy(argChar, modifiedArg.c_str());
            cmdArgs.push_back(argChar);
        }
    }

    cmdArgs.push_back(nullptr);

    return cmdArgs;
}

char *searchCommand(const string &command)
{
    if (command.front() == '/')
    {
        char *result = new char[command.size() + 1];
        strcpy(result, command.c_str());
        return result;
    }
    else
    {
        char *myPathEnv = getenv("MYPATH");
        if (myPathEnv != nullptr)
        {
            string myPath = myPathEnv;
            size_t start = 0;
            size_t end = myPath.find(':');

            while (end != string::npos)
            {
                string path = myPath.substr(start, end - start);
                string fullCommandPath = path + '/' + command;
                if (access(fullCommandPath.c_str(), F_OK) == 0)
                {
                    char *result = new char[fullCommandPath.size() + 1];
                    strcpy(result, fullCommandPath.c_str());
                    return result;
                }
                start = end + 1;
                end = myPath.find(':', start);
            }
        }
        else
        {
            cerr << "Error: MYPATH environment variable is not set!" << endl;
        }
        char *result = new char[command.size() + 1];
        strcpy(result, command.c_str());
        return result;
    }
}

string searchFile(const string &filename)
{
    if (filename[0] == '/')
    {
        return filename;
    }
    else
    {
        const char *myPathEnv = getenv("MYPATH");
        if (myPathEnv != nullptr)
        {
            std::string myPath = myPathEnv;
            size_t start = 0;
            size_t end = myPath.find(':');

            while (end != string::npos)
            {
                string path = myPath.substr(start, end - start);
                string fullPath = path + '/' + filename;
                if (access(fullPath.c_str(), F_OK) == 0)
                {
                    return fullPath;
                }
                start = end + 1;
                end = myPath.find(':', start);
            }
        }
        return filename;
    }
}

void terminateChildProcesses(vector<pid_t> childProcesses)
{
    for (pid_t childPid : childProcesses)
    {
        if (printDebugInfo)
        {
            printdebugInfoLine("mymake", "Killing child PID: " + to_string(childPid), 0);
        }
        kill(childPid, SIGKILL);
    }
}

void terminateProcess(int parentId)
{
    if (printDebugInfo)
    {
        printdebugInfoLine("mymake", "Parent ID: " + to_string(parentId), 0);
        printdebugInfoLine("mymake", "Child PIDs: " + to_string(childProcesses.size()), 0);
    }
    terminateChildProcesses(childProcesses);
    terminateChildProcesses(childProcesses2);
    kill(parentId, SIGTERM);
}

vector<string> splitCommands(const string &command)
{
    vector<string> commands;
    istringstream iss(command);
    string singleCommand;

    while (getline(iss, singleCommand, ';'))
    {
        // Remove leading and trailing whitespaces
        singleCommand.erase(0, singleCommand.find_first_not_of(" \t"));
        singleCommand.erase(singleCommand.find_last_not_of(" \t") + 1);

        if (!singleCommand.empty())
        {
            commands.push_back(singleCommand);
        }
    }

    return commands;
}
vector<string> splitPipeCommands(const string &command)
{
    string commandCopy = command;
    vector<string> commands;
    size_t pos = 0;

    while ((pos = commandCopy.find('|')) != string::npos)
    {
        commands.push_back(commandCopy.substr(0, pos));
        commandCopy.erase(0, pos + 1);
    }

    if (!commandCopy.empty())
    {
        commands.push_back(commandCopy);
    }
    return commands;
}

void handleChildSigInt(int signo)
{
    printdebugInfoLine("mymake", "Interrupt", 0);
    if (printDebugInfo)
    {
        printdebugInfoLine(target, "Received SIGINT signal.", 0);
    }
    int parentId = getpid();
    terminateProcess(parentId);
}
void handleSigInt(int signo)
{
    printdebugInfoLine("mymake", "Interrupt", 0);
    if (printDebugInfo)
    {
        printdebugInfoLine(target, "Received SIGINT signal.", 0);
    }
    int parentId = getpid();
    terminateProcess(parentId);
}

void handleChildTimeout(int signo){
    int parentId = getpid();
    terminateProcess(parentId);
    terminateChildProcesses(childProcesses2);

}

void handleTimeout(int signo)
{
    printdebugInfoLine("mymake", "Timeout Reached", 0);
    timeoutFlag = true;
    handleSigInt(SIGINT);
}

void executeCommand(const string &command, string target, string prerequisite)
{

    pid_t childPid = fork();

    if (childPid == -1)
    {
        perror("Error forking");
        return;
    }

    if (childPid == 0)
    {
        signal(SIGINT, handleChildSigInt);
        signal(SIGALRM, handleChildTimeout);
        vector<string> commandsSplit = splitCommands(command);
        for (string singleCommand : commandsSplit)
        {
            vector<string> commands = splitPipeCommands(singleCommand);
            ;
            int input = -1;
            int output = -1;
            int pipefd[2];
            int lastOutput = -1;
            for (int i = 0; i < static_cast<int>(commands.size()); ++i)
            {
                size_t inputRedirectionPos = commands[i].find('<');
                if (inputRedirectionPos != string::npos && commands[i].find("$<") == string::npos)
                {
                    string inputFile = commands[i].substr(inputRedirectionPos + 1);
                    commands[i] = commands[i].substr(0, inputRedirectionPos);
                    inputFile = regex_replace(inputFile, regex("^\\s+|\\s+$"), "");
                    input = open(inputFile.c_str(), O_RDONLY);
                    if (input == -1)
                    {
                        perror("Error opening input file");
                        return;
                    }
                }

                size_t outputRedirectionPos = commands[i].find('>');
                string outputFile = "";
                if (outputRedirectionPos != string::npos)
                {
                    outputFile = commands[i].substr(outputRedirectionPos + 1);
                    outputFile = regex_replace(outputFile, regex("^\\s+|\\s+$"), "");
                    commands[i] = commands[i].substr(0, outputRedirectionPos);
                    output = open(outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    if (output == -1)
                    {
                        perror("Error opening output file");
                        return;
                    }
                }
                bool changeDir = false;
                if (commands[i][0] == 'c' && commands[i][1] == 'd')
                {
                    changeDir = true;
                }
                vector<char *> cmdArgs = buildCommandArgs(commands[i], target, prerequisite);
                cmdArgs[0] = searchCommand(cmdArgs[0]);
                // cout<<cmdArgs[1]<<endl;
                printCommand(cmdArgs);
                if (changeDir)
                {
                    if (cmdArgs.size() == 3)
                    {
                        if (chdir(cmdArgs[1]) != 0)
                        {
                            perror("Error changing directory");
                            exit(EXIT_FAILURE);
                        }
                    }
                    else
                    {
                        cerr << "Error: 'cd' command requires one argument (directory)." << endl;
                    }
                }
                else
                {
                    if (pipe(pipefd) == -1)
                    {
                        perror("Error creating pipe");
                        return;
                    }
                    pid_t childPid2 = fork();

                    if (childPid2 == -1)
                    {
                        perror("Error forking");
                        return;
                    }

                    if (childPid2 == 0)
                    {
                        signal(SIGINT, handleChildSigInt);
                        signal(SIGALRM, handleChildTimeout);
                        // cout<<"input: "<< input<<endl;
                        // cout<<"pipefd[0]: "<< pipefd[0]<<endl;
                        // cout<<"pipefd[1]: "<< pipefd[1]<<endl;
                        // cout<<"output: "<< output<<endl;
                        if (input != -1)
                        {
                            close(STDIN_FILENO);
                            dup(input);
                            close(input);
                        }
                        if (output == -1 && i < static_cast<int>(commands.size()) - 1)
                        {
                            close(STDOUT_FILENO);
                            dup(pipefd[1]);
                            close(pipefd[1]);
                        }
                        else if (output != -1)
                        {
                            close(STDOUT_FILENO);
                            dup(output);
                            close(output);
                        }
                        close(pipefd[0]);

                        if (execv(cmdArgs[0], cmdArgs.data()) == -1)
                        {
                            perror("Error executing command");
                            exit(EXIT_FAILURE);
                        }
                        exit(EXIT_SUCCESS);
                    }
                    else
                    {
                        childProcesses2.push_back(childPid2);
                        int status;
                        waitpid(childPid2, &status, 0);
                        if (WIFEXITED(status))
                        {
                            int exitStatus = WEXITSTATUS(status);
                            if (exitStatus != 0)
                            {
                                terminateChildProcesses(childProcesses2);
                                exit(EXIT_FAILURE);
                            }
                        }
                        if (i > 0)
                        {
                            close(input);
                        }

                        if (i < static_cast<int>(commands.size()) - 1)
                        {
                            input = pipefd[0];
                        }
                        else
                        {
                            lastOutput = pipefd[0];
                        }
                        close(pipefd[1]);
                    }
                }
            }
            char buffer[4096];
            ssize_t bytesRead = read(lastOutput, buffer, sizeof(buffer));
            // if(output == -1){
            //     output = STDOUT_FILENO;
            // }
            write(output, buffer, bytesRead);

            // Close the last output pipe
            close(lastOutput);
            close(output);
        }
        terminateChildProcesses(childProcesses2);
        exit(EXIT_SUCCESS); // L1 ch
    }
    else
    { // p
        childProcesses.push_back(childPid);

        int status;
        waitpid(childPid, &status, 0);
        if (WIFEXITED(status))
        {
            int exitStatus = WEXITSTATUS(status);
            if (exitStatus != 0)
            {
                if (continueOnError)
                {
                    printdebugInfoLine("mymake", "** Error code: " + to_string(WIFEXITED(status)) + ", Continue", 0);
                }
                else
                {
                    printdebugInfoLine("mymake", "** Error code: " + to_string(WIFEXITED(status)) + ", Terminate", 0);
                    terminateProcess(getpid());
                }
            }
        }
    }
}

vector<string> splitString(const string &input)
{
    vector<string> result;
    string token;

    for (char ch : input)
    {
        if (ch == '.')
        {
            if (token.size() > 0)
                result.push_back(token);
            result.push_back(".");
            token.clear();
        }
        else if (ch == '%')
        {
            if (token.size() > 0)
                result.push_back(token);
            result.push_back("%");
            token.clear();
        }
        else
        {
            token += ch;
        }
    }
    if (token.size() > 0)
        result.push_back(token);
    return result;
}

void processGenericRules()
{
    for (Rule rule : rules)
    {
        // cout<<rule.target<<endl;
        vector<string> tokens = splitString(rule.target);
        GenericRule gr;
        while (tokens.size() > 1)
        {
            // cout<<tokens.size()<<endl;
            if (tokens[0] == ".")
            {
                gr.ext.push_back(tokens[1]);
                tokens.erase(next(tokens.begin()), next(tokens.begin(), 3));
            }
            else if (tokens[0] == "%" && tokens[1] == ".")
            {
                gr.ext.push_back(tokens[2]);
                tokens.erase(next(tokens.begin()), next(tokens.begin(), 4));
            }
            else
            {
                break;
            }
        }
        if (gr.ext.size() > 0)
        {
            gr.target = rule.target;
            gr.prerequisites = rule.prerequisites;
            gr.commands = rule.commands;
            genericRules.push_back(gr);
        }
    }
}

GenericRule matchesTarget(const string &target)
{
    vector<string> tokens = splitString(target);
    for (GenericRule gr : genericRules)
    {
        for (string ext : gr.ext)
        {
            if (tokens[2] == ext)
            {
                return gr;
            }
        }
    }
    GenericRule gr;
    return gr;
}

void executeTarget(const string &target, int level, string parent)
{
    string debugVal = target;
    if (target[0] != '.' && target[0] != '%')
    {
        if (visited.count(target) > 0)
        {
            return;
        }
        visited.insert(target);
    }
    else
    {
        debugVal = parent;
    }
    for (const Rule &rule : rules)
    {
        if (rule.target == target)
        {
            if (printDebugInfo)
            {
                printdebugInfoLine(debugVal, "Making " + target, level);
                printRuleForTarget(rule, level);
            }
            for (const string &prereq : rule.prerequisites)
            {
                if (visited.count(prereq) == 0)
                {
                    executeTarget(prereq, level + 1, target);
                }
            }
            for (const string &command : rule.commands)
            {
                if (printDebugInfo)
                {
                    printdebugInfoLine(debugVal, "Action: " + command, level);
                }
                if (parent.size() > 0)
                    executeCommand(command, rule.target, parent);
                else
                    executeCommand(command, rule.target, target);
            }
            if (printDebugInfo)
            {
                printdebugInfoLine(debugVal, "Done Making " + target, level);
            }
            return;
        }
    }
    if (target.find(".c") != string::npos || target.find(".h") != string::npos)
    {
        return;
    }
    GenericRule gr = matchesTarget(target);
    if (gr.ext.size() > 0)
    {
        executeTarget(gr.target, level + 1, splitString(target)[0] + "." + gr.ext[0]);
        return;
    }
    else
    {
        printdebugInfoLine("mymake", "**Target not found", 0);
        return;
    }
    return;
}


int main(int argc, char *argv[])
{
    if (validate(argc, argv))
    {
        readMakefile(fileName);
        processGenericRules();
        // printGenericRules();
        if (target.empty())
        {
            target = rules[0].target;
        }
        // printTargets();
        // printRules();
        // printVariables();
        if (blockSIGINT)
        {
            sigset_t set;
            sigemptyset(&set);
            sigaddset(&set, SIGINT);

            if (sigprocmask(SIG_BLOCK, &set, nullptr) == -1)
            {
                perror("sigprocmask");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            signal(SIGINT, handleSigInt);
        }
        signal(SIGALRM, handleTimeout);
        if (timeoutSeconds > 0)
        {
            alarm(timeoutSeconds);
        }
        if (printDebugInfo)
        {
            cout << "DebugInfo: " << endl;
            printDebugStats();
        }
        if (!buildRulesDatabase)
        {
            executeTarget(target, 0, "");
        }
        else
        {
            printVariables();
            printRules();
        }
    }

    return 0;
}
