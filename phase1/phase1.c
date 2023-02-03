/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel(void *);
extern int start1(char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();
void sort_readylist(proc_ptr process_input);
proc_ptr remove_head();
proc_ptr fetch_next_process();
void add_next_process(proc_ptr process_input);
void clock_handler(int dev, void *arg);
void add_next_process_blocked(proc_ptr input);
void remove_from_block_list(proc_ptr input);

/*TODO*/
int getpid(void);
void dump_processes(void);
int block_me(int new_status);
int unblock_proc(int pid);
int read_cur_start_time(void);
void time_slice(void);
int readtime(void);
int join(int *status);
void quit(int status);
int zap(int pid);
int is_zapped(void);
/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 0;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
proc_ptr ReadyList[7];
proc_ptr BlockedList[7];
/* current process ID */
proc_ptr Current;
proc_ptr Old;
proc_ptr Blocked;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;

void clock_handler(int dev, void *arg)
{
   int curTime = sys_clock();
   if ((curTime - Current->startTime) > 80000)
   {
      // call to dispatcher();
   }
}
/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
        Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing

  ----------------------------------------------------------------------- */
int num_procs = 0;
// void timeSlice() {
//    if (readtime() >= time_slice) {
//       dispatcher();
//    }
//    return;
// }

void startup()
{
   int i;      /* loop index */
   int result; /* value returned by call to fork1() */
   /* initialize the process table */

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");

   /* Initialize the clock interrupt handler */
   //int (*f)(char *)
   //set int_vec of 0 (aka clock_int) to the function pointer of time_slice()
   //int_vec[CLOCK_INT] = *time_slice();

   /* startup a sentinel process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                  SENTINELPRIORITY);
   if (result < 0)
   {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }

   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
   if (result < 0)
   {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
   if (DEBUG && debugflag)
      console("in finish...\n");
   exit(0);
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(char *), char *arg, int stacksize, int priority)
{
   int proc_slot = 0;
   // Current->status = BLOCKED;
   // Blocked = Current;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);
   disableInterrupts();
   /* test if in kernel mode; halt if in user mode */
   if (!kernel_or_user())
   {
      console("Process %d -- called in user mode. Halting process.\n", Current->pid);
      halt(1);
   }
   /* Return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK)
   {
      console("Process %s could not be allocated\n", name);
      return (-2);
   }
   /*Return if function pointer is NULL*/
   if (f == NULL)
   {
      console("Function pointer invalid, exiting...\n");
      return (-1);
   }
   /*Return if name is NULL*/
   if (name == NULL)
   {
      console("Program name invalid, exiting....\n");
      return (-1);
   }
   /*Return if Priority is out of range*/
   if (priority < MAXPRIORITY && priority > MINPRIORITY)
   {
      console("Priority of program out of range, exiting...\n");
      return (-1);
   }
   /* find an empty slot in the process table */

   while (ProcTable[proc_slot].currentContext.start != NULL)
   {
      proc_slot++;
   }
   /* fill-in entry in process table */
   if (strlen(name) >= (MAXNAME - 1))
   {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }
   strcpy(ProcTable[proc_slot].name, name);
   ProcTable[proc_slot].start_func = f;
   if (arg == NULL)
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if (strlen(arg) >= (MAXARG - 1))
   {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);

   ;
   ProcTable[proc_slot].stack = (char *)malloc(stacksize);
   ProcTable[proc_slot].stacksize = stacksize;
   ProcTable[proc_slot].pid = next_pid++;
   ProcTable[proc_slot].priority = priority;
   ProcTable[proc_slot].status = READY;
   ProcTable[proc_slot].proc_table_location = proc_slot;
   ProcTable[proc_slot].parent_location = -1;
   num_procs++;
   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].currentContext),
                psr_get(),
                ProcTable[proc_slot].stack,
                ProcTable[proc_slot].stacksize,
                launch);

   if (strcmp(name, "sentinel") == 0 || strcmp(name, "start1") == 0)
   {
      ProcTable[proc_slot].parent_pid = -1;
      ProcTable[proc_slot].kid_num = -1;
   }
   else
   {
      ProcTable[proc_slot].parent_pid = Current->pid;
      Current->kids++;
      ProcTable[proc_slot].parent_location = Current->proc_table_location;
      if (Current->child_proc_ptr == NULL)
      {
         Current->child_proc_ptr = &(ProcTable[proc_slot]);
         ProcTable[proc_slot].kid_num = 0;
      }
      else
      {
         proc_ptr child;
         child = Current->child_proc_ptr;
         while (child->next_sibling_ptr != NULL)
         {
            child = child->next_sibling_ptr;
         }
         child->next_sibling_ptr = &ProcTable[proc_slot];
         ProcTable[proc_slot].kid_num = Current->kids;
      }
   }

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);
   add_next_process(&ProcTable[proc_slot]);
   // sort_readylist(&ProcTable[proc_slot]);
   if (strcmp("start1", ProcTable[proc_slot].name) == 0)
   {
      dispatcher();
   }

   return ProcTable[proc_slot].pid;
} /* fork1 */

