#include "types.h"
#include "user.h"
#include "stat.h"

int arrGlobal[10000];

int main(int argc, char *argv[])
{
    // for (int i = 0; i < 10000; ++i)
    //     arrGlobal[i] = i;
    arrGlobal[3000] = 100;
    // int arrLocal[10000];
    // arrLocal[0] = 0;
    // printf(1, "Array: %d\n", arrLocal[0]);
    if (pgtPrint())
        printf(1, "mypgtPrint: Could not print the page table\n");
    exit();
}