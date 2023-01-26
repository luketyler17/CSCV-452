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
int sentinel (void *);
extern int start1 (char *);
void dispatcher();
void launch();
static void enableInterrupts();
static void check_deadlock();
void sort_readylist(proc_ptr process_input);
proc_ptr remove_head();

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
int debugflag = 1;

/* the process table */
proc_struct ProcTable[MAXPROC];

/* Process lists  */
static proc_ptr ReadyList;

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;


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
   ReadyList = NULL;

   /* Initialize the clock interrupt handler */
   //USLOSS_IntVec[USLOSS_CLOCK_INT] = timeSlice();

   //int_vec(CLOCK_INT) = 
   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");
   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }
  
   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");
   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
   if (result < 0) {
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
      return(-2);
   }
   /*Return if function pointer is NULL*/
   if (f == NULL) 
   {
      console("Function pointer invalid, exiting...\n");
      return(-1);
   }
   /*Return if name is NULL*/
   if (name == NULL) 
   {
      console("Program name invalid, exiting....\n");
      return(-1);
   }
   /*Return if Priority is out of range*/
   if (priority < MAXPRIORITY && priority > MINPRIORITY) 
   {
      console("Priority of program out of range, exiting...\n");
      return(-1);
   }
   /* find an empty slot in the process table */
   
   while (ProcTable[proc_slot].state.start != NULL) 
   {
      proc_slot++;
   }
   /* fill-in entry in process table */
   if ( strlen(name) >= (MAXNAME - 1) ) {
      console("fork1(): Process name is too long.  Halting...\n");
      halt(1);
   }
   strcpy(ProcTable[proc_slot].name, name);
   ProcTable[proc_slot].start_func = f;
   if ( arg == NULL )
      ProcTable[proc_slot].start_arg[0] = '\0';
   else if ( strlen(arg) >= (MAXARG - 1) ) {
      console("fork1(): argument too long.  Halting...\n");
      halt(1);
   }
   else
      strcpy(ProcTable[proc_slot].start_arg, arg);

   ;
   ProcTable[proc_slot].stack = (char *) malloc(stacksize);
   ProcTable[proc_slot].stacksize = stacksize;
   ProcTable[proc_slot].pid = next_pid++;
   ProcTable[proc_slot].priority = priority;
   num_procs++;
   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC)
    */
   context_init(&(ProcTable[proc_slot].state), 
      psr_get(), 
      ProcTable[proc_slot].stack, 
      ProcTable[proc_slot].stacksize, 
      launch);

   if (strcmp(name, "sentinel") == 0 || strcmp(name, "start1") == 0)
   {
      ProcTable[proc_slot].parent_pid = -1;
      ProcTable[proc_slot].kid_num = -1;
      Current = &(ProcTable[proc_slot]);
   }
   else
   {
      ProcTable[proc_slot].parent_pid = Current->pid;
      Current->kids++;
      if (Current->child_proc_ptr == NULL)
      {
         Current->child_proc_ptr = &(ProcTable[proc_slot]);
         ProcTable[proc_slot].kid_num = 0;
      }
      else
      {
         proc_ptr child;
         child = Current->child_proc_ptr;
         while(child->next_sibling_ptr != NULL)
         {
            child = child->next_sibling_ptr;
         }
         child->next_sibling_ptr = &ProcTable[proc_slot];
         ProcTable[proc_slot].kid_num = Current->kids;
      }
   }

   /* for future phase(s) */
   p1_fork(ProcTable[proc_slot].pid);

   sort_readylist(&ProcTable[proc_slot]);
   if (strcmp("sentinel", ProcTable[proc_slot].name) == 0)
   {

   }
   else
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
   int living_kids = Current->kids;
   proc_ptr oldChild = Current->child_proc_ptr;
   while(oldChild->status != QUIT)
   {
      quit(0);
   }
   Current->child_proc_ptr = oldChild->next_sibling_ptr;
   return oldChild->pid;
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
   proc_ptr parent;
   if (DEBUG && debugflag)
      {
         console("quit() has been called on process %d with exit code of %d", Current->pid, code);
      }
   
   disableInterrupts();
   if (strcmp(Current->name, "start1") == 0) 
   {
   Current->status = QUIT;
   int i = 0;
   if (Current->parent_pid != -1) {
   while(ProcTable[i].pid != Current->parent_pid)
   {
      i++;
   }
   }
   ProcTable[i].kids_status_list[Current->kid_num] = QUIT;

   p1_quit(Current->pid);
   dispatcher();
   }
   else{
      finish();
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
void dispatcher()
{

   proc_ptr next_process = remove_head();

   if (strcmp(next_process->name, "start1") == 0)
   {

      Current = next_process;
      enableInterrupts();
      context_switch(NULL, &(next_process->state));
   }
   else
   {
      sort_readylist(Current);

      proc_ptr old = Current;
      Current = next_process;
      enableInterrupts();
      context_switch(&(old->state), &(Current->state));
   }
   p1_switch(Current->pid, next_process->pid);
} /* dispatcher */


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
int sentinel (void * dummy)
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
} /* check_deadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */

void enableInterrupts()
{
   if (kernel_or_user() == 0)
   {
      //IN USER MODE
      console("Kernel Error: Not in kernel mode, may not enable interrupts\n");
      halt(1);
   } else {
      //KERNEL MODE
      psr_set(psr_get() | PSR_CURRENT_INT);
   }
}

void sort_readylist(proc_ptr proccess_input)
{
   if (DEBUG && debugflag) {
      console("Sorting process -- Priority: %d, PID: %d.\n", proccess_input->priority, proccess_input->pid);
   }

   if (ReadyList == NULL)  {
      ReadyList = proccess_input;
   }
   else if (proccess_input->priority < ReadyList->priority)
   {
      proccess_input->next_proc_ptr = ReadyList;
      ReadyList = proccess_input;
   }
   else
   {
      proc_ptr next;
      next = ReadyList;
      while(proccess_input->priority > next->priority){
         next = next->next_proc_ptr;
      }
      next->next_proc_ptr = proccess_input;

   }
   
}

proc_ptr remove_head()
{
   proc_ptr tmp = ReadyList;

   ReadyList = ReadyList->next_proc_ptr;
   tmp->next_proc_ptr = NULL;

   return tmp;
}