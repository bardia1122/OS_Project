#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define NPROC 64
// enum procstate
// {
//     UNUSED,
//     USED,
//     SLEEPING,
//     RUNNABLE,
//     RUNNING,
//     ZOMBIE
// };
// struct proc_info
// {
//     char name[16];
//     int pid;
//     int ppid;
//     enum procstate state;
// };

// struct child_processes
// {
//     int count;
//     struct proc_info processes[NPROC];
// };

const char *procstate_to_string(enum procstate state)
{
    switch (state)
    {
    case UNUSED:
        return "UNUSED";
    case USED:
        return "USED";
    case SLEEPING:
        return "SLEEPING";
    case RUNNABLE:
        return "RUNNABLE";
    case RUNNING:
        return "RUNNING";
    case ZOMBIE:
        return "ZOMBIE";
    default:
        return "UNKNOWN";
    }
}

int main(int argc, char *argv[])
{

    int pid = fork();
    if (pid == 0)
    {
        int pid2 = fork();
        if (pid2 == 0)
        {
            int pid3 = fork();
            if (pid3 == 3)
            {
                sleep(20);
                exit(0);
            }
            sleep(20);
            exit(0);
        }
        sleep(20);
        exit(0);
    }
    sleep(2);

    struct child_processes cp;

    int status = chp(&cp);
    if (status < 0)
    {
        printf("RIDI");
        exit(0);
    }

    printf("number of childs: %d\n", cp.count);
    printf("PID\tPPID\tSTATE\t\tNAME\n");
    for (int i = 0; i < cp.count; i++)
    {
        printf("%d\t%d\t%s\t%s\n",
               cp.processes[i].pid,
               cp.processes[i].ppid,
               procstate_to_string(cp.processes[i].state),
               cp.processes[i].name);
    }
    return 0;
}
