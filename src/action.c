#include "action.h"

#include <unistd.h>

void start_terminal(void)
{
    if (fork() == 0) {
        execl("/usr/bin/xterm", "/usr/bin/xterm", NULL);
    }
}

void do_action(int action)
{
    switch (action) {
    case ACTION_START_TERMINAL:
        start_terminal();
        break;
    }
}
