# gdb-tty-helper
tools to solve `warning: GDB: Failed to set controlling terminal: Operation not permitted`

## compile
to make it run, do the following commands:
```sh
sudo apt install libc-dev liburing-dev
gcc -o gdb-tty-helper gdb-tty-helper.c -luring -lutil
```
## usage
Suppose you have two terminals:
- A terminal: run gdb
- B terminal: deal with target program's IO

First, run `./gdb-tty-helper` in B terminal:
![](/images/post/README/2022-08-18-21-18-09.png)
Then, use the tty to start gdb in A terminal, and run `gdb --tty=<tty> <target_program>`, my target is python:
![](/images/post/README/2022-08-18-21-21-26.png)
use `r` in A terminal, then you can interact with your target program in B terminal.
![](/images/post/README/2022-08-18-21-24-23.png)

## kill
use `killall gdb-tty-helper` to stop the process.
## source 
Originally proposed by enigmaticPhysicist in [stackoverflow post](https://stackoverflow.com/questions/8963208/gdb-display-output-of-target-application-in-a-separate-window)