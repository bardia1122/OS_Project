#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define MAX_REPORT_BUFFER_SIZE 10

struct report
{
    char pname[16]; // Process Name
    int pid;        // Process ID
    uint64 scause;  // Supervisor Trap Cause
    uint64 sepc;    // Supervisor Exception Program Counter
    uint64 stval;   // Supervisor Trap Value
};

struct report_traps
{
    struct report reports[MAX_REPORT_BUFFER_SIZE];
    int count;
};

int main()
{
    if (fork() == 0)
    {
        int *p = 0;
        *p = 1;
    }
    if (fork() == 0)
    {
        int *p = 0;
        *p = 1;
    }

    sleep(2);
    struct report_traps trap;
    report(&trap);
    printf("number of exceptions: %d\n", trap.count);
    printf("PID\tPNAME\t\tacause\tsepc\tstval\n");
    for (int i = 0; i < trap.count; i++)
    {
        printf("%d\t%s\t%ld\t%ld\t%ld\n",
               trap.reports[i].pid,
               trap.reports[i].pname,
               trap.reports[i].scause,
               trap.reports[i].sepc,
               trap.reports[i].stval);
    }
}