#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
	char *save_ptr;
    strtok_r (file_name, " ", &save_ptr); // 인자로 들어오는 file_name parsing
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */

	/* Create a new thread to execute FILE_NAME. */
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy); // 위에서 parsing한 파일 이름을 새로 생성할 스레드 이름으로 지정
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0) // load가 실패한 경우(즉, 반환값이 -1인 경우) 오류
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* 현재 프로세스를 복제하여 name이라는 이름의 새로운 프로세스를 만들고, 그 프로세스의 스레드 id를 반환한다.
   만약, 스레드 생성에 실패하면 TID_ERROR를 반환한다. */
/* 현재 프로세스를 복제하는 함수 */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	// 현재 스레드의 parent_if에 복제해야 하는 if_ 복사
	struct thread *cur = thread_current ();
	memcpy (&cur->parent_if, if_, sizeof(struct intr_frame));

	// __do_fork() 함수를 이용하여 현재 스레드를 복제한 새로운 스레드 생성하고, 생성된 스레드의 id인 tid를 반환
	tid_t tid = thread_create (name, PRI_DEFAULT, __do_fork, cur);
	
	// 반환된 tid가 TID_ERROR인 경우 TID_ERROR 반환(=스레드가 제대로 생성되지 않은 경우)
	if (tid == TID_ERROR)
		return TID_ERROR;

	// 자식이 로드될 때까지 대기하기 위해서 방금 생성한 자식 스레드 검색
	struct thread *child = get_child_process (tid);

	sema_down (&child->load_sema); // 자식이 로드가 완료될 때까지 부모는 대기

	// 자식이 로드되다가 오류로 exit한 경우
	if (child->exit_status == TID_ERROR)
	{
		// list_remove (&child->child_elem); // 자식이 종료되었으므로 자식 리스트에서 제거
		sema_up (&child->exit_sema); // 자식이 종료되고 스케줄링이 이어질 수 있도록 부모에게 시그널 전송
		return TID_ERROR; // TID_ERROR 반환
	}

	return tid; // 자식이 성공적으로 로드된 경우 자식의 tid 반환
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
/* 페이지 테이블을 복제하는 데 사용되는 함수 */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	if (is_kernel_vaddr(va))
		return true;

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page(parent->pml4, va);
	if (parent_page == NULL)
		return false;

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page(PAL_USER | PAL_ZERO);
	if (newpage == NULL)
		return false;

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page(current->pml4, va, newpage, writable))
	{
		/* 6. TODO: if fail to insert page, do error handling. */
		return false;
	}
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	// struct intr_frame *parent_if;
	/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	struct intr_frame *parent_if = &parent->parent_if; // 인자로 전달 받은 부모 스레드의 parent_if 필드의 값을 parent_if에 할당
	/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	bool succ = true;



	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	if_.R.rax = 0; // 자식 프로세스의 리턴값 0으로 초기화
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

	/* 2. Duplicate PT */
	current->pml4 = pml4_create ();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	// file_duplicate() 함수를 이용하여 파일 디스크립터 테이블 복사
	for (int i = 0; i < FD_COUNT_LIMIT; i++) {
		struct file *file = parent->fdt[i];
		if (file == NULL)
			continue;
		if (file > 2)
			file = file_duplicate (file);
		current->fdt[i] = file;
	}
	current->fd_idx = parent->fd_idx;

	// 자식이 로드가 완료될 때까지 기다리고 있던 부모 대기 해제
	sema_up (&current->load_sema);
	process_init ();

	/* Finally, switch to the newly created process. */
	if (succ)
		do_iret (&if_);
