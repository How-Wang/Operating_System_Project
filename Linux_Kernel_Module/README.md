# Linux Kernel Module
## Objectives
In this project, I create my own linux kernel module. The module shows up 
1. Version :
    - Linux version 
2. CPU :
    - processor、model name、physical-id、core-id、cache size、clfsh size、cache alignment、address sizes 
3. memory information
    - MemTotal、MemFree、Buffers、Activat、Inactivate、Shmen、Dirty、Writeback、KernelStack、PageTables
5. time information
    - Uptime
    - Idletime
## Excution code
Execate the instructions in hw1_How_Wang/module
```shell=
    make          # compile the module 
    make ins      # insert modules into the kernel
    make rm       # remove modules from kernel
    make clean    # clean .o file
```
![](https://i.imgur.com/3XEacxZ.png)

---
Execute the instructions anywhere in kernel envirnoment
```shell=
    cat /proc/my_info
```
![](https://i.imgur.com/qmjB6hJ.png)
![](https://i.imgur.com/bT5TIot.png)

---
You also can execute app file to select the info you want to see, and it has been written in make file in hw1-How-Wang
```shell=
    make        # compile the app.c
    ./app       # execute app
```
![](https://i.imgur.com/GcxI0Zb.png)
