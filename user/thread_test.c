#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

volatile int a = 0, b = 0, c = 0;

void *my_thread(void *arg)
{
    int *number = (int *)arg;
    for (int i = 0; i < 1000; ++i)
    {
        (*number)++;
    }

    if (number == &a)
    {
        printf("thread a: %d\n", *number);
    }
    else if (number == &b)
    {
        printf("thread b: %d\n", *number);
    }
    else if (number == &c)
    {
        printf("thread c: %d\n", *number);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    void *stack = malloc(1024);
    int ta = cthread(my_thread, (void *)&a, stack);
    void *stack1 = malloc(1024);
    int tb = cthread(my_thread, (void *)&b, stack1);
    void *stack2 = malloc(1024);
    int tc = cthread(my_thread, (void *)&c, stack2);
    printf("\n%d %d %d\n", ta, tb, tc);
    printf("done creating threads\n");
    jthread(ta);
    jthread(tb);
    jthread(tc);

    printf("done joining\n");

    exit(0);
}