int kernel_or_user(void)
{
   return PSR_CURRENT_MODE & psr_get();
}

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */

void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   result = Current->start_func(Current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", Current->pid);

   quit(result);

} /* launch */

/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
      -1 if the process was zapped in the join
      -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{

   if (!kernel_or_user())
   {
      console("Join() called in user mode. Halting...\n");
      halt(1);
   }
   if (Current->child_proc_ptr == NULL)
   {
      return -2;
   }
   else
   {
      Blocked = Current;
      Blocked->status = BLOCKED;
      add_next_process_blocked(Blocked);
      proc_ptr oldChild = Current->child_proc_ptr;
      //need to make sure logic works in case of sibling being joined on instead of direct child
      while (oldChild->status != QUIT)
      {
         Blocked->blocked_by = oldChild->pid;
         dispatcher();
         if (Blocked->zapped == 1)
         {
            return -1;
         }
      }
      remove_from_block_list(&Blocked);
      Blocked->status = READY;
      Current->child_proc_ptr = oldChild->next_sibling_ptr;
      *code = oldChild->quit_code;
      return oldChild->pid;
   }
} /* join */

/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
   if (!kernel_or_user())
   {
      console("Quit() called in user mode. Halting...\n");
      halt(1);
   }
   proc_ptr parent;
   if (DEBUG && debugflag)
   {
      console("quit() has been called on process %d with exit code of %d\n", Current->pid, code);
   }

   disableInterrupts();
   Current->status = QUIT;
   int i = 0, found = 0;
   proc_ptr next_ptr;
   if (Current->parent_pid != -1)
   {
      while (ProcTable[i].pid != Current->parent_pid)
      {
         i++;
      }
      for (int j = 0; j < 7; j++)
      {
         next_ptr = BlockedList[j];
         while (next_ptr != NULL)
         {
            //next_ptr == a process on the blocked list, check if current process is blocking that process on the BlockedList
            if (next_ptr->blocked_by == Current->pid)
            {
               found = 1;
               break;
            }
            next_ptr = next_ptr->next_proc_ptr;
         }
         if (found)
         {
            break;
         }
      }
      if (found)
      {
         remove_from_block_list(next_ptr);
      }
      Current->quit_code = code;
      ProcTable[i].kids_status_list[Current->kid_num] = code;
      // ProcTable[i].status = READY;
      p1_quit(Current->pid);
      // add_next_process(&ProcTable[i]);
      dispatcher();
   }
   else
   {
      dispatcher();
   }
   dispatcher();
} /* quit */

