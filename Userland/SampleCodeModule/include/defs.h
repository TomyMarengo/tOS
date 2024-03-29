#ifndef _DEFS_H_
#define _DEFS_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* ------------------- */
/* --- Kernel Defs --- */
/* ------------------- */

/* ---  File Descriptors --- */

/**
 * @brief Standard Input File Descriptor.
 */
#define STDIN 0

/**
 * @brief Standard Output File Descriptor.
 */
#define STDOUT 1

/**
 * @brief Standard Error File Descriptor.
 */
#define STDERR 2

/**
 * @brief Keyboard Input File Descriptor.
 */
#define KBDIN 3

/* --- Memory Management --- */

/**
 * @brief Represents the various categories of supported memory managers.
 */
typedef enum { LIST, BUDDY } MemoryManagerType;

/**
 * @brief Reflects the condition of the system memory at a specific moment.
 */
typedef struct {
    size_t total;
    size_t used;
    MemoryManagerType type;
} MemoryState;

/* --- Processes --- */

/**
 * @brief Represents a process id.
 */
typedef int Pid;

/**
 * @brief Represents a process priority.
 */
typedef int8_t Priority;

/**
 * @brief Default priority for a created process.
 */
#define PRIORITY_DEFAULT 0

/**
 * @brief Lowest priority limit reached for a process.
 */
#define PRIORITY_MIN 10

/**
 * @brief Highest priority limit reached for a process.
 */
#define PRIORITY_MAX -10

/**
 * @brief Lowest priority to be considered a process that will run next after unblock.
 */
#define PRIORITY_IMPORTANT -5

/**
 * @brief Maximum length for the name of a system resource, for example a process.
 */
#define MAX_NAME_LENGTH 16

/**
 * @brief Defines the maximum amount of Pids that can be returned by an embedded array in a query.
 */
#define MAX_PID_ARRAY_LENGTH 8

/**
 * @brief Maximum amount of process living at the same time.
 */
#define MAX_PROCESSES 8

/**
 * @brief Process start function.
 */
typedef void (*ProcessStart)(int argc, char *argv[]);

/**
 * @brief Represents the various categories of supported process status.
 */
typedef enum { READY = 0, RUNNING = 1, BLOCKED = 2, KILLED = 3 } ProcessStatus;

/**
 * @brief Represents information of a process at particular time.
 */
typedef struct {
    Pid pid;
    char name[MAX_NAME_LENGTH + 1];
    void *stackEnd;
    void *stackStart;
    int isForeground;
    Priority priority;
    ProcessStatus status;
    void *currentRSP;
} ProcessInfo;

/**
 * @brief Represents the information needed for a create process request.
 */
typedef struct {
    const char *name;
    ProcessStart start;
    int isForeground;
    Priority priority;
    int argc;
    const char *const *argv;
} ProcessCreateInfo;

/* --- Pipes --- */

/**
 * @brief Represents a pipe.
 */
typedef int Pipe;

/**
 * @brief Represents information of a process at particular time.
 */
typedef struct {
    size_t remainingBytes;
    unsigned int readerFdCount;
    unsigned int writerFdCount;
    Pid readBlockedPids[MAX_PID_ARRAY_LENGTH + 1];
    Pid writeBlockedPids[MAX_PID_ARRAY_LENGTH + 1];
    char name[MAX_NAME_LENGTH + 1];
} PipeInfo;

/* --- Semaphores --- */

/**
 * @brief Represents a semaphore.
 */
typedef int8_t Sem;

/**
 * @brief Represents information of a semaphore at particular time.
 */
typedef struct {
    int value;
    int linkedProcesses;
    char name[MAX_NAME_LENGTH + 1];
    Pid processesWQ[MAX_PID_ARRAY_LENGTH + 1];
} SemaphoreInfo;

/* ------------------- */
/* ---  User Defs  --- */
/* ------------------- */

#define MAX_COMMAND_LENGTH 128
#define MAX_ARGS           8
#define MAX_COMMANDS       8
#define PIPE_CHAR          '|'
#define BACKGROUND_CHAR    '&'

typedef int (*CommandFunction)(int stdin, int stdout, int stderr, int isForeground, int argc, const char *const argv[],
                               Pid *createdProcess);

typedef struct {
    CommandFunction function;
    const char *name;
    const char *description;
} Command;

#endif