error:
	sema_up (&current->load_sema);
	exit (TID_ERROR);
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) { // 문자열 f_name이라는 인자를 입력 받음
	char *file_name = f_name; // f_name은 문자열이지만 void로 넘겨 받았기 때문에 문자열로 인식하기 위해 자료형을 char *로 변환
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if; // 레지스터나 스택 포인터 같은 context switching을 위한 정보를 담고 있는 구조체
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup (); // 새로운 실행 파일을 현재 스레드에 담기 전에 현재 프로세스에 담긴 컨텍스트 삭제(=현재 프로세스에 할당된 page directory와 switch information 삭제)
	supplemental_page_table_init(&thread_current()->spt);
	/* And then load the binary */
	success = load (file_name, &_if); // _if와 file_name을 현재 프로세스에 load(성공하면 1을, 실패하면 0을 반환) -> 이 함수에 parsing 작업을 추가 구현해야 한다.

	/* If load failed, quit. */
	palloc_free_page (file_name); // file_name은 프로그램 파일 이름을 입력하기 위해 생성한 임시 변수이므로 load를 끝내면 해당 메모리를 반환
	if (!success) // load에 실패하면 -1 반환
		return -1;
	
	/* Start switched process. */
	do_iret (&_if); // load가 성공적으로 실행되면, 생성된 프로세스로 context switching을 실행
	NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	struct thread *child = get_child_process (child_tid); // 자식 스레드 검색
	
	// 자식 스레드를 찾는데 실패한 경우 -1을 반환
	if (child == NULL)
		return -1;

	sema_down (&child->wait_sema); // 자식이 종료될 때까지 부모는 대기
	list_remove (&child->child_elem); // 자식이 종료되었다는 wait_sema 시그널을 받으면 부모의 자식 리스트에서 자식을 삭제
	sema_up (&child->exit_sema); // 자식 스레드가 종료되고 스케줄링이 이어질 수 있도록 부모에게 시그널 전송

	return child->exit_status; // 자식의 exit_status 반환
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	// 파일 디스크립터 테이블의 모든 파일을 닫고 메모리를 반환
	for (int i = 2; i < FD_COUNT_LIMIT; i++) {
		if (curr->fdt[i] != NULL)
			close (i);
	}

	palloc_free_multiple (curr->fdt, FDT_PAGES);
	file_close (curr->running); // 현재 실행 중인 파일도 닫음

	process_cleanup (); // 프로세스를 클린업
	// hash_destroy(&curr->spt.spt_hash,NULL);

	sema_up (&curr->wait_sema); // 자식이 종료될 때까지 대기하고 있는 부모에게 자식이 종료되었다는 시그널 전송
	sema_down (&curr->exit_sema); // 자식이 부모의 시그널을 기다렸다가, 대기가 풀리고 나면 다른 스레드가 실행
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
/* 실행파일의 file_name을 적재해 실행하는 함수 */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
	char *argv[LOADER_ARGS_LEN]; // argument 배열 포인터 변수 선언
	char *token, *save_ptr; // 토큰과 parsing하고 남은 문자열의 시작주소 포인터 변수 선언
	int argc = 0; // argument 개수 변수 선언 및 0으로 초기화

	token = strtok_r (file_name, " ", &save_ptr); // 토큰에 문자열을 parsing하고 나온 file_name 저장

	while (token) { // 트큰이 NULL일 때까지 문자열 parsing 수행
		argv[argc++] = token; // 0번째 argument부터 parsing하고 나온 인자 저장
		token = strtok_r (NULL, " ", &save_ptr); // 토큰에 위에서 parsing하고 남은 문자열을 다시 parsing하여 저장(두 번째 parsing부터는 첫 번째 인자를 NULL로 설정)
	}
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	file = filesys_open (argv[0]);
	if (file == NULL) {
		printf ("load: %s: open failed\n", argv[0]);
		goto done;
	}

	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", argv[0]);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	t->running = file; // 현재 스레드의 실행중인 파일 저장
	file_deny_write(file); // 현재 실행 중인 파일을 수정하는 일이 발생하는 것을 방지하기 위해 실행 중인 파일에 대한 쓰기 작업을 거부하는 file_deny_write() 함수 호출
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	success = true;

/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
	argument_stack (argv, argc, if_); // parsing한 arguments를 user stack에 넣어주는 argument_stack() 함수 호출
	// hex_dump(if_->rsp, if_->rsp, USER_STACK - if_->rsp, true); // user stack을 16진수로 출력해주기 위한 hex_dump() 함수 호출
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */

done:
	/* We arrive here whether the load is successful or not. */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	// file_close (file); // file_close()를 호출하여 작업을 수행하면 파일이 닫히면서 lock이 풀리므로, 이를 방지하기 위해 주석 처리
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

	return success;
}

