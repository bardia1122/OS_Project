struct stat;
struct proc_info;
struct child_processes;
struct report;
struct _internal_report_list;
struct report_traps;
#define NPROC 64
enum procstate
{
    UNUSED,
    USED,
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE
};
struct cpu_usage
{
    uint sum_of_ticks;
    uint start_tick;
    uint quota;
    uint has_deadline;
    uint deadline;
};

struct proc_info
{
    char name[16];
    int pid;
    int ppid;
    enum procstate state;
    struct cpu_usage usage;
};

struct top
{
    int count;
    struct proc_info processes[NPROC];
};
struct child_processes
{
    int count;
    struct proc_info processes[NPROC];
};

#define MAX_REPORT_BUFFER_SIZE 10

struct report
{
    char pname[16];
    int pid;
    uint64 scause;
    uint64 spec;
    uint64 stval;
};

struct report_traps
{
    struct report reports[MAX_REPORT_BUFFER_SIZE];
    int count;
};
struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int *);
int pipe(int *);
int write(int, const void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(const char *, char **);
int open(const char *, int);
int mknod(const char *, short, short);
int unlink(const char *);
int fstat(int fd, struct stat *);
int link(const char *, const char *);
int mkdir(const char *);
int chdir(const char *);
int dup(int);
int getpid(void);
char *sbrk(int);
int sleep(int);
int uptime(void);
int chp(struct child_processes *);
int report(struct report_traps *);
int cthread(void *(*function)(void *), void *arg, void *stack);
int jthread(int id);
int sthread(void);
int cpu_usage(struct proc_info *po);
int top(struct top *t);
int set_quota(int pid, int quota);
int fork2(int deadline);
// ulib.c
int stat(const char *, struct stat *);
char *strcpy(char *, const char *);
void *memmove(void *, const void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
void fprintf(int, const char *, ...) __attribute__((format(printf, 2, 3)));
void printf(const char *, ...) __attribute__((format(printf, 1, 2)));
char *gets(char *, int max);
uint strlen(const char *);
void *memset(void *, int, uint);
int atoi(const char *);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);

// umalloc.c
void *malloc(uint);
void free(void *);
