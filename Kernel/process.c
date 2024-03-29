#include <defs.h>
#include <graphics.h>
#include <lib.h>
#include <memoryManager.h>
#include <pipe.h>
#include <process.h>
#include <scheduler.h>
#include <string.h>
#include <waitingQueue.h>

#define FD_TABLE_CHUNK_SIZE  8
#define FD_TABLE_MAX_ENTRIES 64
#define MEM_TABLE_CHUNK_SIZE 16
#define MAX_NAME_LENGTH      16

typedef struct {
    void *resource;
    ReadHandler readHandler;
    WriteHandler writeHandler;
    CloseHandler closeHandler;
    DupHandler dupHandler;
} FDEntry;

typedef struct {
    void *stackEnd;
    void *stackStart;
    int isForeground;
    char *name;
    FDEntry *fdTable;
    unsigned int fdTableSize;
    void **memory;
    unsigned int memoryCount, memoryBufSize;
    char **argv;
    int argc;
    WaitingQueue pidWQ;
} Process;

static Process processes[MAX_PROCESSES];

static int deleteFdUnchecked(Process *process, Pid pid, int fd);

static int
getProcessByPid(Pid pid, Process **outProcess) {
    if (pid < 0 || pid >= MAX_PROCESSES || processes[pid].stackEnd == NULL)
        return 0;

    *outProcess = &processes[pid];
    return 1;
}

static int
isValidName(const char *name) {
    if (name == NULL)
        return 0;

    for (int i = 0; i <= MAX_NAME_LENGTH; i++) {
        char c = name[i];
        if (c == '\0')
            return 1;

        if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && c != '_') {
            if (i == 0 || c < '0' || c > '9')
                return 0;
        }
    }

    return 0;
}
Pid
createProcess(const ProcessCreateInfo *createInfo) {
    Pid pid = 0;
    for (; pid < MAX_PROCESSES && processes[pid].stackEnd != NULL; pid++)
        ;

    if (createInfo->argc < 0 || pid == MAX_PROCESSES || !isValidName(createInfo->name))
        return -1;

    void *stackEnd = NULL;
    char *nameCopy = NULL;
    char **argvCopy = NULL;
    if ((stackEnd = malloc(PROCESS_STACK_SIZE)) == NULL || (nameCopy = malloc(strlen(createInfo->name) + 1)) == NULL ||
        (createInfo->argc != 0 && (argvCopy = malloc(sizeof(char *) * createInfo->argc)) == NULL)) {
        free(stackEnd);
        free(nameCopy);
        return -1;
    }

    for (int i = 0; i < createInfo->argc; ++i) {
        size_t length = strlen(createInfo->argv[i]) + 1;

        if ((argvCopy[i] = malloc(length)) == NULL) {
            free(stackEnd);
            free(nameCopy);
            while (i > 0) {
                i--;
                free(argvCopy[i]);
            }
            free(argvCopy);
            return -1;
        }

        memcpy(argvCopy[i], createInfo->argv[i], length);
    }

    strcpy(nameCopy, createInfo->name);

    Process *process = &processes[pid];

    memset(process, 0, sizeof(Process));
    process->stackEnd = stackEnd;
    process->stackStart = stackEnd + PROCESS_STACK_SIZE;
    process->isForeground = createInfo->isForeground;
    process->name = nameCopy;
    process->argv = argvCopy;
    process->argc = createInfo->argc;

    onProcessCreated(pid, createInfo->start, createInfo->priority, process->stackStart, createInfo->argc,
                     (const char *const *) argvCopy);

    return pid;
}

int
kill(Pid pid) {
    Process *process;
    if (!getProcessByPid(pid, &process))
        return 1;

    for (int fd = 0; fd < process->fdTableSize; fd++)
        if (process->fdTable[fd].resource != NULL)
            deleteFdUnchecked(process, pid, fd);

    for (int i = 0; i < process->memoryCount; i++)
        free(process->memory[i]);
    free(process->memory);

    onProcessKilled(pid);

    if (process->pidWQ != NULL) {
        unblockAllInQueue(process->pidWQ);
        freeQueue(process->pidWQ);
    }

    for (int i = 0; i < process->argc; i++) {
        free(process->argv[i]);
    }

    free(process->argv);
    free(process->stackEnd);
    free(process->name);
    free(process->fdTable);
    memset(process, 0, sizeof(Process));

    return 0;
}

void *
handleMalloc(Pid pid, size_t size) {
    Process *process;
    if (!getProcessByPid(pid, &process))
        return NULL;

    if (process->memoryBufSize == process->memoryCount) {
        size_t newBufSize = process->memoryBufSize + MEM_TABLE_CHUNK_SIZE;
        void **newMemory = realloc(process->memory, newBufSize * sizeof(Process));
        if (newMemory == NULL)
            return NULL;

        process->memory = newMemory;
        process->memoryBufSize = newBufSize;
    }

    void *ptr = malloc(size);

    if (ptr != NULL)
        process->memory[process->memoryCount++] = ptr;

    return ptr;
}

int
handleFree(Pid pid, void *memorySegment) {
    Process *process;
    if (!getProcessByPid(pid, &process))
        return 1;

    int index = -1;
    for (int i = 0; i < process->memoryCount; i++) {
        if (process->memory[i] == memorySegment) {
            index = i;
            break;
        }
    }

    if (index == -1)
        return 1;

    process->memoryCount--;
    for (int i = index; i < process->memoryCount; i++)
        process->memory[i] = process->memory[i + 1];

    return free(memorySegment);
}