/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
/* parsing한 arguments를 user stack에 넣어주는 함수 */
/* if_->rsp는 현재 user stack에서 현재 위치를 가리키는 스택 포인터로, 맨 처음 if_->rsp는 0x47480000(USER_STACK)이다. */
void
argument_stack(char **argv, int argc, struct intr_frame *_if) {
	char *arg_address[128];

	// 1) 프로그램 이름, 인자 문자열 삽입
	// 스택은 아래 방향으로 성장하므로 스택에 인자를 추가할 때 문자열을 오른쪽에서 왼쪽 방향으로(역방향으로) 삽입해야 한다.
	for (int i = argc - 1; i >= 0; i--) { // 맨 끝 NULL 값(arg[4]) 제외하고, 가장 인덱스가 큰 argv부터 스택에 삽입
		int argv_len = strlen(argv[i]);  // 각 인자의 크기 저장
		_if->rsp -= (argv_len + 1); // 각 인자에서 인자 크기(argv_len)를 읽고, 그 크기만큼 rsp를 내림
		memcpy(_if->rsp, argv[i], argv_len + 1); // 그 다음 빈 공간만큼 memcpy() 함수를 이용하여 스택에 삽입(각 인자에 sentinel이 포함이므로, argv_len + 1)
		arg_address[i] = _if->rsp; // arg_address 배열에 현재 문자열 시작 주소 위치 저장
    }

	// 2) word-align 패딩 삽입
	// 각 문자열을 삽입하고, 8바이트 단위로 정렬하기 위해 필요한 만큼 패딩을 추가한다.
    while(_if->rsp % 8 != 0) { // _if->rsp 주소값을 8로 나눴을 때 나머지가 0일 때까지 반복문 수행
        _if->rsp--; // _if->rsp -1 이동
        *(uint8_t *)(_if->rsp) = 0; // _if.rsp가 가리키는 내용물을 0으로 채움(1바이트)
	}

	// 3) 각 인자 문자열의 주소 삽입
	// 인자 문자열 삽입하면서 argv에 담아둔 각 문자열의 주소를 삽입한다.
	for (int i = argc; i >= 0; i--) { // 
        _if->rsp -= 8; // _if->rsp를 8 내림
        if (i == argc) // i값이 argc값과 같으면
            memset(_if->rsp, 0, 8); // _if->rsp에 0을 추가(sentinel 같은 느낌?)
        else 
            memcpy(_if->rsp, &arg_address[i], 8); // 나머지에는 arg_address 안에 들어있는 각 문자열의 주소를 스택에 삽입
    }
    
	// 4) return address 삽입
	// 다음 인스트럭션의 주소를 삽입해야 하는데, 지금은 프로세스를 생성하는 거라서 반환 주소가 없기 때문에 fake return address로 0을 추가한다.
    _if->rsp -= 8; // _if->rsp를 8 내림
	memset(_if -> rsp, 0, 8); // _if->rsp를 return address로 0을 추가  

	// 5) 인자의 개수와 argv 시작 주소를 각각 rdi와 rsi에 저장
	_if->R.rdi = argc; // rdi에 인자의 개수 저장
    _if->R.rsi = _if->rsp + 8; // 스택에 마지막에 추가한 fake address를 담기 직전의 주소가 argv에 시작 주소로 설정되어 있으므로, rsi에 현재 스택 포인터 rsp에 8만큼 더한 값 저장
}
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */
struct lazy_load_arg
{
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};
static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)aux;
	file_seek(lazy_load_arg->file, lazy_load_arg->ofs);
	if(file_read(lazy_load_arg->file, page->frame->kva, lazy_load_arg->read_bytes) !=
	(int)(lazy_load_arg->read_bytes))
	{
		palloc_free_page(page->frame->kva);
		return false;
	}
	memset(page->frame->kva + lazy_load_arg->read_bytes, 0, lazy_load_arg->zero_bytes);
	// free(lazy_load_arg); // 어디서 반환?
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		// void *aux = NULL;
		struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));
		lazy_load_arg->file = file;
		lazy_load_arg->ofs = ofs;
		lazy_load_arg->read_bytes = page_read_bytes;
		lazy_load_arg->zero_bytes = page_zero_bytes;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, lazy_load_arg))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	if (vm_alloc_page_with_initializer(VM_ANON, stack_bottom, 1, NULL, NULL))
	{
		success = vm_claim_page(stack_bottom);
		if (success)
			if_->rsp = USER_STACK;
	}
	return success;
}
#endif /* VM */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
/* 자식이 로드될 때까지 대기하기 위해서 방금 생성한 자식 스레드를 자식 리스트에서 검색하는 함수 */
struct
thread *get_child_process (int pid) {
	struct thread *cur = thread_current (); // 현재 스레드 저장
	struct list *child_list = &cur->child_list; // 현재 스레드가 있는 자식 리스트 저장

	// 자식 리스트에서 순차적으로 새로 생성한 스레드 검색
	for (struct list_elem *e = list_begin (child_list); e != list_end (child_list); e = list_next (e)) {
		struct thread *t = list_entry(e, struct thread, child_elem); // list_entry() 함수를 이용하여, 새로 생성된 정확한 스레드 검색

		// 검색한 스레드의 id가 새로 생성한 스레드의 id와 같은 경우 해당 스레드 반환(해당 스레드가 새로 생성한 스레드)
		if (t->tid == pid)
			return t;
	}

	// 자식 리스트에 새로 생성한 스레드가 존재하지 않는 경우 NULL 반환
	return NULL;
}
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
