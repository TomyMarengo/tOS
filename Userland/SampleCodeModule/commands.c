#include <commands.h>
#include <phylo.h>
#include <processes.h>
#include <string.h>
#include <syscalls.h>
#include <testUtil.h>
#include <userlib.h>

static Command validCommands[] = {
    {runHelp, "help", "Displays a list of all available commands."},
    {runClear, "clear", "Clears the window."},
    {runEcho, "echo", "Prints the parameters passed to it to standard output."},
    {runTime, "time", "Displays the system's date and time."},
    {runMem, "mem", "Displays information about the system's memory."},
    {runPs, "ps", "Displays a list of all the currently running processes with their properties."},
    {runLoop, "loop", "Creates a process that prints it's PID once every 3 seconds."},
    {runKill, "kill", "Kills the process with the parameter-specified PID."},
    {runPriority, "nice", "Changes the priority of the process with the parameter-specified PID."},
    {runBlock, "block", "Blocks the process with the given PID."},
    {runUnblock, "unblock", "Unblocks the process with the given PID."},
    {runSem, "sem", "Displays a list of all the currently active semaphores with their properties."},
    {runCat, "cat", "Creates a process that prints the standard input onto the standard output."},
    {runWc, "wc",
     "Creates a process that counts the newlines coming in from standard input and prints that number to standard output."},
    {runFilter, "filter",
     "Creates a process that filters the vowels received from standard input and prints them to standard output."},
    {runPipe, "pipe", "Displays a list of all the currently active pipes with their properties."},
    {runTestMM, "testmm", "Runs a test for memory manager."},
    {runTestSync, "testsync", "Runs a synchronization test with multiple processes with semaphores."},
    {runTestProcesses, "testprocesses", "Runs a test for processes."},
    {runTestPrio, "testprio", "Runs a test on process priorities."},
    {runPhylo, "phylo", "Runs the philosopher, add one philosopher with \"a\", remove one philosopher with \"r\"."},
};

const Command *
getCommandByName(const char *name) {
    for (int i = 0; i < (sizeof(validCommands) / sizeof(validCommands[0])); i++) {
        if (!strcmp(name, validCommands[i].name)) {
            return &validCommands[i];
        }
    }
    return NULL;
}

int
runHelp(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    fprint(stdout, "The available commands are:");

    for (int i = 0; i < (sizeof(validCommands) / sizeof(validCommands[0])); i++)
        fprintf(stdout, "\n\t '%s' \t - %s", validCommands[i].name, validCommands[i].description);

    return 1;
}

int
runClear(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    sys_clearScreen();
    return 1;
}

int
runEcho(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    for (int i = 0; i < argc; i++) {
        if (i != 0)
            fputChar(stdout, ' ');
        fprintf(stdout, argv[i]);
    }

    return 1;
}

int
runTime(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    char time[11];
    sys_time(time);
    fprintf(stdout, "Time: %s", time);

    char date[11];
    sys_date(date);
    fprintf(stdout, "\nDate: %s", date);

    fprintf(stdout, "\nMillis since startup: %u", (unsigned int) sys_millis());

    return 1;
}

int
runMem(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    MemoryState memoryState;
    if (sys_memoryState(&memoryState)) {
        fprint(stderr, "Failed to get memory state.");
        return -1;
    }

    fprintf(stdout, "Memory Manager Type: %s\n",
            memoryState.type == LIST    ? "LIST"
            : memoryState.type == BUDDY ? "BUDDY"
                                        : "UNKNOWN");
    fprintf(stdout, "Total memory: %u.\n", memoryState.total);
    fprintf(stdout, "Used: %u (%u%%).\n", memoryState.used, (memoryState.used * 100 / memoryState.total));
    fprintf(stdout, "Available: %u.", memoryState.total - memoryState.used);

    return 1;
}

