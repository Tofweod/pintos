#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b


/**
 * 将每个优先队列与其对应的调度策略封装
 * 目前只是用简单的priority_less(),因此不使用该结构体
*/
// struct MLFQ
// {
//   struct list q;
//   list_less_func* schedule_strategy;    
// } MLFQS[MLFQ_SIZE];
struct list mlfq[MLFQ_SIZE];
/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
// next_to_run()使用，当前需要被调度队列的指针
static struct list* ready_list;

// 为每个调度队列中thread分配的时间片
// 时间片的划分会影响mlfq的效能，但此处不作优化
static int splices[MLFQ_SIZE] = {9,17,25};

/**
 * 经过指定tick后进行一次mlfq_emerge()
 * 需要注意该值的设定与splices数组以及调度策略有关，需要经过实践以得到更为优化的取值，此处不作优化
 * 该值既不能太大也不能太小
*/
#define EMERGE_TIME 50
static int emerege_time = 0;

/**
 * 抢占标识，用于实现抢占式调度
 * 当下所有的实现均是非抢占式的，需要在thread_current()剩余时间片用完后再thread_yield调度
 * 抢占式调度运行在其时间片有剩余时也能直接进行thread_yield
*/
static bool PREEMPT_NOW = false;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

// UNUSED NOW
/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
// implemenet of mlfq
/**
 * 调度策略函数集合，目前仅有priority_less
 */
static bool priority_less(const struct list_elem *a,const struct list_elem *b,void* aux UNUSED);
/**
 * 对mlfq操作的函数集合
 * 下面三个函数对mlfq做修改时的参数t，均已假设t不在所有的mlfqs中且不作保证，因此需要谨慎使用
*/
static void change_to_mlfq(struct thread *t,int idx);
static void add_to_mlfq(struct thread *t,int idx);
static void schedule_to_mlfq(struct thread* t);

static void set_rest_time(struct thread *t,int idx);
static int get_rest_time(struct thread *t,int idx);
static void mlfq_emerge_except(struct thread* except);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  int i;
  for(i = 0;i < MLFQ_SIZE;i++)
  {
    list_init(&mlfq[i]);
  }
  // list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  // add_to_mlfq(initial_thread,0);
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/**
 * 根据进程的优先级进行比较
 * priority_less()实现了如下规则：
 * 1.若优先级A>B,则调度A
 * 2.若优先级A=B,则FCFS
*/
bool
priority_less(const struct list_elem *a,const struct list_elem *b,void* aux UNUSED)
{
  return list_entry(a,struct thread,ready_elem)->priority > list_entry(b,struct thread,ready_elem)->priority;
}

// UNUSED
void
remove_from_mlfq(struct thread *t)
{
  struct list_elem *e;
  struct list *l = &mlfq[t->mlfq[MLFQ_SIZE]];
  for(e = list_begin(l);e != list_end(l);e = list_next(e))
  {
    if(e == &t->ready_elem)
    {
	    list_remove(e);
	    break;
    }
  }
}

/**
 * 修改线程t至指定idx的mlfq队列中
 * idx不能越界，且idx != t->mlfq[MLFQ_SIZE]
*/
void
change_to_mlfq(struct thread *t,int idx)
{
  ASSERT(idx >= 0 && idx < MLFQ_SIZE);
  ASSERT(is_thread(t));
  if(idx != t->mlfq[MLFQ_SIZE])
  {
    list_insert_ordered(&mlfq[idx],&t->ready_elem,priority_less,NULL);
    t->mlfq[MLFQ_SIZE] = idx;
  }
}

/**
 * 添加thread t前应确保队列中没有t，此函数不作保证，须谨慎使用
*/
void
add_to_mlfq(struct thread *t,int idx)
{
  ASSERT(idx >= 0 && idx < MLFQ_SIZE);
  ASSERT(is_thread(t));
  list_insert_ordered(&mlfq[idx],&t->ready_elem,priority_less,NULL);
  t->mlfq[MLFQ_SIZE] = idx;
}


/**
 * 抢占式地将某一线程直接添加到对应mlfq队列头部
 * 需要在设置抢占标识PREEMPT_NOW之后才能使用
*/
void
preempt_add_to_mlfq(struct thread *t,int idx)
{
  ASSERT(PREEMPT_NOW);
  ASSERT(idx >= 0 && idx < MLFQ_SIZE);
  ASSERT(is_thread(t));
  list_push_front(&mlfq[i],&t->ready_elem);
  t->mlfq[MLFQ_SIZE] = idx;
}

/**
 * 当t在对应优先队列中剩余时间片为0时，降低其到下一优先级队列，最低队列除外
*/
void schedule_to_mlfq(struct thread* cur)
{
  if (cur->mlfq[MLFQ_SIZE] < MLFQ_SIZE - 1)
  {
    change_to_mlfq(cur, cur->mlfq[MLFQ_SIZE] + 1);
  }
  else
  {
    add_to_mlfq(cur, cur->mlfq[MLFQ_SIZE]);
  }
  set_rest_time(cur, cur->mlfq[MLFQ_SIZE]);
}

