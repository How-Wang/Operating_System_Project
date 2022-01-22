#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

typedef struct TLB_item
{
    int VPN;
    int PFN;
    int order;
} TLB_item;

typedef struct PT_item
{
    int PFN_or_DBI;
    int In_PM;
} PT_item;

typedef struct PM_item
{
    int reference;
    int process;
    int VP;
    int order;
} PM_item;

char TLB_replace[10], Page_replace[10], Frame_allocate[10];
int P, N, M, Free_Frame, Disk_top, Disk_gap;
int PM_global_clock_index;
int *PM_process_clock_index;
PT_item **PT;
PM_item *PM;
TLB_item TLB[32];

int current_process;
int last_process = -1;
int current_VP;

int *lookup_TLB_count, *hit_TLB_count;
int *total_count, *page_fault_count;

char current_process_string[1];

void Handler();
void Initial_Table();
void Flush_TLB();
void Move_PT_to_TLB();
void Fill_PM();
void Kick_and_Fill_PM();

void FIFO_GLOBAL_handler();
void FIFO_LOCAL_handler();
void CLOCK_GLOBAL_handler();
void CLOCK_LOCAL_handler();

bool Check_Hit_TLB();
bool Check_Hit_PT();

FILE *output_fp;
FILE *analysis_fp;

int main()
{
    // Read system configuration
    FILE *sys_fp;

    sys_fp = fopen("sys_config.txt", "r");
    analysis_fp = fopen("Result.txt","w");

    if (sys_fp == NULL)
        exit(EXIT_FAILURE);
    int count = fscanf(sys_fp, "TLB Replacement Policy: %s\n"
                       "Page Replacement Policy: %s\n"
                       "Frame Allocation Policy: %s\n"
                       "Number of Processes: %d\n"
                       "Number of Virtual Page: %d\n"
                       "Number of Physical Frame: %d",
                       TLB_replace, Page_replace, Frame_allocate, &P, &N, &M);
    if (count != 6)
        printf("input error\n");
    fclose(sys_fp);

    // Create Output file
    output_fp = fopen("output.txt", "w");

    // Initial all value
    Initial_Table();

    // Read trace file line by line, and call handler
    FILE *trace_fp = fopen("trace.txt", "r");
    char *line = NULL;
    size_t len = 0;
    size_t read;
    if (trace_fp == NULL)
        exit(EXIT_FAILURE);
    while ((read = getline(&line, &len, trace_fp)) != -1)
    {
        if (read == 17)
            sscanf(line, "Reference(%[^,], %d)\n", current_process_string, &current_VP);
        else
            sscanf(line, "Reference(%[^,], %d)", current_process_string, &current_VP);
        current_process = current_process_string[0] - 'A';

        if (last_process == -1)
            last_process = current_process;

        // Call Handler
        Handler();
    }
    for(int i=0 ; i<P; i++)
    {
        float hit_rate = ((float)hit_TLB_count[i])/((float)lookup_TLB_count[i]);
        char temp_process_string = i + 'A';
        float effective_access_time = hit_rate*(100+20) + (1 - hit_rate)*(200 + 20);
        float page_fault_rage = ((float)page_fault_count[i])/((float)total_count[i]);
        fprintf(analysis_fp,"Process %c, Effect Access Time = %.3f\n",temp_process_string, effective_access_time);
        fprintf(analysis_fp,"Process %c, Page Fault Rate = %.3f\n",temp_process_string, page_fault_rage);
    }
    fclose(analysis_fp);
    return 0;
}