int
runPs(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessInfo array[MAX_PROCESSES];
    int count = sys_listProcesses(array, MAX_PROCESSES);

    for (int i = 0; i < count; i++) {
        const char *status = array[i].status == READY     ? "READY"
                             : array[i].status == RUNNING ? "RUNNING"
                             : array[i].status == BLOCKED ? "BLOCKED"
                             : array[i].status == KILLED  ? "KILLED"
                                                          : "UNKNOWN";

        fprintf(stdout,
                "PID=%d \t Name=%s \t Status=%s \t Priority=%d \t Foreground=%d \t stackEnd=%x \t stackStart=%x \t RSP=%x\n",
                array[i].pid, array[i].name, status, array[i].priority, array[i].isForeground, array[i].stackEnd,
                array[i].stackStart, array[i].currentRSP);
    }

    return 1;
}

int
runKill(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    if (argc != 1) {
        fprint(stderr, "kill: usage: kill [PID]");
        return 0;
    }

    Pid pidToKill = atoi(argv[0]);

    if (pidToKill == sys_getpid()) {
        fprint(stderr, "Impossible to kill shell.");
        return 0;
    }

    int result = sys_kill(pidToKill);

    ProcessInfo array[MAX_PROCESSES];
    int count = sys_listProcesses(array, MAX_PROCESSES);

    if (result == 0) {
        for (int i = 0; i < count; i++) {
            if (array[i].pid == pidToKill) {
                fprintf(stdout, "Stoped %s.", array[i].name);
                return 1;
            }
        }
    }

    fprintf(stderr, "kill: (%d) - Error: %d.", pidToKill, result);
    return 0;
}

int
runPriority(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    if (argc != 2) {
        fprint(stderr, "nice: usage: nice [PID] [PRIORITY]. Priority must be a number between -10 and 10.");
        return 0;
    }

    Pid pidToChange = atoi(argv[0]);
    Priority newPriority = atoi(argv[1]);

    if (newPriority < PRIORITY_MAX || newPriority > PRIORITY_MIN) {
        fprint(stderr, "Invalid priority value. Must be in the range -10 to 10.");
        return 0;
    }

    int result = sys_priority(pidToChange, newPriority);
    if (result == 0) {
        fprintf(stdout, "Changed priority of %d to %d.\n", pidToChange, newPriority);
    } else {
        fprintf(stdout, "nice: (%d) - Error: %d.", pidToChange, result);
    }

    return 1;
}

int
runBlock(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    if (argc != 1) {
        fprint(stderr, "block: usage: block [PID]");
        return 0;
    }

    Pid pidToBlock = atoi(argv[0]);

    if (pidToBlock == sys_getpid()) {
        fprintf(stderr, "Impossible to block shell.");
        return 0;
    }

    int result = sys_block(pidToBlock);
    if (result == 0) {
        fprintf(stdout, "Blocked process with PID %d.", pidToBlock);
        return 1;
    }

    fprintf(stderr, "block: (%d) - Error: %d.", pidToBlock, result);
    return 0;
}

int
runUnblock(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    if (argc != 1) {
        fprint(STDERR, "unblock: usage: unblock [PID]");
        return 0;
    }

    Pid pidToUnblock = atoi(argv[0]);

    if (pidToUnblock == 0) {
        fprint(stderr, "Impossible to unblock shell.");
        return 0;
    }

    int result = sys_unblock(pidToUnblock);

    if (result == 0) {
        fprintf(stdout, "Unblocked process with PID %d.", pidToUnblock);
        return 1;
    }

    fprintf(stdout, "unblock: (%d) - Error: %d.", pidToUnblock, result);
    return 0;
}

int
runSem(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    SemaphoreInfo array[16];
    int count = sys_listSemaphores(array, 16);
    fprintf(stdout, "Listing %d semaphore%s:", count, count == 1 ? "" : "s");

    for (int i = 0; i < count; i++) {
        fprintf(stdout, "\nName=%s, Value=%d, Processes linked=%d", array[i].name, array[i].value, array[i].linkedProcesses);

        fprintf(stdout, ", Process waiting queue={");
        for (int c = 0; array[i].processesWQ[c] >= 0; c++) {
            if (c != 0) {
                fprintf(stdout, ", ");
            }
            fprintf(stdout, "%d", array[i].processesWQ[c]);
        }
        fprintf(stdout, "}");
    }

    return 1;
}

