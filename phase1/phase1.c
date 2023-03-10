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
void disableInterrupts();
int kernel_or_user(void);
void remove_from_block_list_no_add(proc_ptr input);
int getpid(void);

/*TODO*/
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
   // int curTime = sys_clock();
   // if ((curTime - Current->startTime) > 80000)
   // {
   //    dispatcher();
   // }
   time_slice();
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

void startup()
{
   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   for (int i = 0; i < 50; i++)
   {
      ProcTable[i].next_proc_ptr = NULL;
      ProcTable[i].child_proc_ptr = NULL;
      ProcTable[i].next_sibling_ptr = NULL;
      ProcTable[i].pid = NULL;              /* process id */
      ProcTable[i].priority = NULL;
      ProcTable[i].stacksize = NULL;
      ProcTable[i].status = NULL;      /* READY, BLOCKED, QUIT, etc. */
      ProcTable[i].total_time = NULL;  // amount of time used by the CPU
      ProcTable[i].startTime = NULL;   // time started by CPU - will change on each call
      ProcTable[i].lastRunTime = NULL; // time ended by CPU
      ProcTable[i].parent_pid = NULL;  /*IF -1 NO PARENT EXISTS*/
      ProcTable[i].zapped = NULL;      // 1 == TRUE 0 == FALSE
      ProcTable[i].kids = NULL;
      ProcTable[i].kid_num = NULL;
      ProcTable[i].quit_code = NULL;           // if quit, what code is it
      ProcTable[i].proc_table_location = NULL; // location on process table
      ProcTable[i].parent_location = NULL;     // parent location on process table
      ProcTable[i].blocked_by = NULL;          // pid of process blocking current proccess
      ProcTable[i].status = 0;
      ProcTable[i].quit_children_num = 0;
   }

   /* Initialize the Ready list, etc. */
   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");

   /* Initialize the clock interrupt handler */
   int_vec[CLOCK_INT] = clock_handler;

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
   int proc_slot = next_pid % MAXPROC;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);
   disableInterrupts();
   /* test if in kernel mode; halt if in user mode */
   if (!(kernel_or_user()))
   {
      console("Process %d -- called in user mode. Halting process.\n", Current->pid);
      halt(1);
   }
   /* Return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK)
   {
      return (-2);
   }
   /*Return if function pointer is NULL*/
   if (f == NULL)
   {
      return (-1);
   }
   /*Return if name is NULL*/
   if (name == NULL)
   {
      return (-1);
   }
   /*Return if Priority is out of range*/
   if (strcmp(name, "sentinel") && (priority < 1 || priority > 5))
   {
      return (-1);
   }
   /* Return if maximum processes reached */
   if (num_procs >= MAXPROC)
   {
      return (-1);
   }
   /* find an empty slot in the process table */

   while (ProcTable[proc_slot].status != QUIT && ProcTable[proc_slot].status != 0)
   {
      proc_slot++;
      if (proc_slot == MAXPROC)
      {
         proc_slot = 0;
      }
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

   // initialize values to the procTable
   ProcTable[proc_slot].stack = (char *)malloc(stacksize);
   ProcTable[proc_slot].stacksize = stacksize;
   ProcTable[proc_slot].pid = next_pid++;
   ProcTable[proc_slot].priority = priority;
   ProcTable[proc_slot].status = READY;
   ProcTable[proc_slot].proc_table_location = proc_slot;
   ProcTable[proc_slot].parent_location = -1;
   ProcTable[proc_slot].next_proc_ptr = NULL;
   ProcTable[proc_slot].child_proc_ptr = NULL;
   ProcTable[proc_slot].next_sibling_ptr = NULL;
   ProcTable[proc_slot].total_time = 0;
   ProcTable[proc_slot].startTime = 0;
   ProcTable[proc_slot].kid_num = 0;
   ProcTable[proc_slot].quit_code = 0;
   ProcTable[proc_slot].blocked_by = 0;
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
      // add child process to parents child_process ptr
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
         // add sibling process to the parent's process->child_proc_ptr->next_sibling_ptr
         proc_ptr child;
         child = Current->child_proc_ptr;
         while (child->next_sibling_ptr != NULL)
         {
            child = child->next_sibling_ptr;
         }
         child->next_sibling_ptr = &ProcTable[proc_slot];
         // what kid they are in the list of kid_codes is their kid_num - 0 indexed
         ProcTable[proc_slot].kid_num = Current->kids;
      }
   }

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);
   add_next_process(&ProcTable[proc_slot]);
   enableInterrupts();
   if (strcmp("sentinel", ProcTable[proc_slot].name) != 0)
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

   proc_ptr oldChild = Current->child_proc_ptr;

   if (!(kernel_or_user()))
   {
      console("Join() called in user mode. Halting...\n");
      halt(1);
   }
   if (oldChild == NULL)
   {
      return -2;
   }
   if (oldChild->status == QUIT)
   {
      if (Current->zapped == 1)
      {
         *code = oldChild->quit_code;
         return -1;
      }
      *code = oldChild->quit_code;
      Current->child_proc_ptr = oldChild->next_sibling_ptr;
      return oldChild->pid;
   }
   else
   {
      // current process is now going to be blocked by the join
      Blocked = Current;
      Blocked->status = JOIN_BLOCK;
      // add process into blocked_list
      add_next_process_blocked(Blocked);
      // until the child quits, continue running dispatcher until the child runs and quits
      Blocked->blocked_by = oldChild->pid;
      dispatcher();
      if (Current->zapped == 1)
      {
         // if zapped during join, return -1
         Current->child_proc_ptr = oldChild->next_sibling_ptr;
         oldChild->next_sibling_ptr = NULL;
         *code = oldChild->quit_code;
         return -1;
      }
      Blocked->kids--;
      // remove the child from the list of children, make the sibling the new child
      //find the child that quit and remove them from the process list
      proc_ptr quitChild = Current->child_proc_ptr;
      proc_ptr pPrevious = NULL;
      while(quitChild->status != QUIT)
      {
         pPrevious = quitChild;
         quitChild = quitChild->next_sibling_ptr;
      }
      if (pPrevious != NULL && pPrevious->next_sibling_ptr != NULL)
      {
         pPrevious->next_sibling_ptr = quitChild->next_sibling_ptr;
      }
      else
      {
         Current->child_proc_ptr = quitChild->next_sibling_ptr;
      }   
      *code = quitChild->quit_code;
      return quitChild->pid;
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
   if (!(kernel_or_user()))
   {
      console("Quit() called in user mode. Halting...\n");
      halt(1);
   }
   if (DEBUG && debugflag)
   {
      console("quit() has been called on process %d with exit code of %d\n", Current->pid, code);
   }
   if (Current->child_proc_ptr != NULL && Current->child_proc_ptr->status != QUIT)
   {
      console("quit() has been called while process has living children, halting...\n");
      halt(1);
   }
   disableInterrupts();
   Current->status = QUIT;
   int i = 0, found = 0;
   proc_ptr next_ptr;
   if (Current->parent_pid != -1)
   {
      // iterate over proccess table to find parent so we can talk back to the parent about the quitting child
      while (ProcTable[i].pid != Current->parent_pid)
      {
         i++;
      }
      if (ProcTable[i].status == JOIN_BLOCK)
      {
         ProcTable[i].status = READY;
         remove_from_block_list(&ProcTable[i]);
      }
      for (int j = 0; j < 7; j++)
      {
         // check if blocking any processes on the blocked list
         next_ptr = BlockedList[j];
         while (next_ptr != NULL)
         {
            // next_ptr == a process on the blocked list, check if current process is blocking that process on the BlockedList
            if (next_ptr->blocked_by == Current->pid)
            {
               remove_from_block_list(next_ptr);
               next_ptr = BlockedList[j];
            }
            else
            {
               next_ptr = next_ptr->next_proc_ptr;
            }
         }
      }
      Current->quit_code = code;
      ProcTable[i].quit_children = Current;
      ProcTable[i].quit_children_num++;
      ProcTable[i].kids_status_list[Current->kid_num] = code;
      // ProcTable[i].status = READY;
      p1_quit(Current->pid);
      // add_next_process(&ProcTable[i]);
      enableInterrupts();
      num_procs--;
      dispatcher();
   }
   else
   {
      num_procs--;
      enableInterrupts();
      dispatcher();
   }
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
      for (int i = 0; i <= Current->priority - 1; i++)
      {
         if (ReadyList[i] != NULL)
         {
            process_switch = 1;
            Current->status = READY;
            add_next_process(Current);
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
      if (Current != NULL)
      {
         pPreviousContext = &Current->currentContext;
      }
      if (pPreviousContext != NULL)
      {
         // add time ran to process
         Current->lastRunTime = sys_clock();
         Current->total_time = Current->total_time + (Current->lastRunTime - Current->startTime);
      }
      Current = next_process;
      Current->status = RUNNING;
      Current->startTime = sys_clock();
      enableInterrupts();
      context_switch(pPreviousContext, &(next_process->currentContext));
      p1_switch(Current->pid, next_process->pid);
   }
}

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
   // if there is a process on the ready list it will appear in next_proc
   proc_ptr next_proc = fetch_next_process();
   if (next_proc == NULL)
   {
      for (int i = 0; i < 6; i++)
      {
         // now check through the blocked list
         next_proc = BlockedList[i];
         if (next_proc != NULL)
         {
            printf("check_deadlock processes still exist.. halting\n");
            halt(1);
         }
      }
      printf("All processes completed.\n");
      exit(0);
   }
   else
   {
      printf("check_deadlock processes still exist.. halting\n");
      halt(1);
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
/* Removes highest priority process from the ready list and returns it to dispatcher */
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
/* Adds given input process onto the ready list */
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
   if (!(kernel_or_user()))
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
   // add process onto blocked list, tell the process
   // that its being blocked by the specific PID then wait until process quits to return
   add_next_process_blocked(Current);
   proc_ptr blocked_ptr = Current;
   proc_ptr zapped_proc = &ProcTable[i];
   blocked_ptr->blocked_by = zapped_proc->pid;
   zapped_proc->zapped = 1;
   /*
   if (zapped_proc->status == QUIT)
   {
      // if the process quit before we zapped, no need to dispatch
      //zapped_proc->zapped = 0;
      remove_from_block_list_no_add(blocked_ptr);
      return 0;
   }
   while (zapped_proc->status != QUIT)
   {
      blocked_ptr->status = ZAP_BLOCK;
      if (blocked_ptr->zapped == 1)
      {
         zapped_proc->zapped = 0;
         return -1;
      }
      else
      {
         dispatcher();
      }
   }
   //zapped_proc->zapped = 0;
   return 0;
}
*/

   if (zapped_proc->status == QUIT)
   {
      // if the process quit before we zapped, no need to dispatch
      remove_from_block_list_no_add(blocked_ptr);
      zapped_proc->zapped = 0;
      return 0;
   }
   /*
   while (zapped_proc->status != QUIT)
   {
      blocked_ptr->status = ZAP_BLOCK;
      if (blocked_ptr->zapped == 1)
      {

         return -1;
      }
      else
      {
         dispatcher();
      }
   }
   */

   blocked_ptr->status = ZAP_BLOCK;
   dispatcher();
   if (Current->zapped == 1)
   {
      return -1;
   }
   
   zapped_proc->zapped = 0;
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
   char *status;
   //printf("---------------\n");
   printf("PID\tParent\tPriority\tStatus\tNum Kids\tTime Used\tName\n");
   for (int i = 0; i < 50; i++)
   {
      // needs to be redone - relooked at
      if (ProcTable[i].status >= 10)
      {
         printf("%d\t%d\t\t%d\t%d\t%d\t\t\t%d\t\t\t%s\n", ProcTable[i].pid, ProcTable[i].parent_pid, ProcTable[i].priority, ProcTable[i].status, ProcTable[i].kids, ProcTable[i].total_time, ProcTable[i].name);
      }
      else
      {

         switch (ProcTable[i].status)
         {
         case 0:
            status = "EMPTY";
            break;
         case READY:
            status = "READY";
            break;
         case RUNNING:
            status = "RUNNING";
            break;
         case JOIN_BLOCK:
            status = "JOIN BLOCKED";
            break;
         case ZAP_BLOCK:
            status = "ZAP BLOCKED";
            break;
         case BLOCKED:
            status = "BLOCKED";
            break;
         case QUIT:
            status = "QUIT";
            break;
         }
         printf("%d\t%d\t\t%d\t%s\t%d\t\t\t%d\t\t\t%s\n", ProcTable[i].pid, ProcTable[i].parent_pid, ProcTable[i].priority, status, ProcTable[i].kids, ProcTable[i].total_time, ProcTable[i].name);
      }
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

/*This process adds an input process to the blocked list at the desired priority
if the priority has a process blocked in that posistion, finds the tail of the linked list and adds it to the tail
using the next_proc_ptr value*/
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

/* Removes given process from the blocked list and adds it into the ready list using add_next_process*/
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
            next_ptr->blocked_by = 0;
            add_next_process(next_ptr);
            return;
         }
      }
      i++;
   }
}

