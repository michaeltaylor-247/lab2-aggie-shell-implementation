#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <cstdlib>

#include <vector>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define WHITE "\033[1;37m"
#define NC "\033[0m"

using namespace std;

int main()
{
    vector<pid_t> bg;
    for (;;)
    {
        // process backgroud josb (reap if done)
        for (auto it = bg.begin(); it != bg.end();)
        {
            int status;
            pid_t r = waitpid(*it, &status, WNOHANG);
            if (r == 0)
                ++it;
            else
                it = bg.erase(it);
        }

        // need date/time, username, and absolute path to current dir
        // Time
        time_t tEpoch;
        time(&tEpoch);
        char *time = ctime(&tEpoch);

        // User
        char *user = getenv("USER");
        if (user == NULL)
        {
            cerr << "getenv(\"USER\") error" << endl;
        }

        // Current Working Directory
        char dirBuf[256];
        if (getcwd(dirBuf, 256) == NULL)
        {
            cerr << "getcwd() error" << endl;
        }

        // print shell prompt
        cout << BLUE << dirBuf << " " << WHITE << time;
        cout << GREEN << user << " $" << NC << " ";

        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit")
        { // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl
                 << "Goodbye" << NC << endl;
            break;
        }

        if (input.empty())
            continue;

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError())
        { // continue to next prompt if input had an error
            continue;
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        /*
        for (auto cmd : tknr.commands)
        {
            for (auto str : cmd->args)
            {
                cerr << "|" << str << "| ";
            }
            if (cmd->hasInput())
            {
                cerr << "in< " << cmd->in_file << " ";
            }
            if (cmd->hasOutput())
            {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }
        */

        auto cmd = tknr.commands.at(0);
        if (cmd->args.size() > 0)
        {
            // change dir
            if (cmd->args[0] == "cd")
            {
                string prevDir;
                char buf[256];
                getcwd(buf, 256);
                std::string currDir = buf;
                std::string target = (cmd->args.size() > 1 ? cmd->args[1] : getenv("HOME"));
                if (target == "-")
                {
                    if (!prevDir.empty())
                        chdir(prevDir.c_str());
                }
                else
                {
                    if (chdir(target.c_str()) != 0)
                        perror("chdir");
                }
                prevDir = currDir;
                continue;
            }
            else if (cmd->args[0] == "pwd")
            {
                char buf[256];
                getcwd(buf, 256);
                cout << buf << endl;
                continue;
            }
        }

        if (tknr.commands.size() > 1)
        {
            int n = tknr.commands.size();
            std::vector<int> pipes(2 * (n - 1));
            for (int i = 0; i < n - 1; ++i)
                pipe(&pipes[2 * i]);

            for (int i = 0; i < n; ++i)
            {
                pid_t pid = fork();
                if (pid == 0)
                { 
                    if (i > 0)
                        dup2(pipes[2 * (i - 1)], STDIN_FILENO);
                    if (i < n - 1)
                        dup2(pipes[2 * i + 1], STDOUT_FILENO);

                    for (int fd : pipes)
                        close(fd);

                    auto cmd = tknr.commands.at(i);
                    std::vector<char *> argv;
                    for (auto &arg : cmd->args)
                        argv.push_back((char *)arg.c_str());
                    argv.push_back(nullptr);

                    if (cmd->hasInput())
                    {
                        int fd = open(cmd->in_file.c_str(), O_RDONLY);
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                    }
                    if (cmd->hasOutput())
                    {
                        int fd = open(cmd->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                    }

                    execvp(argv[0], argv.data());
                    perror("execvp");
                    _exit(1);
                }
            }

            for (int fd : pipes)
                close(fd);
            for (int i = 0; i < n; ++i)
                wait(nullptr);
            cout << endl;
            continue;
        }

        // fork to create child
        pid_t pid = fork();
        if (pid < 0)
        { // error check
            perror("fork");
            exit(2);
        }

        if (pid == 0)
        { // if child, exec to run command
            auto cmd = tknr.commands.at(0);
            std::vector<char *> argv;
            for (auto &arg : cmd->args)
                argv.push_back((char *)arg.c_str());

            argv.push_back(nullptr);

            if (cmd->hasInput())
            {
                int fd = open(cmd->in_file.c_str(), O_RDONLY);
                if (fd < 0)
                {
                    perror("open <");
                    _exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (cmd->hasOutput())
            {
                int fd = open(cmd->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd < 0)
                {
                    perror("open >");
                    _exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (execvp(argv[0], argv.data()) < 0)
            {
                perror("execvp");
                _exit(2);
            }
        }
        else
        { // if parent, wait for child to finish
            if (cmd->isBackground())
            {
                bg.push_back(pid);
            }
            else
            {
                int status = 0;
                waitpid(pid, &status, 0);
                if (status > 1)
                    exit(status);
            }
        }
        cout << endl;
    }
}
