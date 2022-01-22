#include <stdio.h>
#include <stdlib.h>

int main()
{
    FILE *file_in = fopen("/proc/my_info", "r");
    char str_ver[10000],str_cpu[10000],str_mem[10000],str_time[10000];
    char op[100];
    if (file_in == NULL)
    {
        printf("Error");
        exit(-1);
    }
    if(fscanf(file_in,"\n%*[^\n]\n%[^=]%*[^\n]\n%[^=]%*[^\n]\n%[^=]%*[^\n]\n%[^=]",str_ver,str_cpu,str_mem,str_time))
    {
        //printf("output:\n\ns1 == %s\n\n s2 == %s\n\n s3 == %s\n\n s4 == %s\n",str_ver,str_cpu,str_mem,str_time);
    }
    else
    {
        printf("Failed to read string.\n");
    }
    fclose(file_in);
    while(1)
    {
        printf("\nWhick information do you want?\nVersion(v),CPU(c),Memory(m),Time(t),All(a),Exit(e)?\n");
        if(scanf("%s",op))
        {
            if(*op == 'v')
                printf("\nVersion : %s\n-------------------------\n",str_ver);
            else if(*op == 'c')
                printf("\nCPU  information: \n%s\n-------------------------\n",str_cpu);
            else if(*op == 'm')
                printf("\nMemory  information: \n%s\n-------------------------\n",str_mem);
            else if(*op == 't')
                printf("\nTime   :\n%s\n-------------------------\n",str_time);
            else if(*op == 'a')
            {
                printf("================Version================\n%s",str_ver);
                printf("================CPU================\n%s",str_cpu);
                printf("================Memory================\n%s",str_mem);
                printf("================Time================\n%s",str_time);
            }
            else if(*op == 'e')
                break;
            else
                continue;
        }
        else
        {
            printf("scan op failed!\n");
        }
    }
    return 0;
}