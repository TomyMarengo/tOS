// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <phylo.h>
#include <syscalls.h>
#include <userlib.h>
#include <defs.h>
#include <string.h>

static char* phyloName[MAX_PHYLOSOPHERS] = {"Aristoteles", "Socrates", "Platon", "Pitagoras", "Kant",
                                            "Heraclito", "Sofocles", "TalesDeMileto", "Diogenes",
                                            "Democrito"};

static Phylo phylos[MAX_PHYLOSOPHERS];
static int phyloCount = 0;

static Sem forks[MAX_FORKS];
static Sem semPickForks;

static void phylosopher(int argc, char* argv[]);
static void waitForKey();

static void phyloThink(int phyloIdx);
static void phyloEat(int phyloIdx);
static void phyloSleep(int phyloIdx);

static void addPhylo();
static void removePhylo();
static void addChopstick();
static void removeChopstick();

static void terminatePhylos();
static void terminateForks();

static void printState();

// Rand functions taken from:
// https://stackoverflow.com/questions/4768180/rand-implementation
static unsigned long int nextRand = 1;

static int rand(void) {
    nextRand = nextRand * 1103515245 + 12345;
    return (unsigned int)(nextRand / 65536) % 32768;
}

static inline int getThinkingTime() {
    return THINKING_TIME_MIN + rand() % (THINKING_TIME_MAX - THINKING_TIME_MIN + 1);
}

static inline int getEatingTime() {
    return EATING_TIME_MIN + rand() % (EATING_TIME_MAX - EATING_TIME_MIN + 1);
}

static inline int getSleepingTime() {
    return SLEEPING_TIME_MIN + rand() % (SLEEPING_TIME_MAX - SLEEPING_TIME_MIN + 1);
}

// Initialize a minimun amount of phylosophers.
void startPhylo(int argc, char* argv[]) {

    nextRand = (unsigned int)sys_millis();
    printf("A minimun quantity of phylosophers are beign created, please stand by\n");
    printf("Press %c for quiting, %c for adding a new phylosopher, and %c for removing a phylosopher.\n", QUIT, ADD, REMOVE);
    sleep(500);

    for (int i = 0; i < MAX_PHYLOSOPHERS; ++i) {
        phylos[i].phyloPid = -1;
        forks[i] = -1;
    }

    for (int i = 0; i < MIN_PHYLOSOPHERS; ++i)
        addPhylo();

    if ((semPickForks = sys_openSem("semPickSticks", 1)) == -1) {
        fprint(STDERR, "sys_openSem failed\n");
        terminatePhylos();
        sys_exit();
    }

    waitForKey();
}

static void waitForKey() {

    while (1) {
        char c = getChar();

        if (c < 0 || c == QUIT) {
            terminatePhylos();
            terminateForks();
            sys_exit();
        } else if (c == ADD) {

            if (phyloCount >= MAX_PHYLOSOPHERS)
                printf("\nThere's no more space in the table! Remove a phylosopher!\n");
            else
                addPhylo();

        } else if (c == REMOVE) {

            if (phyloCount <= MIN_PHYLOSOPHERS)
                printf("\nDont be selfish! Invite more phylosophers to the table!\n");
            else
                removePhylo();
        }
    }
}

static void phylosopher(int argc, char* argv[]) {

    int phyloIdx = atoi(argv[0]);

    while (1) {
        phyloThink(phyloIdx);
        if (phyloCount <= phyloIdx)
            break;
        phyloEat(phyloIdx);
        if (phyloCount <= phyloIdx)
            break;
        phyloSleep(phyloIdx);
        if (phyloCount <= phyloIdx)
            break;
    }

    phylos[phyloIdx].phyloState = DEAD;
    phylos[phyloIdx].phyloPid = -1;
}

static void phyloThink(int phyloIdx) {
    phylos[phyloIdx].phyloState = THINKING;
    printState();
    sleep(getThinkingTime());
}

static void phyloEat(int phyloIdx) {
    phylos[phyloIdx].phyloState = WAITINGTOEAT;
    printState();

    int otherSemIdx = (phyloIdx + 1) % phyloCount;
    sys_wait(semPickForks);
    sys_wait(forks[phyloIdx]);    // Left chopstick
    sys_wait(forks[otherSemIdx]); // Right chopstick
    sys_post(semPickForks);

    printState();
    phylos[phyloIdx].phyloState = EATING;
    sleep(getEatingTime());

    sys_post(forks[otherSemIdx]); // Right chopstick
    sys_post(forks[phyloIdx]);    // Left chopstick
}

static void phyloSleep(int phyloIdx) {
    phylos[phyloIdx].phyloState = SLEEPING;
    printState();
    sleep(getSleepingTime());
}

static void addPhylo() {

    if (phylos[phyloCount].phyloPid != -1) {
        fprintf(STDERR, "Please wait a bit before trying to add again.\n");
        return;
    }

    addChopstick();
    phylos[phyloCount].phyloName = phyloName[phyloCount];
    phylos[phyloCount].phyloState = THINKING;

    char idxToString[] = {phyloCount + '0', 0};
    char* auxi[] = {idxToString};
    ProcessCreateInfo phyloContexInfo = {
        .name = phyloName[phyloCount],
        .start = (void*)phylosopher,
        .isForeground = 0,
        .priority = PHYLO_PRIORITY,
        .argc = 1,
        .argv = (const char* const*)auxi,
    };

    phyloCount++;
    phylos[phyloCount - 1].phyloPid = sys_createProcess(-1, -1, -1, &phyloContexInfo);
}

static void removePhylo() {
    removeChopstick();
}

static void addChopstick() {
    if (forks[phyloCount] >= 0)
        return;

    char aux[] = {phyloCount + '0', 0};
    char semName[strlen(FORKS_SEM_NAME) + 2];
    strcpy(semName, FORKS_SEM_NAME);
    strcat(semName, aux);
    forks[phyloCount] = sys_openSem(semName, 1);
    if (forks[phyloCount] < 0) {
        fprint(STDERR, "sys_openSem failed\n");
        terminatePhylos();
        terminateForks();
        sys_exit();
    }
}

static void removeChopstick() {
    phyloCount--;
}

static void terminatePhylos() {
    for (int i = 0; i < phyloCount; ++i)
        sys_kill(phylos[i].phyloPid);
}

static void terminateForks() {
    for (int i = 0; i < MAX_FORKS && forks[i] >= -1; ++i)
        sys_close(forks[i]);

    sys_close(semPickForks);
}

static void printState() {
    print("\n");
    for (int i = 0; i < phyloCount; ++i)
        printf("%c ", phylos[i].phyloState == WAITINGTOEAT ? 'e' : phylos[i].phyloState == EATING ? 'E'
                                                               : phylos[i].phyloState == SLEEPING ? 's'
                                                               : phylos[i].phyloState == THINKING ? 't'
                                                                                                  : 'x');
}