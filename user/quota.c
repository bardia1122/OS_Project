#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void test(int id, long long number)
{
    long long counter = 0;
    for (long long i = 0; i < number; i++)
    {
        counter++;
    }
    printf("%d %lld\n", id, counter);
}

int max_finder(struct top *t)
{
    int index = 0;

    for (int i = 0; i < t->count; i++)
    {
        if (t->processes[i].usage.sum_of_ticks > t->processes[index].usage.sum_of_ticks)
        {
            index = i;
        }
    }
    return index;
}

int main()
{
    int pid = fork();
    if (pid == 0)
    {
        test(pid, 1000000000);
        exit(0);
    }
    set_quota(pid, 100);
    int pid1 = fork();
    if (pid1 == 0)
    {
        test(pid1, 1000000000);
        exit(0);
    }
    set_quota(pid1, 100);
    int pid2 = fork();
    if (pid2 == 0)
    {
        test(pid2, 1000000000);
        exit(0);
    }
    set_quota(pid2, 100);
    int pid3 = fork();
    if (pid3 == 0)
    {
        test(pid3, 1000000000);
        exit(0);
    }
    set_quota(pid3, 10);
    sleep(10);
    struct top t;
    top(&t);
    printf("number of processes: %d\n", t.count);
    printf("PID\t\tPPID\t\tSTATE\t\tNAME\t\tSTART\t\tUSAGE\n");
    int index = 0;

    for (int i = 0; i < t.count; i++)
    {
        index = max_finder(&t);
        printf("%d\t\t", t.processes[index].pid);
        printf("%d\t\t", t.processes[index].ppid);
        switch (t.processes[index].state)
        {
        case UNUSED:
            printf("UNUSED\t\t");
            break;
        case USED:
            printf("USED\t\t");
            break;
        case SLEEPING:
            printf("SLEEPING\t\t");
            break;
        case RUNNABLE:
            printf("RUNNABLE\t\t");
            break;
        case RUNNING:
            printf("RUNNING\t\t");
            break;
        default:
            printf("ZOMBIE\t\t");
            break;
        }
        printf("%s\t\t", t.processes[index].name);
        printf("%d\t\t", t.processes[index].usage.start_tick);
        printf("%d\n", t.processes[index].usage.sum_of_ticks);
        t.processes[index].usage.sum_of_ticks = 0;
    }
    return 0;
}