/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
   proc_ptr next_process = NULL;
   int process_switch = 0;
   context *pPreviousContext = NULL;
   if (Current != NULL && Current->status == RUNNING)
   {
      // make sure no other process is higher
      for (int i = 0; i < Current->priority - 1; i++)
      {
         // gotta handle time slicing still
         if (ReadyList[i] != NULL)
         {
            process_switch = 1;
         }
      }
   }
   else
   {
      process_switch = 1;
   }
   if (process_switch)
   {
      next_process = fetch_next_process();
      next_process->status = RUNNING;
      if (Current != NULL)
      {
         pPreviousContext = &Current->currentContext;
      }
      Current = next_process;
      context_switch(pPreviousContext, &(next_process->currentContext));
      p1_switch(Current->pid, next_process->pid);
   }
}
// if (Current->status == BLOCKED)
// {
//    next_process->next_proc_ptr = &(Current);
//    //if
//    //XXP1 -> Start1() -> Sentinel
//    //next_process -> take the current process and put it behind
//    // XXp2.status == RUNNING -> XXp1.status == BLOCKED -> start1.status == BLOCKED
//    // XXp2.status == QUIT -> XXp!.status == BLOCKED -> start1.status == BLOCKED
//    // XXp1.status == RUNNING -> start1.status == BLOCKED
//    // XXp1 == QUIT -> start1 == BLOCKED
//    // start1 == RUNNING
//    /*
//       while next_process.status != QUIT
//       {
//          next_process = next_process.next_process_ptr;
//       }
//       Current = next_process;
//       context_switch(Current)
//    */
// }
// if (strcmp(next_process->name, "start1") == 0)
// {
//       Current = next_process;
//       Current->status == RUNNING;
//       enableInterrupts();
//       context_switch(NULL, &(next_process->state));
// }
// else
// {
//    sort_readylist(Current);

//    proc_ptr old = Current;
//    Current = next_process;
//    Current->next_proc_ptr = old;
//    enableInterrupts();
//    context_switch(&(old->state), &(Current->state));
// }
/* dispatcher */

/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
        processes are blocked.  The other is to detect and report
        simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
         and halt.
   ----------------------------------------------------------------------- */
int sentinel(void *dummy)
{
   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */

/* check to determine if deadlock has occurred... */
static void check_deadlock()
{
   proc_ptr next_proc = fetch_next_process();
   if (next_proc == NULL)
   {
      console("All processes completed\n");
      halt(1);
   }
   else
   {
      add_next_process(next_proc);
      dispatcher();
   }
} /* check_deadlock */

/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
   /* turn the interrupts OFF iff we are in kernel mode */
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // not in kernel mode
      console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
      halt(1);
   }
   else
      /* We ARE in kernel mode */
      psr_set(psr_get() & ~PSR_CURRENT_INT);
} /* disableInterrupts */

void enableInterrupts()
{
   if (kernel_or_user() == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not enable interrupts\n");
      halt(1);
   }
   else
   {
      // KERNEL MODE
      psr_set(psr_get() | PSR_CURRENT_INT);
   }
}

// void sort_readylist(proc_ptr proccess_input)
// {
//    if (proccess_input->status != READY)
//    {
//       return;
//    }
//    if (DEBUG && debugflag)
//    {
//       console("Sorting process -- Priority: %d, PID: %d.\n", proccess_input->priority, proccess_input->pid);
//    }

//    if (ReadyList == NULL)
//    {
//       ReadyList = proccess_input;
//    }
//    else if (proccess_input->priority < ReadyList->priority)
//    {
//       proccess_input->next_proc_ptr = ReadyList;
//       ReadyList = proccess_input;
//    }
//    else
//    {
//       proc_ptr next;
//       next = ReadyList;
//       while (proccess_input->priority > next->next_proc_ptr->priority)
//       {
//          next = next->next_proc_ptr;
//       }
//       proccess_input->next_proc_ptr = next->next_proc_ptr;
//       next->next_proc_ptr = proccess_input;
//    }
// }

// proc_ptr remove_head()
// {
//    proc_ptr tmp = ReadyList;

//    ReadyList = ReadyList->next_proc_ptr;
//    tmp->next_proc_ptr = NULL;

//    return tmp;
// }

proc_ptr fetch_next_process()
{
   for (int i = 0; i < 7; i++)
   {
      if (ReadyList[i] != NULL)
      {
         proc_ptr new_proc = ReadyList[i];
         ReadyList[i] = new_proc->next_proc_ptr;
         new_proc->next_proc_ptr = NULL;
         return new_proc;
      }
   }
   return NULL;
}

void add_next_process(proc_ptr input)
{
   int pri = input->priority;
   if (ReadyList[pri] != NULL)
   {
      proc_ptr src = ReadyList[pri];
      while (src->next_proc_ptr != NULL)
      {
         src = src->next_proc_ptr;
      }
      src->next_proc_ptr = input;
   }
   else
   {
      ReadyList[pri] = input;
   }
}