void *
handleRealloc(Pid pid, void *memorySegment, size_t size) {
    Process *process;
    if (!getProcessByPid(pid, &process))
        return NULL;

    int index = -1;
    for (int i = 0; i < process->memoryCount; i++) {
        if (process->memory[i] == memorySegment) {
            index = i;
            break;
        }
    }

    if (index == -1)
        return NULL;

    void *newPtr = realloc(memorySegment, size);

    if (newPtr != NULL)
        process->memory[index] = newPtr;

    return newPtr;
}

int
isForeground(Pid pid) {
    Process *process;
    if (!getProcessByPid(pid, &process))
        return -1;

    return process->isForeground;
}

int
toForeground(Pid pid) {
    Process *process;
    if (!getProcessByPid(pid, &process))
        return -1;

    return process->isForeground = 1;
}

int
toBackground(Pid pid) {
    Process *process;
    if (!getProcessByPid(pid, &process))
        return -1;

    return process->isForeground = 0;
}

int
addFd(Pid pid, int fd, void *resource, ReadHandler readHandler, WriteHandler writeHandler, CloseHandler closeHandler,
      DupHandler dupHandler) {
    Process *process;
    if (resource == NULL || !getProcessByPid(pid, &process))
        return -1;

    if (fd < 0) {
        for (fd = 3; fd < process->fdTableSize && process->fdTable[fd].resource != NULL; fd++)
            ;
    } else {
        if (fd < process->fdTableSize && process->fdTable[fd].resource != NULL)
            return -1;
    }

    // If the table doesn't have enough space for this fd, expand it.
    if (fd >= process->fdTableSize) {
        size_t newFdTableSize = (process->fdTableSize + FD_TABLE_CHUNK_SIZE) / FD_TABLE_CHUNK_SIZE * FD_TABLE_CHUNK_SIZE;
        if (newFdTableSize > FD_TABLE_MAX_ENTRIES)
            newFdTableSize = FD_TABLE_MAX_ENTRIES;

        if (fd >= newFdTableSize)
            return -1;

        FDEntry *newFdTable = realloc(process->fdTable, sizeof(FDEntry) * newFdTableSize);
        if (newFdTable == NULL)
            return -1;

        memset(&newFdTable[process->fdTableSize], 0, sizeof(FDEntry) * (newFdTableSize - process->fdTableSize));
        process->fdTable = newFdTable;
        process->fdTableSize = newFdTableSize;
    }

    process->fdTable[fd].resource = resource;
    process->fdTable[fd].readHandler = readHandler;
    process->fdTable[fd].writeHandler = writeHandler;
    process->fdTable[fd].closeHandler = closeHandler;
    process->fdTable[fd].dupHandler = dupHandler;

    return fd;
}

int
deleteFd(Pid pid, int fd) {
    Process *process;
    if (fd < 0 || !getProcessByPid(pid, &process) || process->fdTableSize <= fd || process->fdTable[fd].resource == NULL)
        return 1;

    return deleteFdUnchecked(process, pid, fd);
}

static int
deleteFdUnchecked(Process *process, Pid pid, int fd) {
    FDEntry *entry = &process->fdTable[fd];
    int r;
    if (entry->closeHandler != NULL && (r = entry->closeHandler(pid, fd, entry->resource)) != 0)
        return r;

    entry->resource = NULL;
    entry->readHandler = NULL;
    entry->writeHandler = NULL;
    entry->closeHandler = NULL;
    return 0;
}

int
dupFd(Pid pidFrom, Pid pidTo, int fdFrom, int fdTo) {
    Process *processFrom;
    if (fdFrom < 0 || !getProcessByPid(pidFrom, &processFrom) || processFrom->fdTableSize <= fdFrom ||
        processFrom->fdTable[fdFrom].resource == NULL || processFrom->fdTable[fdFrom].dupHandler == NULL)
        return -1;

    return processFrom->fdTable[fdFrom].dupHandler(pidFrom, pidTo, fdFrom, fdTo, processFrom->fdTable[fdFrom].resource);
}

ssize_t
handleRead(Pid pid, int fd, char *buffer, size_t count) {
    Process *process;
    FDEntry *entry;
    if (fd < 0 || !getProcessByPid(pid, &process) || process->fdTableSize <= fd ||
        (entry = &process->fdTable[fd])->resource == NULL || entry->readHandler == NULL)
        return -1;
    ssize_t c = entry->readHandler(pid, fd, entry->resource, buffer, count);
    return c;
}

ssize_t
handleWrite(Pid pid, int fd, const char *buffer, size_t count) {
    Process *process;
    FDEntry *entry;
    if (fd < 0 || !getProcessByPid(pid, &process) || process->fdTableSize <= fd ||
        (entry = &process->fdTable[fd])->resource == NULL || entry->writeHandler == NULL)
        return -1;

    return entry->writeHandler(pid, fd, entry->resource, buffer, count);
}

int
unblockOnKilled(Pid pidToUnblock, Pid pidToWait) {
    Process *process;
    if (!getProcessByPid(pidToWait, &process))
        return 1;

    if (process->pidWQ == NULL && (process->pidWQ = newQueue()) == NULL)
        return -1;

    addIfNotExistsInQueue(process->pidWQ, pidToUnblock);
    return 0;
}

uint8_t
listProcesses(ProcessInfo *storingInfo, uint8_t maxProcesses) {
    int processCounter = 0;
    for (int i = 0; i < MAX_PROCESSES && processCounter < maxProcesses; ++i) {
        Process *process = &processes[i];
        if (process->stackEnd != NULL) {
            ProcessInfo *info = &storingInfo[processCounter++];
            info->pid = i;
            strncpy(info->name, process->name, MAX_NAME_LENGTH);
            info->stackEnd = process->stackEnd;
            info->stackStart = process->stackStart;
            info->isForeground = process->isForeground;
            getProcessInfo(i, info);
        }
    }

    return processCounter;
}