#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct trump {
    int order;
    char shape[4];
    int number;
    char face[3];
};

void make_card(struct trump m_card[]);
void shuffle_card(struct trump m_card[]);
void display_card(struct trump m_card[]);

int main(void) {
    struct trump card[52];
    make_card(card);
    shuffle_card(card);
    display_card(card);
    return 0;
}

void make_card(struct trump m_card[]) {
    int i, j;
    char shape[4][4] = {"♤", "◇", "♡", "♧"};
    char face_chars[4][3] = {"A", "J", "Q", "K"};

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 13; j++) {
            m_card[i * 13 + j].order = i;
            strcpy(m_card[i * 13 + j].shape, shape[i]);
            m_card[i * 13 + j].number = j + 1;

            if (m_card[i * 13 + j].number == 1) {
                strcpy(m_card[i * 13 + j].face, "A");
            } else if (m_card[i * 13 + j].number > 10) {
                strcpy(m_card[i * 13 + j].face, face_chars[m_card[i * 13 + j].number - 10]);
            } else {
                strcpy(m_card[i * 13 + j].face, "");
            }
        }
    }
}

void shuffle_card(struct trump m_card[]) {
    int i, rnd;
    struct trump temp;
    srand(time(NULL));

    for (i = 0; i < 52; i++) {
        rnd = rand() % 52;
        temp = m_card[rnd];
        m_card[rnd] = m_card[i];
        m_card[i] = temp;
    }
}

void display_card(struct trump m_card[]) {
    int i;
    for (i = 0; i < 52; i++) {
        printf("%s", m_card[i].shape);
        if (m_card[i].number == 1 || m_card[i].number > 10) {
            printf("%-2s  ", m_card[i].face);
        } else {
            printf("%-2d  ", m_card[i].number);
        }
        if ((i + 1) % 13 == 0) {
            printf("\n");
        }
    }
}