#ifndef UTILITIES_H
#define UTILITIES_H

#include <string>
#include <vector>
#include <termios.h>

class HistoryManager {
public:
    HistoryManager();
    ~HistoryManager();

    std::string readline(const std::string& prompt);
    void add_to_history(const std::string& command);

private:
    std::vector<std::string> history;
    int history_index;
    struct termios orig_termios;

    void enable_raw_mode();
    void disable_raw_mode();
    int read_key();
};

#endif