//set and get of thread t's rest time in special mlfq
void
set_rest_time(struct thread *t,int idx)
{
  // 对于idle_thread，其分配的时间片恒为最少的一类
  if(t == idle_thread) idx = 0;
  t->mlfq[idx] = splices[idx];
}

int
get_rest_time(struct thread *t,int idx)
{
  return t->mlfq[idx];
}

/**
 * 将指定thread t之外所有非最高优先级队列中thread上浮到最高优先级，预防饥饿现象
 * 一般此处的t均为thread_current()
 * 此函数需要在中断禁止条件下执行
 * thread_tick()中最后注释说明为什么thread_current()不上浮
*/
void
mlfq_emerge_except(struct thread* except)
{
  ASSERT(intr_get_level() == INTR_OFF)
  ASSERT(intr_context());

  struct list_elem *e;
  int i;
  for(int i = 1;i < MLFQ_SIZE;i++)
  {
    for(e = list_begin(&mlfq[i]);e != list_end(&mlfq[i]);)
    {
      struct thread *t = list_entry(e,struct thread,ready_elem);
      if(t == except)
      {
        e = list_next(e);
        continue;
      }
      e = list_remove(e);
      change_to_mlfq(t,0);
      set_rest_time(t,0);
      t->status = THREAD_READY;
    }
  }
}

// thread.c
/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;
  
  /**
   * thread_foreach()需要禁止中断，我不清楚在执行中断处理程序时是否已经禁止中断。
   * 事实上，注释掉144行代码仍能运行，但为保险起见我选择保留此处代码
   * 更新：timer.c中的timer_init()注册timer_interrupt(),intr_register_ext()将以禁止中断的形式注册
   * 此处具体的区别在于通过intr_level来注册igt_gate或trap_gate
   * 至于中断恢复，interrupt最后用IRET指令从内核栈返回用户栈时，同时设置中断允许
  */
  // intr_disable();
  /* 此处简单轮询所有线程，可优化 */
  thread_foreach(&sleep_and_wakeup,NULL);


  /* Enforce preemption. */
  // t在其优先队列中分配所得的时间片用完，降低到下一队列中,最低队列除外
  if (!--t->mlfq[t->mlfq[MLFQ_SIZE]])
  {
    intr_yield_on_return();
  }
  /*
    避免饥饿,确保在指定emerge_time之后一定发生emerge
    intr_merge_on_return()如同intr_yield_on_return()，会在intr_handle()内调用thread_yield()来在emerge之后重新调度
    假设mlfq_emerge_except()将所有threads提升到最高进程，那么t可能会在mlfq中重复出现
    且当前进程时间片用完时不将其emerge也是一种合理的实现
  */
  if(!(emerege_time = (emerege_time+1)%EMERGE_TIME))
  {
    mlfq_emerge_except(t);
    intr_emerge_on_return();
  }

}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);
  
  struct thread* t = thread_current();
  t->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  /**
   * 将唤醒thread置于最高优先级队列
   * 我的想法时当一个thread在满足IO和sleep结束后被唤醒时，应立即做出响应，因此放在最优级队列
   * 因为t一定是blocked，因此其是thread_create()或thread_block()来的,即不在优先队列中，可放心加入
  */
  add_to_mlfq(t,0);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  struct thread* cur = thread_current();
  list_remove(&cur->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
  {
    if(get_rest_time(cur,cur->mlfq[MLFQ_SIZE]))
    {
      // 如果cur的时间片有剩余，则将其重新加入调度队列中
      add_to_mlfq(cur,cur->mlfq[MLFQ_SIZE]);
    }
    else
    {
      // 否则降级
      schedule_to_mlfq(cur);
    }
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

//thread.c
/**
 * 如果一个进程status为BLOCKED且waiting_time不为0，即说明其陷入睡眠，减少睡眠时间并在其为0时唤醒该线程
 * 需要注意的是，THREAD_BLOCKED不仅仅是用于sleep线程的，
 * 由thread_status可知，等待IO也应使用该状态，但其是异步的，可以使用signal来通知进程，但其实现不应放在此处，这也是为什么需要检测waiting_time是否为0
 * 尽管BLOCKED的用处不唯一，此处函数的作用与名称相符
*/
void
sleep_and_wakeup(struct thread* t,void* aux UNUSED)
{

  if(t->status == THREAD_BLOCKED)
    if(t->waiting_time)
      if(!--t->waiting_time)
        thread_unblock(t);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  thread_current ()->priority = new_priority;
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  /* Not yet implemented. */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->waiting_time = 0;
  // 新建线程放入最高优先队列中，设置其时间片，在thread_create()中会自动加入优先队列中
  set_rest_time(t,0);
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  // 从最高优先队列开始调度
  ready_list = &mlfq[MLFQ_SIZE-1];
  int i;
  for(i = 0;i < MLFQ_SIZE-1; i++)
  {
    if(!list_empty(&mlfq[i]))
    {
      ready_list = &mlfq[i];
      break;
    }
  }
  if (list_empty (ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (ready_list), struct thread, ready_elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
