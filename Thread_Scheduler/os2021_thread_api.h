#ifndef OS2021_API_H
#define OS2021_API_H

#define STACK_SIZE 8192
#define _XOPEN_SOURCE 600

#ifdef __GNUC__
#define UNUSED(arg) arg __attribute__((unused))
#else
#define UNUSED(arg) arg
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "function_libary.h"
#include <json-c/json.h>

typedef struct uthread_node uthread_n;
typedef struct schedule_t schedule_t;
typedef struct uthread_t uthread_t;

void PushReadyQueue(uthread_t* pushed_uthread);
void PushExistedReadyQueue(uthread_n* pushed_uthread);
void PopReadyQueueToNextLevel();
void PopReadyQueueToWaitEvent(int event_id);
void PopReadyQueueToWaitTime(int msec);
void PopWaitTimeQueueToReady();
void PopExistToTerminated(char* job_name);
void SaveOriginalAndWaitSignal();

int OS2021_ThreadCreate(char *job_name, char *p_function, char* priority, int cancel_mode);
void OS2021_ThreadCancel(char *job_name);
void OS2021_ThreadWaitEvent(int event_id);
void OS2021_ThreadSetEvent(int event_id);
void OS2021_ThreadWaitTime(int msec);
void OS2021_ThreadSetEvent(int event_id);
void OS2021_DeallocateThreadResource();
void OS2021_TestCancel();

void CreateContext(ucontext_t *, ucontext_t *, void *);
void ResetTimer();
void Dispatcher();
void StartSchedulingSimulation();

#endif
