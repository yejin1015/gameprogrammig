#include <stdio.h>
#include <stdlib.h>
#include <conio.h>

int main(void) {
    int i, j;

    for (j = 1; j <= 9; j++) {
        system("cls");
        for (i = 1; i <= 9; i++)
            printf("%d * %d = %d\n", j, i, j * i);
        printf("\n아무 키나 누르시오...");
        getch();
    }

    return 0;
}