int zap(int pid)
{
   if (!kernel_or_user)
   {
      console("Zap called in user mode:: Halting - Process ID: %d\n", pid);
   }
   if (getpid() == pid)
   {
      console("Program attempted to zap itself, process ID: %d \n", pid);
      halt(1);
   }
   int i = 0;
   while (pid != ProcTable[i].pid)
   {
      i++;
      if (i > 50)
      {
         console("Program attempt to zap program that does not exist! Calling - Process ID %d, Pid to zap %d\n", Current->pid, pid);
         halt(1);
      }
   }
   add_next_process_blocked(Current);
   proc_ptr blocked_ptr = Current;
   proc_ptr zapped_proc = &ProcTable[i];
   blocked_ptr->blocked_by = zapped_proc->pid;
   zapped_proc->zapped = 1;
   while (zapped_proc->status != QUIT)
   {
      blocked_ptr->status = BLOCKED;
      if (blocked_ptr->zapped == 1)
      {

         return -1;
      }
      else
      {
         dispatcher();
      }
   }
   //remove_from_block_list(blocked_ptr);
   return 0;
}

int is_zapped(void)
{
   return Current->zapped;
}

int getpid(void)
{
   return Current->pid;
}

void dump_processes(void)
{
   int i = 0;
   char *status;
   printf("---------------\n");
   printf("PID\tParent\tPriority\tStatus\tNum Kids\tTime Used\tName\n");
   while (ProcTable[i].status != NULL)
   {
      //needs to be redone - relooked at
      if (ProcTable[i].status > 10)
      {
         status = "BLOCK ME";
      }
      else
      {

         switch (ProcTable[i].status)
         {
         case READY:
            status = "READY";
            break;
         case RUNNING:
            status = "RUNNING";
            break;
         case BLOCKED:
            status = "BLOCKED";
            break;
         case QUIT:
            status = "QUIT";
            break;
         }
      }
      printf("%d\t%d\t%d\t%s\t%d\t%d\t%s\n", ProcTable[i].pid, ProcTable[i].parent_pid, ProcTable[i].priority, status, ProcTable[i].kid_num, ProcTable[i].total_time, ProcTable[i].name);
      i++;
   }
}

/*This operation returns the time (in microseconds) at which the currently executing
process began its current time slice*/
int read_cur_start_time(void)
{
   return Current->startTime;
}

/*This operation calls the dispatcher if the currently executing process has exceeded its
time slice. Otherwise, it simply returns.*/
void time_slice(void)
{
   if ((sys_clock() - read_cur_start_time()) > 80000)
   {
      Current->lastRunTime = sys_clock();
      Current->total_time = Current->total_time + (Current->lastRunTime - read_cur_start_time());
      Current->status = READY;
      add_next_process(Current);
      dispatcher();
   }
   else
   {
      return;
   }
}

int readtime(void)
{
   return sys_clock();
}

void add_next_process_blocked(proc_ptr input)
{
   int pri = input->priority;
   if (BlockedList[pri] != NULL)
   {
      proc_ptr src = BlockedList[pri];
      if (src->pid == input->pid)
      {
         return;
      }
      while (src->next_proc_ptr != NULL)
      {
         if (src->pid != input->pid)
         {
            src = src->next_proc_ptr;
         }
         else
         {
            return;
         }
      }
      src->next_proc_ptr = input;
   }
   else
   {
      BlockedList[pri] = input;
   }
}

void remove_from_block_list(proc_ptr input)
{
   int found = 0, i = 0;
   while (!found)
   {
      proc_ptr next_ptr = BlockedList[i];
      while (next_ptr != NULL)
      {
         while (next_ptr != NULL && next_ptr->pid != input->pid)
         {
            next_ptr = next_ptr->next_proc_ptr;
         }
         if (next_ptr != NULL && next_ptr->pid == input->pid)
         {
            BlockedList[i] = next_ptr->next_proc_ptr;
            next_ptr->next_proc_ptr = NULL;
            add_next_process(next_ptr);
            return;
         }
      }
      i++;
   }
}