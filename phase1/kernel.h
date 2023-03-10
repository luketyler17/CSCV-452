#define DEBUG 1

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

struct proc_struct {
   proc_ptr       next_proc_ptr;
   proc_ptr       child_proc_ptr;
   proc_ptr       next_sibling_ptr;
   char           name[MAXNAME];          /* process's name */
   char           start_arg[MAXARG];      /* args passed to process */
   context        currentContext;         /* current context for process */
   short          pid;                    /* process id */
   int            priority;
   int (* start_func) (char *);           /* function where process begins -- launch */
   char          *stack;
   unsigned int   stacksize;
   int            status;                 /* READY, BLOCKED, QUIT, etc. */
 
   /* other fields as needed... */
   proc_ptr       quit_children;          // children that have quit
   int            quit_children_num;      // num of quit children
   int            total_time;             // amount of time used by the CPU
   int            startTime;              // time started by CPU - will change on each call
   int            lastRunTime;            // time ended by CPU
   int            parent_pid;             /*IF -1 NO PARENT EXISTS*/
   int            zapped;                 // 1 == TRUE 0 == FALSE
   int            kids;
   int            kid_num;
   int            kids_status_list[MAXPROC];
   int            quit_code;              //if quit, what code is it
   int            proc_table_location;    //location on process table
   int            parent_location;        //parent location on process table
   int            blocked_by;             //pid of process blocking current proccess
};

struct psr_bits {
        unsigned int cur_mode:1;
       unsigned int cur_int_enable:1;
        unsigned int prev_mode:1;
        unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY
#define QUIT 1
#define READY 2
#define JOIN_BLOCK 3
#define RUNNING 4
#define ZAP_BLOCK 5
#define BLOCKED 6