void Initial_Table()
{
    // Initialize Traslation Lookaside Buffer
    Flush_TLB();
    // Initialize TLB analize count
    lookup_TLB_count = malloc(sizeof(int)* P);
    hit_TLB_count = malloc(sizeof(int)* P);
    total_count = malloc(sizeof(int)* P);
    page_fault_count = malloc(sizeof(int)* P);
    for (int i = 0; i < P; ++i)
    {
        lookup_TLB_count[i] = 0;
        hit_TLB_count[i] = 0;
        total_count[i] = 0;
        page_fault_count[i] = 0;
    }
    // Initialize Physical Memory
    PM = malloc(sizeof(PM_item) * M);
    for (int i = 0; i < M; ++i)
    {
        PM[i].process = -1;
        PM[i].reference = 0;
        PM[i].VP = -1;
        PM[i].order = -1;
    }
    Free_Frame = 0;
    // Initialize Page Table
    PT = malloc(sizeof(PT_item *) * P);
    for (int j = 0; j < P; ++j)
        PT[j] = malloc(sizeof(PT_item) * N);
    for (int i = 0; i < P; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            PT[i][j].In_PM = -1;
            PT[i][j].PFN_or_DBI = -1;
        }
    }
    // Initial clock order of PM
    PM_global_clock_index = 0;
    PM_process_clock_index = malloc(sizeof(int) * P);
    for (int i = 0; i < P; ++i)
    {
        PM_process_clock_index[i] = 0;
    }
    // Initial Disk
    Disk_top = 0;
    Disk_gap = -1;
    // Time seed
    srand(time(NULL));
    return;
}

void Handler()
{
    total_count[current_process]++;
    // Check process change or not
    if (last_process != current_process)
    {
        Flush_TLB();
        last_process = current_process;
    }
    // Check TLB hit or not
    // lookup_TLB_count[current_process]++;
    if (!Check_Hit_TLB())
    {
        if (!Check_Hit_PT())
        {
            if (Free_Frame < M)
                Fill_PM();
            else
            {
                Kick_and_Fill_PM();
            }
        }
    }
}

void Flush_TLB()
{
    for (int i = 0; i < 32; ++i)
    {
        TLB[i].PFN = -1;
        TLB[i].VPN = -1;
        TLB[i].order = -1;
    }
    return;
}

void Move_PT_to_TLB()
{
    if (strcmp(TLB_replace, "LRU") == 0)
    {
        for (int i = 0; i < 32; i++)
        {
            if (((TLB[i].order == 31) || (TLB[i].order == -1)))
            {
                TLB[i].VPN = current_VP;
                TLB[i].PFN = PT[current_process][current_VP].PFN_or_DBI;
                // Change TLB order
                for (int j = 0; j < 32; ++j)
                {
                    if (TLB[j].order != -1)
                    {
                        TLB[j].order += 1;
                    }
                }
                TLB[i].order = 0;
                hit_TLB_count[current_process]++;
                lookup_TLB_count[current_process]++;
                fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, TLB[i].PFN);

                break;
            }
        }
    }
    else if (strcmp(TLB_replace, "RANDOM") == 0)
    {
        int r = rand() % 32;
        for (int i = 0; i < 32; i++)
        {
            if (TLB[i].order == -1)
            {
                TLB[i].VPN = current_VP;
                TLB[i].PFN = PT[current_process][current_VP].PFN_or_DBI;
                hit_TLB_count[current_process]++;
                lookup_TLB_count[current_process]++;
                fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, TLB[i].PFN);
                return;
            }
        }
        for (int i = 0; i < 32; i++)
        {
            if (TLB[i].order == r)
            {
                TLB[i].VPN = current_VP;
                TLB[i].PFN = PT[current_process][current_VP].PFN_or_DBI;
                hit_TLB_count[current_process] ++;
                lookup_TLB_count[current_process] ++;
                fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, TLB[i].PFN);
                return;
            }
        }
    }
}