/* Removes given process from the blocked list and DOES NOT add it into the ready list*/
void remove_from_block_list_no_add(proc_ptr input)
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
            next_ptr->blocked_by = 0;
            return;
         }
      }
      i++;
   }
}

/* Blocks the current running process with the value of block being new_status
   if zapped during blocked returns -1*/
int block_me(int new_status)
{
   proc_ptr blocked = Current;
   blocked->status = new_status;
   add_next_process_blocked(blocked);
   /*
   while (blocked->status == new_status)
   {
      if (blocked->zapped == 1)
      {
         // process was zapped while in block_me block
         return -1;
      }
      else
      {
         dispatcher();
      }
   }
   */
  dispatcher();
  if (Current->zapped)
  {
   return -1;
  }
   return 0;
}
/*
   Unblocks the process input by the PID, looks for it on the process table and ensures that it is blocked by
   block_me and not join/zap bock
*/
int unblock_proc(int pid)
{
   // iterate over process table to fid PID
   for (int i = 0; i < 50; i++)
   {
      // once PID found and IS NOT NULL continue
      if (&ProcTable[i] != NULL && ProcTable[i].pid == pid)
      {
         // if status is not greater than 10, return -2
         if (ProcTable[i].status <= 10)
         {
            return -2;
         }
         if (ProcTable[i].pid == Current->pid)
         {
            // attempting to unblock itself
            return -2;
         }
         remove_from_block_list(&ProcTable[i]);
         ProcTable[i].status = READY;
         while (ProcTable[i].status != ZAP_BLOCK && ProcTable[i].status != QUIT)
         {
            // until process is quit, check to ensure process does not get zapped
            dispatcher();
         }
         if (ProcTable[i].status == ZAP_BLOCK)
         {
            // calling process was zapped
            return -1;
         }
         else
         {
            // process quit normally
            return 0;
         }
      };
   }
   // process was not found in table, return -2
   return -2;
}
