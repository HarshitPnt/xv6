
#include "types.h"
#include "user.h"
#include "stat.h"

#define N 3000 // global array size - change to see effect. Try 3000, 5000, 10000
int glob[N];
int main(int argc, char *argv[])
{
    glob[0] = 0;
    printf(1, "Parent Page Table:\n");
    pgtPrint();
    int ret = fork();
    if (ret < 0)
    {
        printf(2, "Fork failed\n");
        exit();
    }
    else if (ret == 0)
    {
        printf(1, "Child Page Table:\n");
        pgtPrint();
        glob[0] = 2; // initialize with any integer value
        printf(1, "global addr from user space: %x\n", glob);
        for (int i = 1; i < N; i++)
        {
            glob[i] = glob[i - 1];
            if (i % 1000 == 0)
            {
                printf(1, "Child\n");
                pgtPrint();
            }
        }
        printf(1, "Printing final child page table:\n");
        pgtPrint();
        printf(1, "Value: %d\n", glob[N - 1]);

        exit();
    }
    else
    {
        wait();

        printf(1, "Parent Table\n");
        glob[0] = 2; // initialize with any integer value
        printf(1, "global addr from user space: %x\n", glob);
        for (int i = 1; i < N; i++)
        {
            glob[i] = glob[i - 1];
            if (i % 1000 == 0)
            {
                printf(1, "Child\n");
                pgtPrint();
            }
        }
        printf(1, "Printing final parent page table:\n");
        pgtPrint();
        printf(1, "Value: %d\n", glob[N - 1]);
    }
    exit();
}