bool Check_Hit_TLB()
{
    for (int i = 0; i < 32; ++i)
    {
        // if hit
        if (TLB[i].VPN == current_VP)
        {
            lookup_TLB_count[current_process]++;
            hit_TLB_count[current_process]++;
            fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, TLB[i].PFN);

            // if TLB algorithm is LRU
            if (strcmp(TLB_replace, "LRU") == 0)
            {
                // Refresh order
                for (int j = 0; j < 32; ++j)
                {
                    if ((TLB[j].order < TLB[i].order) & (TLB[j].order != -1))
                    {
                        TLB[j].order += 1;
                    }
                }
                TLB[i].order = 0;
                // Refresh reference in Physical Mem
                PM[PT[current_process][current_VP].PFN_or_DBI].reference = 1;
            }
            // if TLB algorithm is RANDOM
            else if (strcmp(TLB_replace, "RANDOM") == 0)
            {
                // Refresh reference in Physical Mem
                PM[PT[current_process][current_VP].PFN_or_DBI].reference = 1;
            }
            return true;
        }
    }
    return false;
}

bool Check_Hit_PT()
{
    // lookup_TLB_count[current_process] = lookup_TLB_count[current_process] + 1;
    if (PT[current_process][current_VP].In_PM == 1)
    {
        lookup_TLB_count[current_process] ++;
        fprintf(output_fp, "Process %s, TLB Miss, Page Hit, %d=>%d\n", current_process_string, current_VP, PT[current_process][current_VP].PFN_or_DBI);
        PM[PT[current_process][current_VP].PFN_or_DBI].reference = 1;
        for (int j = 0; j < M; ++j)
        {
            if ((PM[j].process == current_process) & (PM[j].VP == current_VP))
                PM[j].reference = 1;
        }
        // Move into TLB
        Move_PT_to_TLB();
        return true;
    }
    return false;
}

void Fill_PM()
{
    // Set Physical Mem
    PM[Free_Frame].process = current_process;
    PM[Free_Frame].reference = 1;
    PM[Free_Frame].VP = current_VP;
    for (int j = 0; j < M; ++j)
    {
        if (PM[j].order != -1)
        {
            PM[j].order += 1;
        }
    }
    PM[Free_Frame].order = 0;
    // Set Page Table
    PT[current_process][current_VP].In_PM = 1;
    PT[current_process][current_VP].PFN_or_DBI = Free_Frame;
    // Move Free_Frame
    lookup_TLB_count[current_process]++;
    page_fault_count[current_process]++;
    fprintf(output_fp, "Process %s, TLB Miss, Page Fault, %d, Evict -1 of Process %s to -1, %d<<-1\n", current_process_string, Free_Frame, current_process_string, PM[Free_Frame].VP);
    // Set TLB
    Move_PT_to_TLB();
    Free_Frame++;
    return;
}

void Kick_and_Fill_PM()
{
    if ((strcmp(Page_replace, "FIFO") == 0) & (strcmp(Frame_allocate, "GLOBAL") == 0))
    {
        FIFO_GLOBAL_handler();
    }
    else if ((strcmp(Page_replace, "FIFO") == 0) & (strcmp(Frame_allocate, "LOCAL") == 0))
    {
        FIFO_LOCAL_handler();
    }
    else if ((strcmp(Page_replace, "CLOCK") == 0) & (strcmp(Frame_allocate, "GLOBAL") == 0))
    {
        CLOCK_GLOBAL_handler();
    }
    else if ((strcmp(Page_replace, "CLOCK") == 0) & (strcmp(Frame_allocate, "LOCAL") == 0))
    {
        CLOCK_LOCAL_handler();
    }
    return;
}

