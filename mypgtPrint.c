#include "types.h"
#include "user.h"
#include "stat.h"

int arrGlobal[10000];

int main(int argc, char *argv[])
{
    if (pgtPrint())
        printf(1, "mypgtPrint: Could not print the page table\n");
    exit();
}