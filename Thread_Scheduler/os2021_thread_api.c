#include "os2021_thread_api.h"

static volatile sig_atomic_t clock_flag = 0;        // clock handler is modified queue
static volatile sig_atomic_t user_mode_flag = 0;    // user mode is modified queue
static volatile sig_atomic_t change_to_wait = 0;    // thread is in waitting queue, so it need to wait
static volatile sig_atomic_t original_update = 0;   // original thread is updated
static volatile sig_atomic_t change_to_cancell = 0; // thread is in terminaled queuen, so it need to stop
/*initiate periodic timer signals*/
struct itimerval Signaltimer;
struct sigaction sa;
struct sigaction sa2;

int function_id;
int total_thread_num;
enum ThreadState {READY,RUNNING,WAITTING,TERMINATED};

struct uthread_t
{
    ucontext_t ctx;
    enum ThreadState state;

    char *job_name;
    char *p_function;
    int priority; // 2 for H, 1 for M, 0 for L
    int old_priority;
    int cancel_mode;
    int thread_id;

    int TQ_remaining_time;
    int waitting_event;// 012...7
    int waitting_time;

    int ready_time;
    int remain_waitting_time;
    int cancel_flag;

};
struct uthread_node
{
    uthread_t uthread;
    uthread_n* next_node;
};
struct schedule_t
{
    uthread_n* Ready_queue;
    uthread_n* WaittingEvent_queue;
    uthread_n* WaittingTime_queue;
    uthread_n* Terminated_queue;
};

uthread_n* original_thread;
schedule_t* scheduler;