void FIFO_GLOBAL_handler()
{
    int move_in_PM_place = -1;
    // Kick
    for (int i = 0; i < M; i++)
    {
        if (PM[i].order == M - 1)
        {
            // Find out the Physical Frame is going to be kicked out
            move_in_PM_place = i;
            /// Refresh Old Page Table
            PT[PM[i].process][PM[i].VP].In_PM = 0;
            char kick_process[1];
            kick_process[0] = 'A' + PM[i].process;
            lookup_TLB_count[current_process] ++;
            page_fault_count[current_process]++;
            fprintf(output_fp, "Process %s, TLB Miss, Page Fault, %d, ", current_process_string, i);
            fprintf(output_fp, "Evict %d of Process %s to ", PM[i].VP, kick_process);
            if (Disk_gap == -1)
            {
                PT[PM[i].process][PM[i].VP].PFN_or_DBI = Disk_top;
                fprintf(output_fp, "%d, ", Disk_top);
                Disk_top += 1;
                break;
            }
            else
            {
                PT[PM[i].process][PM[i].VP].PFN_or_DBI = Disk_gap;
                fprintf(output_fp, "%d, ", Disk_gap);
                Disk_gap = -1;
                break;
            }
        }
    }
    // Move, and Refresh New PT and PM
    /// Refresh New PT if in disk
    if (PT[current_process][current_VP].In_PM == 0)
    {
        Disk_gap = PT[current_process][current_VP].PFN_or_DBI;
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in_PM_place;
        fprintf(output_fp, "%d<<%d\n", current_VP, Disk_gap);
    }
    /// Refresh New PT if not in disk
    else if (PT[current_process][current_VP].In_PM == -1)
    {
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in_PM_place;
        fprintf(output_fp, "%d<<-1\n", current_VP);
    }
    // Refresh TLB if TLB VPN move to disk
    bool have_change_TLB = false;
    for (int j = 0; j < 32; j++)
    {
        if ((TLB[j].VPN == PM[move_in_PM_place].VP)&(current_process == PM[move_in_PM_place].process))
        {
            have_change_TLB = true;
            TLB[j].VPN = current_VP;
            // Refresh order
            for (int p = 0; p < 32; ++p)
            {
                if ((TLB[p].order < TLB[j].order) & (TLB[p].order != -1))
                {
                    TLB[p].order += 1;
                }
            }
            TLB[j].order = 0;
            hit_TLB_count[current_process] ++;
            lookup_TLB_count[current_process] ++;
            fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, move_in_PM_place);
        }
    }
    /// Refresh PM order
    for (int i = 0; i < M; i++)
    {
        if (PM[i].order < PM[move_in_PM_place].order)
        {
            PM[i].order++;
        }
    }
    PM[move_in_PM_place].order = 0;
    /// Refresh PM reference and VPN
    PM[move_in_PM_place].VP = current_VP;
    PM[move_in_PM_place].process = current_process;
    PM[move_in_PM_place].reference = 0;
    if (!have_change_TLB)
        Move_PT_to_TLB();
}
void FIFO_LOCAL_handler()
{
    int move_in_PM_place = -1;
    // Kick
    int max_order = -1;
    for (int j = 0; j < M; j++)
    {
        if (PM[j].process == current_process)
        {
            if (PM[j].order > max_order)
            {
                max_order = PM[j].order;
            }
        }
    }
    for (int i = 0; i < M; i++)
    {
        // for actual place
        if (PM[i].order == max_order)
        {
            // Find out the Physical Frame is going to be kicked out
            move_in_PM_place = i;
            /// Refresh Old Page Table
            PT[PM[i].process][PM[i].VP].In_PM = 0;
            char kick_process[1];
            kick_process[0] = 'A' + PM[i].process;
            lookup_TLB_count[current_process] ++;
            page_fault_count[current_process]++;
            fprintf(output_fp, "Process %s, TLB Miss, Page Fault, %d, ", current_process_string, i);
            fprintf(output_fp, "Evict %d of Process %s to ", PM[i].VP, kick_process);
            if (Disk_gap == -1)
            {
                PT[PM[i].process][PM[i].VP].PFN_or_DBI = Disk_top;
                fprintf(output_fp, "%d, ", Disk_top);
                Disk_top += 1;
                break;
            }
            else
            {
                PT[PM[i].process][PM[i].VP].PFN_or_DBI = Disk_gap;
                fprintf(output_fp, "%d, ", Disk_gap);
                Disk_gap = -1;
                break;
            }
        }
    }
    // Move, and Refresh New PT and PM
    /// Refresh New PT if in disk
    if (PT[current_process][current_VP].In_PM == 0)
    {
        Disk_gap = PT[current_process][current_VP].PFN_or_DBI;
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in_PM_place;
        fprintf(output_fp, "%d<<%d\n", current_VP, Disk_gap);
    }
    /// Refresh New PT if not in disk
    else if (PT[current_process][current_VP].In_PM == -1)
    {
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in_PM_place;
        fprintf(output_fp, "%d<<-1\n", current_VP);
    }
    // Refresh TLB if TLB VPN move to disk
    bool have_change_TLB = false;
    for (int j = 0; j < 32; j++)
    {
        if (TLB[j].VPN == PM[move_in_PM_place].VP)
        {
            have_change_TLB = true;
            TLB[j].VPN = current_VP;
            // Refresh order
            for (int p = 0; p < 32; ++p)
            {
                if ((TLB[p].order < TLB[j].order) & (TLB[p].order != -1))
                {
                    TLB[p].order += 1;
                }
            }
            TLB[j].order = 0;
            hit_TLB_count[current_process] ++;
            lookup_TLB_count[current_process] ++;
            fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, move_in_PM_place);
        }
    }
    /// Refresh PM order
    for (int i = 0; i < M; i++)
    {
        if (PM[i].order < PM[move_in_PM_place].order)
        {
            PM[i].order++;
        }
    }
    PM[move_in_PM_place].order = 0;
    /// Refresh PM reference and VPN
    PM[move_in_PM_place].VP = current_VP;
    PM[move_in_PM_place].process = current_process;
    PM[move_in_PM_place].reference = 0;
    if (!have_change_TLB)
        Move_PT_to_TLB();
}
void CLOCK_GLOBAL_handler()
{
    int move_in = -1;
    // Kick
    do
    {
        // for place
        PM_global_clock_index = PM_global_clock_index % M;
        if (PM[PM_global_clock_index].reference == 0)
        {
            move_in = PM_global_clock_index;
            PM_global_clock_index++;
            /// Refresh Old Page Table
            PT[PM[move_in].process][PM[move_in].VP].In_PM = 0;
            char kick_process[1];
            kick_process[0] = 'A' + PM[move_in].process;
            lookup_TLB_count[current_process] ++;
            page_fault_count[current_process]++;
            fprintf(output_fp, "Process %s, TLB Miss, Page Fault, %d, ", current_process_string, move_in);
            fprintf(output_fp, "Evict %d of Process %s to ", PM[move_in].VP, kick_process);
            if (Disk_gap == -1)
            {
                PT[PM[move_in].process][PM[move_in].VP].PFN_or_DBI = Disk_top;
                fprintf(output_fp, "%d, ", Disk_top);
                Disk_top += 1;
            }
            else
            {
                PT[PM[move_in].process][PM[move_in].VP].PFN_or_DBI = Disk_gap;
                fprintf(output_fp, "%d, ", Disk_gap);
                Disk_gap = -1;
            }
            break;
        }
        else if (PM[PM_global_clock_index].reference == 1)
        {
            PM[PM_global_clock_index].reference = 0;
        }
    }
    while ((PM_global_clock_index++) > -1);
    // Move, and Refresh New PT and PM
    /// Refresh New PT if in disk
    if (PT[current_process][current_VP].In_PM == 0)
    {
        Disk_gap = PT[current_process][current_VP].PFN_or_DBI;
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in;
        fprintf(output_fp, "%d<<%d\n", current_VP, Disk_gap);
    }
    /// Refresh New PT if not in disk
    else if (PT[current_process][current_VP].In_PM == -1)
    {
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in;
        fprintf(output_fp, "%d<<-1\n", current_VP);
    }
    // Refresh TLB if TLB VPN move to disk
    bool have_change_TLB = false;
    for (int j = 0; j < 32; j++)
    {
        if ((TLB[j].VPN == PM[move_in].VP)&(current_process == PM[move_in].process))
        {
            have_change_TLB = true;
            TLB[j].VPN = current_VP;
            // Refresh order
            for (int p = 0; p < 32; ++p)
            {
                if ((TLB[p].order < TLB[j].order) & (TLB[p].order != -1))
                {
                    TLB[p].order += 1;
                }
            }
            TLB[j].order = 0;
            hit_TLB_count[current_process] ++;
            lookup_TLB_count[current_process] ++;
            fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, move_in);
        }
    }
    /// Refresh PM reference and VPN
    PM[move_in].VP = current_VP;
    PM[move_in].process = current_process;
    PM[move_in].reference = 1;
    if (!have_change_TLB)
        Move_PT_to_TLB();
}
void CLOCK_LOCAL_handler()
{
    int move_in = -1;
    // Kick
    do
    {
        // for place
        PM_process_clock_index[current_process] = PM_process_clock_index[current_process] % M;
        if ((PM[PM_process_clock_index[current_process]].reference == 0) & (PM[PM_process_clock_index[current_process]].process == current_process))
        {
            move_in = PM_process_clock_index[current_process];
            PM_process_clock_index[current_process]++;
            /// Refresh Old Page Table
            PT[PM[move_in].process][PM[move_in].VP].In_PM = 0;
            char kick_process[1];
            kick_process[0] = 'A' + PM[move_in].process;
            lookup_TLB_count[current_process] ++;
            page_fault_count[current_process]++;
            fprintf(output_fp, "Process %s, TLB Miss, Page Fault, %d, ", current_process_string, move_in);
            fprintf(output_fp, "Evict %d of Process %s to ", PM[move_in].VP, kick_process);
            if (Disk_gap == -1)
            {
                PT[PM[move_in].process][PM[move_in].VP].PFN_or_DBI = Disk_top;
                fprintf(output_fp, "%d, ", Disk_top);
                Disk_top += 1;
            }
            else
            {
                PT[PM[move_in].process][PM[move_in].VP].PFN_or_DBI = Disk_gap;
                fprintf(output_fp, "%d, ", Disk_gap);
                Disk_gap = -1;
            }
            break;
        }
        else if ((PM[PM_process_clock_index[current_process]].reference == 1) & (PM[PM_process_clock_index[current_process]].process == current_process))
        {
            PM[PM_process_clock_index[current_process]].reference = 0;
        }
    }
    while ((PM_process_clock_index[current_process]++) > -1);
    // Move, and Refresh New PT and PM
    /// Refresh New PT if in disk
    if (PT[current_process][current_VP].In_PM == 0)
    {
        Disk_gap = PT[current_process][current_VP].PFN_or_DBI;
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in;
        fprintf(output_fp, "%d<<%d\n", current_VP, Disk_gap);
    }
    /// Refresh New PT if not in disk
    else if (PT[current_process][current_VP].In_PM == -1)
    {
        PT[current_process][current_VP].In_PM = 1;
        PT[current_process][current_VP].PFN_or_DBI = move_in;
        fprintf(output_fp, "%d<<-1\n", current_VP);
    }
    // Refresh TLB if TLB VPN move to disk
    bool have_change_TLB = false;
    for (int j = 0; j < 32; j++)
    {
        if (TLB[j].VPN == PM[move_in].VP)
        {
            have_change_TLB = true;
            TLB[j].VPN = current_VP;
            // Refresh order
            for (int p = 0; p < 32; ++p)
            {
                if ((TLB[p].order < TLB[j].order) & (TLB[p].order != -1))
                {
                    TLB[p].order += 1;
                }
            }
            TLB[j].order = 0;
            hit_TLB_count[current_process] ++;
            lookup_TLB_count[current_process] ++;
            fprintf(output_fp, "Process %s, TLB Hit, %d=>%d\n", current_process_string, current_VP, move_in);
        }
    }
    /// Refresh PM reference and VPN
    PM[move_in].VP = current_VP;
    PM[move_in].process = current_process;
    PM[move_in].reference = 1;
    if (!have_change_TLB)
        Move_PT_to_TLB();
}