/*
 * This file is part of GtkEveMon.
 *
 * GtkEveMon is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with GtkEveMon. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PIPED_EXEC_HEADER
#define PIPED_EXEC_HEADER

#include <vector>
#include <string>

class PipedExec
{
  private:
    pid_t child_pid;
    int p2c_pipe[2];
    int c2p_pipe[2];
    int child_ret;

    //void set_nonblock_flag (int fd, bool value = true);

  public:
    PipedExec (void);
    ~PipedExec (void);
    void exec (std::vector<std::string> const& cmd);

    bool output_available (void);
    std::string fetch_output (void);

    void close_sender (void);
    void send_data (std::string const& data);

    bool has_exited (void);
    int get_return_val (void);
    int waitpid (void);

    void terminate (void);
};

#endif /* PIPED_EXEC_HEADER */