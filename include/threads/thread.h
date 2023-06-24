#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#define USERPROG
#define FILESYS
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
#define FDT_PAGES 2        // 파일 디스크립터 테이블에 할당할 페이지 수
#define FD_COUNT_LIMIT 128 // 파일 디스크립터 테이블 인덱스 값 제한(최대 128)
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */

/* ---------------------------------------------- PROJECT1 : Threads - Alarm Clock ---------------------------------------------- */
	int64_t wakeup_tick; // 깨워야 할 스레드 tick 변수 선언
/* ---------------------------------------------- PROJECT1 : Threads - Alarm Clock ---------------------------------------------- */

/* --------------------------------------------------- PROJECT1 : Threads - Priority Scheduling(Priority Invension) --------------------------------------------------- */
	int init_priority; // 스레드가 우선순위를 양도받았다가 도네이션 이후 다시 반납할 때 원래의 우선순위로 돌아올 수 있도록 초기 우선순위 값을 저장하는 변수 선언
	struct lock *wait_on_lock; // 현재 스레드가 얻기 위해 대기 하고 있는 lock의 주소로 이동하기 위한 lock 자료구조의 주소를 저장하는 포인터 변수 선언
	struct list donations; // 스레드가 점유하고 있는 lock을 요청할 때 우선순위를 기부해준 스레드를 저장하기 위한 리스트 선언
	struct list_elem donation_elem; // 스레드가 다른 스레드가 점유하고 있는 lock을 요청했을 때, 다른 스레드에게 priority를 기부하면서 해당 스레드의 donations에 들어갈 때 사용되는 리스트 elem 선언
/* --------------------------------------------------- PROJECT1 : Threads - Priority Scheduling(Priority Invension) --------------------------------------------------- */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	int exit_status; // 스레드 종료 상태 저장 변수 선언(0이면 정상 종료 상태)
	struct file **fdt; // 파일 디스크립터 테이블 변수 선언
	int fd_idx; // 파일 디스크립터 테이블 인덱스 변수 선언
	struct intr_frame parent_if; // 현재 스레드 if_ 선언
	struct list child_list; // 자식 리스트 선언
	struct list_elem child_elem; // 자식 리스트 element 선언
	struct semaphore load_sema; // 현재 스레드가 load되는 동안 부모를 대기시키기 위한 세마포어 선언
	struct semaphore exit_sema; // 자식 스레드가 종료되고 스케줄링이 이어질 수 있도록 부모에게 시그널 보내기 위한 세마포언 선언 
	struct semaphore wait_sema; // 자식 스레드가 종료될 때까지 대기하고 있는 부모에게 자식 스레드가 작업을 종료했다는 시그널을 보내기 위한 세마포어 선언
	struct thread *parent; // 부모 스레드 변수 선언
	struct file *running; // 현재 실행중인 파일 변수 선언
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
	void *rsp;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

/* ---------------------------------------------- PROJECT1 : Threads - Alarm Clock ---------------------------------------------- */
void thread_sleep (int64_t ticks); // 현재 스레드를 ticks까지 재우는 함수 선언
void thread_wakeup (int64_t ticks); // 자고 있는 스레드 중에서 ticks가 지난 스레드를 모두 깨우는 함수 선언
void update_next_tick_to_awake (int64_t ticks); // 가장 빨리 깨워야 할 스레드의 tick을 갱신하는 함수 선언
int64_t get_next_tick_to_awake (void); // 가장 빨리 깨워야 할 스레드의 tick을 반환하는 함수 선언
/* ---------------------------------------------- PROJECT1 : Threads - Alarm Clock ---------------------------------------------- */

/* --------------------------------------------------- PROJECT1 : Threads - Priority Scheduling --------------------------------------------------- */
void test_max_priority (void); // ready_list에서 우선순위가 가장 높은 스레드와 현제 스레드의 우선순위를 비교하는 함수 선언
bool cmp_priority (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED); // 리스트의 첫 번째 인자와 두 번째 인자의 우선순위를 비교하는 함수 선언
/* --------------------------------------------------- PROJECT1 : Threads - Priority Scheduling --------------------------------------------------- */

#endif /* threads/thread.h */
