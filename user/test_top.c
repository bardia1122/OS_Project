#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
void test(long long number)
{
    long long counter = 0;
    for (long long i = 0; i < number; i++)
    {
        counter++;
    }
    printf("%lld\n", counter);
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

    test(1000000000);
    struct top t;
    top(&t);
    printf("number of processes: %d\n", t.count);
    printf("PID\t\tPPID\t\tSTATE\t\tNAME\t\tSTART\t\tUSAGE\n");
    int index = 0;

    for (int i = 0; i < t.count; i++)
    {
        index = max_finder(&t);
        printf("%d\t\t", t.processes[i].pid);
        printf("%d\t\t", t.processes[i].ppid);
        switch (t.processes[i].state)
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
        printf("%s\t\t", t.processes[i].name);
        printf("%d\t\t", t.processes[i].usage.start_tick);
        printf("%d\n", t.processes[i].usage.sum_of_ticks);
        t.processes[index].usage.sum_of_ticks = 0;
    }
    return 0;
}