static void PrintScheduler(int UNUSED(signo))
{
    printf("\n************************************************************************\n");
    printf("*  TID     NAME       State    B_Prioriy  C_Prioriy  Q_Time   W_Time   *\n");
    uthread_n* print_queue[4]= {scheduler->Ready_queue,scheduler->WaittingEvent_queue,scheduler->WaittingTime_queue,scheduler->Terminated_queue};
    for(int i=0 ; i<4; i++)
    {
        uthread_n* print_temp = print_queue[i];
        while(print_temp != NULL)
        {
            printf("*  %-8d",print_temp->uthread.thread_id);
            printf("%-11s",print_temp->uthread.job_name);
            if(print_temp->uthread.state == 0)
                printf("READY     ");
            else if(print_temp->uthread.state == 1)
                printf("RUNNING   ");
            else if(print_temp->uthread.state == 2)
                printf("WAITTING  ");
            else if(print_temp->uthread.state == 3)
                printf("TERMINATED");
            if(print_temp->uthread.old_priority==2)
                printf("%-11s","H");
            else if(print_temp->uthread.old_priority==1)
                printf("%-11s","M");
            else if(print_temp->uthread.old_priority==0)
                printf("%-11s","L");
            if(print_temp->uthread.priority==2)
                printf("%-11s","H");
            else if(print_temp->uthread.priority==1)
                printf("%-11s","M");
            else if(print_temp->uthread.priority==0)
                printf("%-11s","L");

            printf("%-8d",print_temp->uthread.ready_time);
            printf("%-8d *\n",print_temp->uthread.waitting_time);
            print_temp = print_temp->next_node;
        }
    }
    printf("************************************************************************\n");
}
void PushReadyQueue(uthread_t* pushed_uthread)
{
    // Push a uthread_t* to ReadyQueue
    // If the Ready queue is empty
    if(scheduler->Ready_queue==NULL)
    {
        scheduler->Ready_queue = (uthread_n *)malloc(sizeof(uthread_n));
        scheduler->Ready_queue->uthread = *pushed_uthread;
        scheduler->Ready_queue->next_node = NULL;
    }
    else
    {
        // Find out the proper space of uthread in ready queue
        // Compare the fist node in Ready_queue
        if(scheduler->Ready_queue->uthread.priority < pushed_uthread->priority)
        {
            uthread_n *temp_uthread_n = (uthread_n*)malloc(sizeof(uthread_n));
            temp_uthread_n->uthread = *pushed_uthread;
            temp_uthread_n->next_node = scheduler->Ready_queue;
            scheduler->Ready_queue = temp_uthread_n;
        }
        // Move to next one
        else
        {
            uthread_n* temp_Ready_queue = scheduler->Ready_queue;
            while((temp_Ready_queue->next_node != NULL))
            {
                if(temp_Ready_queue->next_node->uthread.priority < pushed_uthread->priority)
                    break;
                else
                    temp_Ready_queue = temp_Ready_queue->next_node;
            }
            // If it has been the last one of queue
            if(temp_Ready_queue->next_node == NULL)
            {
                temp_Ready_queue->next_node = (uthread_n *)malloc(sizeof(uthread_n));
                temp_Ready_queue->next_node->uthread = *pushed_uthread;
                temp_Ready_queue->next_node->next_node = NULL;
            }
            // If in the middle of queue
            else
            {
                uthread_n* temp = (uthread_n*)malloc(sizeof(uthread_n));
                temp->next_node = temp_Ready_queue->next_node;
                temp->uthread = *pushed_uthread;
                temp_Ready_queue->next_node = temp;
            }
        }
    }

}
void PushExistedReadyQueue(uthread_n* pushed_uthread)
{
    // initial pushed uthread next node to NULL
    pushed_uthread->next_node = NULL;
    pushed_uthread->uthread.state = READY;
    if(pushed_uthread->uthread.priority == 2)
    {
        pushed_uthread->uthread.TQ_remaining_time = 100;
    }
    else if(pushed_uthread->uthread.priority == 1)
    {
        pushed_uthread->uthread.TQ_remaining_time = 200;
    }
    if(pushed_uthread->uthread.priority == 0)
    {
        pushed_uthread->uthread.TQ_remaining_time = 300;
    }
    // If the scheduler is NULL, just assign it
    if(scheduler->Ready_queue == NULL)
    {
        scheduler->Ready_queue = pushed_uthread;
    }
    else
    {
        // if the first one is hit!!
        if(scheduler->Ready_queue->uthread.priority < pushed_uthread->uthread.priority)
        {
            pushed_uthread->next_node = scheduler->Ready_queue;
            scheduler->Ready_queue = pushed_uthread;
        }
        else // Add the node to scheduler Ready queue linked list
        {
            uthread_n* temp_Ready_queue = scheduler->Ready_queue;
            // Find out the proper space of uthread in ready queue
            while((temp_Ready_queue->next_node != NULL))
            {
                if(temp_Ready_queue->next_node->uthread.priority < pushed_uthread->uthread.priority)
                    break;
                else
                    temp_Ready_queue = temp_Ready_queue->next_node;
            }
            // If it has been the last one of queue
            if(temp_Ready_queue->next_node == NULL)
            {
                temp_Ready_queue->next_node = pushed_uthread;
            }
            // If in the middle of queue
            else
            {
                pushed_uthread->next_node = temp_Ready_queue->next_node;
                temp_Ready_queue->next_node = pushed_uthread;
            }
        }
    }
}
void PopReadyQueueToNextLevel()
{
    // If scheduler -> Ready_queue more than one node
    // here we need to consider an example
    // new_thread1(temp1 points to here) -> original_thread(temp2 points to here) -> new_thread2 -> old_thread
    if(scheduler->Ready_queue->next_node != NULL)
    {
        // the thread in front of original will skip without doubt(ex: new_thread1),
        // but we need to consider new thread "behind" the original(ex: new_thread2),
        // this is the reason why we use scheduler->Ready_queue in temp1
        uthread_n* temp1 = scheduler->Ready_queue;
        uthread_n* temp2 = original_thread;
        // temp2->next_node = NULL;
        // Find out proper place in ready queue
        /// First, remove original in Ready queue
        /// Second,insert again
        if(scheduler->Ready_queue->uthread.job_name == original_thread->uthread.job_name)
        {
            // if the first one hit
            scheduler->Ready_queue = scheduler->Ready_queue->next_node;
        }
        else
        {
            while(temp1->next_node->uthread.job_name != original_thread->uthread.job_name)
                temp1 = temp1->next_node;
            temp1->next_node = temp1->next_node->next_node;
        }
        /// insert again
        PushExistedReadyQueue(temp2);
    }

}
void PopReadyQueueToWaitEvent(int event_id)
{
    // Set scheduler event_id
    original_thread->uthread.waitting_event = event_id;
    // Take out the original one from Ready_queue
    uthread_n* temp1 = scheduler->Ready_queue;
    uthread_n* temp2 = original_thread;
    // if temp1 first thread is original_thread, just move head to next one
    if(scheduler->Ready_queue->uthread.job_name == temp2->uthread.job_name)
    {
        scheduler->Ready_queue = scheduler->Ready_queue->next_node;
    }
    // else find out the true place in new(equals to temp1 and scheduler->Ready_queue)
    // and remove it
    else
    {
        while(temp1->next_node != NULL)
        {
            if(temp1->next_node->uthread.job_name == temp2->uthread.job_name)
                break;
            else
                temp1 = temp1->next_node;
        }
        // Original thread is not in Ready_queue!
        if(temp1->next_node == NULL)
        {
            perror("Original thread is not in Ready_queue!");
        }
        else
        {
            temp1->next_node = temp1->next_node->next_node;
        }
    }
    // set temp2 next to NULL for latter move to WaitEvent_queue
    temp2->next_node = NULL;
    printf("%s wants to wait for event %d\n",temp2->uthread.job_name,event_id);
    // Add the node to scheduler waitting event queue
    // if scheduler->WaittingEvent_queue is empty
    if(scheduler->WaittingEvent_queue == NULL)
    {
        scheduler->WaittingEvent_queue = temp2;
    }
    else
    {
        // Compare the fist node in Ready_queue
        if((scheduler->Ready_queue->uthread.waitting_event > temp2->uthread.waitting_event)
                || ((scheduler->Ready_queue->uthread.waitting_event == temp2->uthread.waitting_event)
                    &(scheduler->Ready_queue->uthread.priority < temp2->uthread.priority))
          )
        {
            temp2->next_node = scheduler->Ready_queue->next_node;
            scheduler->Ready_queue = temp2;
        }
        else
        {
            uthread_n* temp_WaittingEvent_queue = scheduler->WaittingEvent_queue;
            // Find out the appropriate "event" place in waitting event queue
            while(temp_WaittingEvent_queue->next_node != NULL)
            {
                if(temp_WaittingEvent_queue->next_node->uthread.waitting_event >= temp2->uthread.waitting_event)
                    break;
                else
                    temp_WaittingEvent_queue = temp_WaittingEvent_queue->next_node;
            }
            // Find out the appropriate "priority" place,
            // and the waitting_event id need to be the same, that is meaningful
            while(temp_WaittingEvent_queue->next_node != NULL)
            {
                if((temp_WaittingEvent_queue->next_node->uthread.waitting_event != temp2->uthread.waitting_event)
                        || ((temp_WaittingEvent_queue->next_node->uthread.priority < temp2->uthread.priority)
                            &(temp_WaittingEvent_queue->next_node->uthread.waitting_event == temp2->uthread.waitting_event)))
                    break;
                else
                    temp_WaittingEvent_queue = temp_WaittingEvent_queue->next_node;
            }
            // If it has been the last one of queue
            if(temp_WaittingEvent_queue->next_node == NULL)
            {
                temp_WaittingEvent_queue->next_node = temp2;
            }
            // If in the middle of queue
            else
            {
                temp2->next_node = temp_WaittingEvent_queue->next_node;
                temp_WaittingEvent_queue->next_node = temp2;
            }
        }

    }
}
void PopReadyQueueToWaitTime(int msec)
{
    // Set original_thread waitting time
    original_thread->uthread.remain_waitting_time = msec*10;
    // Take out the original one from Ready_queue
    uthread_n* temp1 = scheduler->Ready_queue;
    uthread_n* temp2 = original_thread;
    // if temp1 first thread is original_thread, just move head to next one
    if(scheduler->Ready_queue->uthread.job_name == temp2->uthread.job_name)
    {
        scheduler->Ready_queue = scheduler->Ready_queue->next_node;
    }
    // else find out the true place in new(equals to temp1 and scheduler->Ready_queue)
    // and remove it
    else
    {
        while((temp1->next_node != NULL))
        {
            if(temp1->next_node->uthread.job_name == temp2->uthread.job_name)
                break;
            else
                temp1 = temp1->next_node;
        }
        // Original thread is not in Ready_queue!
        if(temp1->next_node == NULL)
        {
            perror("Original thread is not in Ready_queue!");
        }
        // here temp1->next_node value equals to temp2(original thread)
        else
        {
            temp1->next_node = temp1->next_node->next_node;
        }
    }
    // set temp2 next to NULL for latter move to WaitEvent_queue
    temp2->next_node = NULL;
    // assert temp2 to WaittingTime_queue
    if(scheduler->WaittingTime_queue == NULL)
    {
        scheduler->WaittingTime_queue = temp2;
    }
    else
    {
        temp2->next_node = scheduler->WaittingTime_queue;
        scheduler->WaittingTime_queue = temp2;
    }
}
void PopExistToTerminated(char* job_name)
{
    uthread_n* temp1 = scheduler->Ready_queue;
    uthread_n* temp2;
    // if node is in fisrt temp1
    if(strcmp(scheduler->Ready_queue->uthread.job_name,job_name)==0)
    {
        temp2 = scheduler->Ready_queue;
        temp2->uthread.state = TERMINATED;
        scheduler->Ready_queue = scheduler->Ready_queue->next_node;
        temp2->next_node = NULL;
    }
    // else is located behind
    else
    {
        while(temp1->next_node != NULL)
        {
            if(strcmp(temp1->next_node->uthread.job_name,job_name))
                break;
            temp1 = temp1->next_node;
        }
        if(temp1->next_node != NULL)
        {
            temp2 = temp1->next_node;
            temp1->next_node = temp1->next_node->next_node;
            temp2->next_node = NULL;
            temp2->uthread.state = TERMINATED;
        }
    }

    // Put temp2 to Terminated_queue
    if(scheduler->Terminated_queue == NULL)
    {
        scheduler->Terminated_queue = temp2;
    }
    else
    {
        temp2 -> next_node = scheduler -> Terminated_queue;
        scheduler->Terminated_queue = temp2;
    }
}
void SaveOriginalAndWaitSignal()
{
    // Save the scheduler ready queue to original_thread,for later swapcontext and save the state
    if(original_update == 0)
    {
        original_thread = scheduler->Ready_queue;
        original_update = 1;
    }
    // if clock is used, must stop running user mode, and wait swapcontext in handler
    if(clock_flag == 1)
    {
        while(clock_flag == 1);
        // for(int delay=0; delay<WAIT_RUN_SIGNAL; delay++);
        // return back to new call, save original_update immediately
        if(original_update == 0)
        {
            original_thread = scheduler->Ready_queue;
            original_update = 1;
        }
    }
}
int OS2021_ThreadCreate(char *job_name, char *p_function, char* priority, int cancel_mode)
{
    //printf("hello thread %s 439 Thread Created\n",job_name);
    SaveOriginalAndWaitSignal();
    if(clock_flag == 0)
    {
        void* FunctionAddress[6] = {&Function1,&Function2,&Function3,&Function4,&Function5,&ResourceReclaim};
        total_thread_num += 1;
        // start using user mode, clock cannot access
        user_mode_flag = 1;
        uthread_t *t = (uthread_t*) malloc(sizeof(uthread_t));

        t->job_name = job_name;
        t->p_function = p_function;
        t->cancel_mode = cancel_mode;
        t->waitting_event = -1;
        t->waitting_time = 0;
        t->ready_time = 0;
        t->remain_waitting_time = 0;
        t->state = READY;
        t->cancel_flag = 0;
        t->thread_id = total_thread_num;

        if(strcmp(priority,"H") == 0)
        {
            t->TQ_remaining_time = 100;
            t->priority = 2;
            t->old_priority = 2;
        }
        else if(strcmp(priority,"M") == 0)
        {
            t->TQ_remaining_time = 200;
            t->priority = 1;
            t->old_priority = 1;
        }
        else if(strcmp(priority,"L") == 0)
        {
            t->TQ_remaining_time = 300;
            t->priority = 0;
            t->old_priority = 0;
        }
        //printf("in 482 p_function is %s\n",p_function);
        if(strcmp(p_function,"Function1") == 0)
            function_id = 0;
        else if(strcmp(p_function,"Function2") == 0)
            function_id = 1;
        else if(strcmp(p_function,"Function3") == 0)
            function_id = 2;
        else if(strcmp(p_function,"Function4") == 0)
            function_id = 3;
        else if(strcmp(p_function,"Function5") == 0)
            function_id = 4;
        else if(strcmp(p_function,"ResourceReclaim") == 0)
            function_id = 5;

        CreateContext(&(t->ctx),NULL,FunctionAddress[function_id]);//Next ctx STILL NOT SETTING!!!

        PushReadyQueue(t);
        // stop used user_mode
        user_mode_flag = 0;
    }
    //printf("hello thread %s 488 Thread Created\n",job_name);
    return 1;
}
void OS2021_ThreadCancel(char *job_name)
{
    // check cancel_mode first if 1, make cancel flag to 1, if not cancel it directly
    SaveOriginalAndWaitSignal();
    if(clock_flag == 0)
    {
        // start using user mode, clock cannot access
        user_mode_flag = 1;
        uthread_n* temp = scheduler->Ready_queue;
        while(temp != NULL)
        {
            // find out the thread has the job name in ready queue
            if(strcmp(temp->uthread.job_name,job_name) == 0)
            {
                // check whether can direct cancel it, if no, set flag
                if(temp->uthread.cancel_mode == 1)
                {
                    temp->uthread.cancel_flag = 1;
                }
                // if yes, cancel
                else if(temp->uthread.cancel_mode == 0)
                {
                    PopExistToTerminated(job_name);
                    change_to_cancell = 1;
                    user_mode_flag = 0;
                    while(change_to_cancell == 1);
                }
            }
            temp = temp->next_node;
        }
        // stop using user mode, clock can access
        user_mode_flag = 0;
    }
}
void OS2021_ThreadWaitEvent(int event_id)
{
    // move the first uthread to waitting queue according to the event and priority
    SaveOriginalAndWaitSignal();
    if(clock_flag == 0)
    {
        // start using user mode, clock cannot access
        user_mode_flag = 1;
        change_to_wait = 1;
        PopReadyQueueToWaitEvent(event_id);
        // stop using user mode, clock can access
        user_mode_flag = 0;
        // wait until clock happen, then it can jump out the while loop
        while(change_to_wait == 1);
        // return back to new call, save original_update immediately
        if(original_update == 0)
        {
            original_thread = scheduler->Ready_queue;
            original_update = 1;
        }
    }
}
void OS2021_ThreadWaitTime(int msec)
{
    // move the first ready queue uthread to waitting time queue and set its waitting time
    SaveOriginalAndWaitSignal();
    if(clock_flag == 0)
    {
        // start using user mode, clock cannot access
        user_mode_flag = 1;
        change_to_wait = 1;
        PopReadyQueueToWaitTime(msec);
        // stop using user mode, clock can access
        user_mode_flag = 0;
        // wait until clock happen, then it can jump out the while loop
        while(change_to_wait == 1);
        if(original_update == 0)
        {
            original_thread = scheduler->Ready_queue;
            original_update = 1;
        }
    }
}
void OS2021_ThreadSetEvent(int event_id)
{
    // move the waitting queue uthread to ready queue
    SaveOriginalAndWaitSignal();
    if(clock_flag == 0)
    {
        // start using user mode, clock cannot access
        user_mode_flag = 1;
        uthread_n* temp1 = scheduler->WaittingEvent_queue;
        uthread_n* temp2 = (uthread_n*)malloc(sizeof(uthread_n));
        // if temp1 first one hit
        if(scheduler->WaittingEvent_queue != NULL)
        {
            if(scheduler->WaittingEvent_queue->uthread.waitting_event == event_id)
            {
                uthread_n* temp2 = scheduler->WaittingEvent_queue;
                scheduler->WaittingEvent_queue = scheduler->WaittingEvent_queue->next_node;
                temp2->next_node = NULL;
                printf("%s changes the status of %s to READY\n",original_thread->uthread.job_name,temp2->uthread.job_name);
                PushExistedReadyQueue(temp2);
                //PrintScheduler();
            }
            // if temp1 is more than one node
            else
            {
                while(temp1->next_node != NULL)
                {
                    if(temp1->next_node->uthread.waitting_event == event_id)
                    {
                        temp2 = temp1->next_node;
                        temp1->next_node = temp1->next_node->next_node;
                        temp2->next_node = NULL;
                        printf("%s changes the status of %s to READY\n",original_thread->uthread.job_name,temp2->uthread.job_name);
                        PushExistedReadyQueue(temp2);
                        break;
                    }
                    else
                    {
                        temp1 = temp1->next_node;
                    }
                }
            }
            // stop using user mode, clock can access
            user_mode_flag = 0;
        }

    }
}
void OS2021_DeallocateThreadResource()
{
    // remove Terminated_queue
    SaveOriginalAndWaitSignal();
    if(clock_flag == 0)
    {
        // start using user mode, clock cannot access
        user_mode_flag = 1;
        //uthread_n* temp = scheduler->Terminated_queue;
        uthread_n* temp_removed; //= (uthread_n *)malloc(sizeof(uthread_n)); // Test not to free
        while(scheduler->Terminated_queue != NULL)
        {
            temp_removed = scheduler->Terminated_queue;
            scheduler->Terminated_queue = scheduler->Terminated_queue->next_node;
            printf("The memory space of %s has been released.\n",temp_removed->uthread.job_name);
            free(temp_removed);
        }
        // stop using user mode, clock can access
        user_mode_flag = 0;
    }
}
void OS2021_TestCancel()
{
    // check whether to cancel self in Ready_queue
    SaveOriginalAndWaitSignal();
    if(clock_flag == 0)
    {
        // start using user mode, clock cannot access
        user_mode_flag = 1;
        uthread_n* temp2 = original_thread;
        // check self whether cancel_flag = cancel_mode = 1, if yes. move to Terminated queue
        if((temp2->uthread.cancel_mode == 1) & (temp2->uthread.cancel_flag == 1))
        {
            PopExistToTerminated(temp2->uthread.job_name);
            change_to_cancell = 1;
            user_mode_flag = 0;
            while(change_to_cancell == 1);
        }
        // stop using user mode, clock can access
        user_mode_flag = 0;
    }
}
void CreateContext(ucontext_t *context, ucontext_t *next_context, void *func)
{
    //printf("654 getcontext and makecontext fuc is %p, and successful\n",func);
    // connect the context with the function it need to implement
    getcontext(context);
    context->uc_stack.ss_sp = malloc(STACK_SIZE);
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = next_context;
    makecontext(context,(void (*)(void))func,0);
}
void ResetTimer()
{
    // set the timer
    Signaltimer.it_interval.tv_usec = 10000;
    Signaltimer.it_interval.tv_sec = 0;
    Signaltimer.it_value.tv_sec = 1;
    Signaltimer.it_value.tv_usec = 0;
    if(setitimer(ITIMER_REAL, &Signaltimer, NULL) < 0)
    {
        printf("ERROR SETTING TIME SIGALRM!\n");
    }
    //printf("hello reset timer\n");
}
static void handler(int UNUSED(signo))
{
    if(user_mode_flag == 0)
    {
        // Lock the data avoid segmentation fault
        clock_flag = 1;
        // Add all ready queue time,
        uthread_n* temp = scheduler->Ready_queue;
        uthread_n* temp_removed = (uthread_n*)malloc(sizeof(uthread_n));
        while(temp!=NULL)
        {
            if(temp->uthread.state == READY)
            {
                temp->uthread.ready_time += 10;
            }
            temp = temp -> next_node;
        }
        // Add waitting event queue time
        temp = scheduler->WaittingEvent_queue;
        while(temp!=NULL)
        {
            temp->uthread.waitting_time += 10;
            temp->uthread.state = WAITTING;
            temp = temp -> next_node;
        }
        // Add waitting time queue time, and check back or not
        temp = scheduler->WaittingTime_queue;
        while(temp!=NULL)
        {
            temp->uthread.waitting_time += 10;
            temp->uthread.remain_waitting_time -= 10;
            temp->uthread.state = WAITTING;
            // Check waitting time queue node can push back or not
            if(temp->uthread.remain_waitting_time <= -10)
            {
                // First one hit
                if(temp->uthread.job_name == scheduler->WaittingTime_queue->uthread.job_name)
                {
                    temp_removed = temp;
                    scheduler->WaittingTime_queue = scheduler->WaittingTime_queue->next_node;
                    temp = temp->next_node;
                    temp_removed->next_node = NULL;
                    temp_removed->uthread.state = READY;
                    PushExistedReadyQueue(temp_removed);
                }// Other hit
                else
                {
                    temp_removed = temp;
                    temp = temp->next_node;
                    temp_removed->next_node=NULL;
                    temp_removed->uthread.state = READY;
                    PushExistedReadyQueue(temp_removed);
                }
            }// No one hit
            else
            {
                temp = temp->next_node;
            }
        }
        // Change Terminated queue event
        temp = scheduler->Terminated_queue;
        while(temp!=NULL)
        {
            temp->uthread.state = TERMINATED;
            temp = temp->next_node;
        }
        // Minus TQ remaining time and
        original_thread->uthread.TQ_remaining_time -= 10;
        // If TQ time == 0 lower level, and change next uthread to run
        if(original_update == 0)
        {
            original_thread = scheduler->Ready_queue;
            original_update = 1;
        }
        original_thread -> uthread.state = RUNNING;
        // Check Current thread is waitting to be change to Wait event queue
        // Flag will be set in OS2021_ThreadWaitTime, OS2021_ThreadWaitEvent
        if(change_to_wait == 1)
        {
            change_to_wait = 0;
            original_update = 0;
            clock_flag = 0;
            if(original_thread->uthread.priority == 2)
            {
                original_thread->uthread.priority = 2;
                original_thread->uthread.TQ_remaining_time = 100;
            }
            else if(original_thread->uthread.priority == 1)
            {
                // if wait hapen TQ remaining time != 0, we need to change prority
                if(original_thread->uthread.TQ_remaining_time != -10)
                {
                    printf("the priority of thread %s is change from M to H\n",original_thread->uthread.job_name);
                    original_thread->uthread.priority = 2;
                    original_thread->uthread.TQ_remaining_time = 100;
                }
                // we don't change priority
                else
                {
                    original_thread->uthread.priority = 1;
                    original_thread->uthread.TQ_remaining_time = 200;
                }
            }
            else if(original_thread->uthread.priority == 0)
            {
                // if wait hapen TQ remaining time != 0, we need to change prority
                if(original_thread->uthread.TQ_remaining_time == -10)
                {
                    printf("the priority of thread %s is change from L to M\n",original_thread->uthread.job_name);
                    original_thread->uthread.priority = 1;
                    original_thread->uthread.TQ_remaining_time = 200;
                }
                // we don't change priority
                else
                {
                    original_thread->uthread.priority = 0;
                    original_thread->uthread.TQ_remaining_time = 300;
                }

            }
            swapcontext(&(original_thread->uthread.ctx),&(scheduler->Ready_queue->uthread.ctx));
        }
        else if(change_to_cancell == 1)
        {
            change_to_cancell = 0;
            original_update = 0;
            clock_flag = 0;
            swapcontext(&(original_thread->uthread.ctx),&(scheduler->Ready_queue->uthread.ctx));
        }
        else if(original_thread->uthread.TQ_remaining_time <= 0)
        {
            if(original_thread->uthread.priority == 2)
            {
                printf("the priority of thread %s is change from H to M\n",original_thread->uthread.job_name);
                original_thread->uthread.priority = 1;
                original_thread->uthread.TQ_remaining_time = 200;
            }
            else if(original_thread->uthread.priority == 1)
            {
                printf("the priority of thread %s is change from M to L\n",original_thread->uthread.job_name);
                original_thread->uthread.priority = 0;
                original_thread->uthread.TQ_remaining_time = 300;
            }
            else if(original_thread->uthread.priority == 0)
            {
                original_thread->uthread.priority = 0;
                original_thread->uthread.TQ_remaining_time = 300;
            }
            PopReadyQueueToNextLevel();
            original_update = 0;
            clock_flag = 0;
            swapcontext(&(original_thread->uthread.ctx),&(scheduler->Ready_queue->uthread.ctx));
        }
    }
}
void InitialReadyQueue()
{
    //read json file, used OS2021_ThreadCreate, reset origianl thread
    FILE *fp;
    char buffer[1024];
    struct json_object *parsed_json;
    struct json_object *Thread_array;
    struct json_object *Thread_object;
    struct json_object *name;
    struct json_object *entry_function;
    struct json_object *priority;
    struct json_object *cancel_mode;
    size_t Thread_numbers;
    size_t i;
    char* name_string;
    char* entry_functon_string;
    char* priority_string;
    int cancel_mode_num;

    fp = fopen("init_threads.json","r");
    fread(buffer, 1024, 1, fp);
    fclose(fp);

    parsed_json = json_tokener_parse(buffer);

    json_object_object_get_ex(parsed_json, "Threads", &Thread_array);
    Thread_numbers = json_object_array_length(Thread_array);
    for(i=0; i<Thread_numbers; i++)
    {
        Thread_object = json_object_array_get_idx(Thread_array, i);
        json_object_object_get_ex(Thread_object,"name",&name);
        json_object_object_get_ex(Thread_object,"entry function",&entry_function);
        json_object_object_get_ex(Thread_object,"priority",&priority);
        json_object_object_get_ex(Thread_object,"cancel mode",&cancel_mode);
        name_string = (char *)json_object_get_string(name);
        entry_functon_string = (char *)json_object_get_string(entry_function);
        priority_string = (char *)json_object_get_string(priority);
        cancel_mode_num  = json_object_get_int(cancel_mode);

        OS2021_ThreadCreate(name_string,entry_functon_string,priority_string,cancel_mode_num);
    }
    OS2021_ThreadCreate("reclaimer","ResourceReclaim","L",1);
    original_thread = scheduler->Ready_queue;
    original_update = 0;
}
void StartSchedulingSimulation()
{
    // Create Scheduler include three queue
    scheduler = (schedule_t*)malloc(sizeof(schedule_t));
    scheduler->Ready_queue = NULL;
    scheduler->WaittingEvent_queue = NULL;
    scheduler->WaittingTime_queue = NULL;
    scheduler->Terminated_queue = NULL;
    total_thread_num = 0;

    // InitialReadyQueue
    InitialReadyQueue();

    // Set Timer
    ResetTimer();

    // Initiate periodic timer signals
    memset((void *)&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, 0))
    {
        perror("sigaction");
        exit(1);
    }
    memset((void *)&sa2, 0, sizeof sa);
    sa2.sa_handler = PrintScheduler;
    sa2.sa_flags = SA_SIGINFO;
    if (sigaction(SIGTSTP, &sa2, 0))
    {
        perror("sigaction");
        exit(1);
    }
    // start running
    setcontext(&(original_thread->uthread.ctx));
}
