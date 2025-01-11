#include "action.h"

#include <unistd.h>

void start_terminal(void)
{
    if (fork() == 0) {
        execl("/usr/bin/st", "/usr/bin/st", NULL);
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