int
runPipe(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    PipeInfo array[16];
    int count = sys_listPipes(array, 16);
    fprintf(stdout, "Listing %d pipe%s:", count, count == 1 ? "" : "s");

    for (int i = 0; i < count; i++) {
        fprintf(stdout, "\nBytes=%u, Readers=%u, Writers=%u, Name=%s", (unsigned int) array[i].remainingBytes,
                (unsigned int) array[i].readerFdCount, (unsigned int) array[i].writerFdCount, array[i].name);

        fprintf(stdout, ", Read Blocked={");
        for (int c = 0; array[i].readBlockedPids[c] >= 0; c++) {
            if (c != 0) {
                fprintf(stdout, ", ");
            }
            fprintf(stdout, "%d", array[i].readBlockedPids[c]);
        }
        fprintf(stdout, "}");

        fprintf(stdout, ", Write Blocked={");
        for (int c = 0; array[i].writeBlockedPids[c] >= 0; c++) {
            if (c != 0) {
                fprintf(stdout, ", ");
            }
            fprintf(stdout, "%d", array[i].writeBlockedPids[c]);
        }
        fprintf(stdout, "}");
    }

    return 1;
}

int
runCat(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo catInfo = {.name = "cat",
                                 .start = catProcess,
                                 .isForeground = isForeground,
                                 .priority = PRIORITY_DEFAULT,
                                 .argc = argc,
                                 .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &catInfo);
    return *createdProcess >= 0;
}

int
runWc(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo wcInfo = {
        .name = "wc", .start = wcProcess, .isForeground = isForeground, .priority = PRIORITY_DEFAULT, .argc = argc, .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &wcInfo);
    return *createdProcess >= 0;
}

int
runFilter(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo filterInfo = {.name = "filter",
                                    .start = filterProcess,
                                    .isForeground = isForeground,
                                    .priority = PRIORITY_DEFAULT,
                                    .argc = argc,
                                    .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &filterInfo);
    return *createdProcess >= 0;
}

int
runLoop(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo loopInfo = {.name = "loop",
                                  .start = loopProcess,
                                  .isForeground = isForeground,
                                  .priority = PRIORITY_DEFAULT,
                                  .argc = argc,
                                  .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &loopInfo);
    return *createdProcess >= 0;
}

int
runTestMM(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo pci = {.name = "testMM",
                             .start = testMM,
                             .isForeground = isForeground,
                             .priority = PRIORITY_DEFAULT,
                             .argc = argc,
                             .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &pci);
    return *createdProcess >= 0;
}

int
runTestSync(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo pci = {.name = "testSync",
                             .start = testSync,
                             .isForeground = isForeground,
                             .priority = PRIORITY_DEFAULT,
                             .argc = argc,
                             .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &pci);
    return *createdProcess >= 0;
}

int
runTestProcesses(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo pci = {.name = "testSync",
                             .start = testProcesses,
                             .isForeground = isForeground,
                             .priority = PRIORITY_DEFAULT,
                             .argc = argc,
                             .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &pci);
    return *createdProcess >= 0;
}

int
runTestPrio(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo pci = {.name = "testPrio",
                             .start = testPrio,
                             .isForeground = isForeground,
                             .priority = PRIORITY_DEFAULT,
                             .argc = argc,
                             .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &pci);
    return *createdProcess >= 0;
}

int
runPhylo(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[], Pid *createdProcess) {
    ProcessCreateInfo pci = {.name = "phylo",
                             .start = startPhylo,
                             .isForeground = isForeground,
                             .priority = PRIORITY_DEFAULT,
                             .argc = argc,
                             .argv = argv};

    *createdProcess = sys_createProcess(stdin, stdout, stderr, &pci);
    return *createdProcess >= 0;
}