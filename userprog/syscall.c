#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
#include "include/threads/palloc.h" // EXEC 시스템 콜 함수에서 PAL_ZERO를 사용하기 위한 헤더 추가
#include "include/userprog/process.h" // EXEC 시스템 콜 함수에서 process_exec() 함수를 호출하기 위한 헤더 추가
#include "include/filesys/filesys.h" // 파일 디스크립터 시스템 콜 함수에서 filesys 관련 함수를 호출하기 위한 헤더 추가
#include "include/filesys/file.h" // 파일 디스크립터 시스템 콜 함수에서 file 관련 함수를 호출하기 위한 헤더 추가
#include <stdbool.h> // bool 타입을 사용하기 위한 헤더 추가

void check_address (void *addr); // 주소 값이 유저 영역에서 사용하는 주소 값인지 확인 하는 함수 선언
int add_file_to_fdt (struct file *file); // 현재 프로세스의 파일 디스크립터 테이블에 파일을 추가하는 함수 선언
static struct file *find_file_by_fd (int fd); // fd로 파일을 찾는 함수 선언
void remove_file_from_fdt (int fd); // 파일 디스크립터 테이블에서 현재 스레드를 제거하는 함수 선언
void halt (void); // 핀토스 종료 시스템 콜 함수 선언
void exit (int status); // 현재 프로세스를 종료시키는 시스템 콜 함수 선언
tid_t fork (const char *thread_name, struct intr_frame *f); // 자식 프로세스를 복제하고 실행시키는 시스템 콜 함수 선언
int exec (const char *cmd_line); // 현재 프로세스를 새로운 프로세스로 덮어 씌워 실행하는 시스템 콜 함수 선언
int wait (int pid); // 자식 프로세스가 종료될 때까지 대기하고, 정상적으로 종료되었는지 상태를 확인하는 시스템 콜 함수 선언
bool create (const char *file, unsigned initial_size); // 파일을 생성하는 시스템 콜 함수 선언
bool remove (const char *file); // 파일을 삭제하는 시스템 콜 함수 선언
int open (const char *file); // 파일을 오픈하는 시스템 콜 함수 선언
int filesize (int fd); // 파일 크기를 알려주는 시스템 콜 함수 선언
int read (int fd, void *buffer, unsigned size); // 열린 파일의 데이터를 읽는 시스템 콜 함수 선언
int write (int fd, const void *buffer, unsigned size); // 열린 파일의 데이터를 기록하는 시스템 콜 함수 선언
void seek (int fd, unsigned position); // 열린 파일의 위치(offset)를 이동하는 시스템 콜 함수 선언
unsigned tell (int fd); // 열린 파일의 위치(offset)를 알려주는 시스템 콜 함수 선언
void close (int fd);  // 열린 파일을 닫는 시스템 콜 함수 선언
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);
}

/* The main system call interface */
/* 시스템 콜 인터페이스 메인 함수 */
void
syscall_handler (struct intr_frame *f UNUSED) {
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
	int system_call_number = f->R.rax; // 호출한 시스템 콜 번호를 저장하는 변수 선언
	switch(system_call_number) {
		case SYS_HALT :
			halt ();
			break;
		case SYS_EXIT :
			exit (f->R.rdi);
			break;
		case SYS_FORK :
			f->R.rax = fork (f->R.rdi, f);
			break;
		case SYS_EXEC :
			f->R.rax = exec(f->R.rdi);
			break;
		case SYS_WAIT :
			f->R.rax = wait (f->R.rdi);
			break;
		case SYS_CREATE :
			f->R.rax = create (f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE :
			f->R.rax = remove (f->R.rdi);
			break;
		case SYS_OPEN :
			f->R.rax = open (f->R.rdi);
			break;
		case SYS_FILESIZE :
			f->R.rax = filesize (f->R.rdi);
			break;
		case SYS_READ :
			f->R.rax = read (f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE :
			f->R.rax = write (f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK :
			seek (f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL :
			f->R.rax = tell (f->R.rdi);
			break;
		case SYS_CLOSE :
			close (f->R.rdi);
			break;
		// default :
		// 	exit (-1);
		// 	break;
	}

	// printf ("system call!\n");
	// thread_exit ();
}

/* 주소 값이 유저 영역에서 사용하는 주소 값인지 확인 하는 함수 */
/* 주소유효성 겸사 : 포인터가 가리키는 주소가 사용자 영역(0x8048000~0xc0000000)인지 확인 */
/* 유저 영역을 벗어난 영역인 경우 프로세스 종료(exit(-1)) */
void
check_address (void *addr) {
	// 현재 접근하는 메모리 주소가 NULL이거나, 커널 영역에서 사용하는 주소이거나, 유저 영역에서 사용하는 주소이지만 페이지로 할당되지 않은 주소일 경우(=잘못된 접근)
	if (addr == NULL){
		exit(-1); // 프로세스 종료
	}
	if (!is_user_vaddr(addr)){
		exit(-1);
	}
	//if (pml4_get_page (thread_current ()->pml4, addr) == NULL)
	//	exit(-1);
}

/* 현재 프로세스의 파일 디스크립터 테이블에 파일을 추가하는 함수 */
int
add_file_to_fdt (struct file *file) {
	struct thread *cur = thread_current (); // 현재 스레드 저장
	struct file **fdt = cur->fdt; // 현재 스레드의 파일 디스크립터 테이블 저장

	// FD_COUNT_LIMIT 범위를 넘지 않는 범위 안에서 빈 자리 탐색
	while (cur->fd_idx < FD_COUNT_LIMIT && fdt[cur->fd_idx]) {
		cur->fd_idx++;
	}

	// 파일 디스크립터 테이블이 가득차서 할당에 실패한 경우 -1 반환
	if (cur->fd_idx >= FD_COUNT_LIMIT)
		return -1;

	// 할당 가능한 파일 디스크립터 테이블 인덱스 위치를 찾은 경우, 해당 자리에 파일을 할당하고 현재 스레드의 파일 디스크립터 테이블 인덱스 반환
	fdt[cur->fd_idx] = file;
	return cur->fd_idx;
}

/* fd로 파일을 찾는 함수 */
static
struct file *find_file_by_fd (int fd) {
	struct thread *cur = thread_current (); // 현재 스레드 구조체 저장

	// fd가 0보다 작거나, FD_COUNT_LIMIT를 넘는 경우 NULL 반환
	if (fd < 0 || fd >= FD_COUNT_LIMIT) {
		return NULL;
	}

	return cur->fdt[fd]; // 현재 스레드에 해당하는 fd를 찾은 경우 해당 fd 반환
}

/* 파일 디스크립터 테이블에서 현재 스레드를 제거하는 함수 */
void
remove_file_from_fdt (int fd) {
	struct thread *cur = thread_current (); // 현재 스레드 저장

	// 파일 디스크립터 테이블에서 0보다 작지 않고, 인덱스 제한 값보다 같거나 큰 경우 리턴
	if (fd < 0 || fd >= FD_COUNT_LIMIT) {
		return;
	}

	cur->fdt[fd] = NULL; // 현재 스레드를 찾은 경우 현재 스레드 파일 디스크립터 테이블에 NULL 할당
}

/* 핀토스 종료 시스템 콜 함수 */
void
halt (void) {
	power_off (); // HALT 시스템 콜 호출 시, init.c의 power_off() 함수로 핀토스를 종료
}

/* 현재 프로세스를 종료시키는 시스템 콜 함수 */
void
exit (int status) {
	struct thread *cur = thread_current (); // 실행중인 현재 스레드 구조체를 curr에 저장
	cur->exit_status = status; // 현재 스레드 종료 상태 저장(0이면 정상 종료)
	printf("%s: exit(%d)\n", thread_name (), status); // 프로세스 종료 메시지 출력
	thread_exit (); // 스레드 종료
}

/* 자식 프로세스를 복제하고 실행시키는 시스템 콜 함수 */
tid_t
fork (const char *thread_name, struct intr_frame *f) {
	return process_fork (thread_name, f); // 현재 프로세스를 복제하는 process_fork() 함수 호출
}

/* 현재 프로세스를 새로운 프로세스로 덮어 씌워 실행하는 시스템 콜 함수 */
int
exec (const char *file) {
	check_address (file); // 현재 가리키는 주소가 유저 영역의 주소인지 확인하여, 잘못된 주소이면 프로세스 종료
	char *fn_copy = palloc_get_page (PAL_ZERO); // 커널 풀에서 페이지를 가져와 페이지를 0으로 채우고, 사용 가능한 페이지가 없으면 NULL 포인터 반환

	// 메모리 할당 실패 시, 프로세스 종료
	if(fn_copy == NULL)
		exit (-1);
	
	strlcpy (fn_copy, file, PGSIZE); // 파일 이름 복사

	// 복사한 파일 이름을 인자로 process_exec() 함수를 호출하고, load에 실패한 경우 -1 반환
	if (process_exec (fn_copy) == -1) {
		exit(-1);
	}
}

/* 자식 프로세스가 종료될 때까지 대기하고, 정상적으로 종료되었는지 상태를 확인하는 시스템 콜 함수 */
int
wait (int pid) {
	return process_wait (pid); // 스레드 식별자 TID가 종료될 때까지 기다리고, exit status를 반환하는 process_wait() 함수 호출
}

/* 파일을 생성하는 시스템 콜 함수 */
bool
create (const char *file, unsigned initial_size) {
	check_address (file); // 현재 가리키는 주소가 유저 영역의 주소인지 확인하여, 잘못된 주소이면 프로세스 종료
	return filesys_create (file, initial_size); // 파일 이름(file)과 크기(initial_size)에 해당하는 파일 생성(성공하면 True, 실패하면 False 반환)
}

/* 파일을 삭제하는 시스템 콜 함수 */
bool 
remove (const char *file) {
	check_address (file); // 현재 가리키는 주소가 유저 영역의 주소인지 확인하여, 잘못된 주소이면 프로세스 종료
	return filesys_remove (file); // 파일 이름(file)에 해당하는 파일 삭제
}

/* 파일을 오픈하는 시스템 콜 함수 */
int
open (const char *file) {
	check_address (file); // 현재 가리키는 주소가 유저 영역의 주소인지 확인하여, 잘못된 주소이면 프로세스 종료
	struct file *open_file = filesys_open (file); // filesys_open() 함수를 이용하여 파일 오픈

	// 파일을 찾지 못하거나 내부 메모리 할당에 실패하여 파일을 열 수 없는 경우 -1 반환
	if (open_file == NULL) {
		return -1;
	}

	int fd = add_file_to_fdt (open_file); // 파일 디스크립터 테이블에 file 추가(성공하면 fd, 실패하면 -1 반환)

	// 파일 디스크립터 테이블에 추가할 수 없는 경우 파일을 닫고 -1 반환
	if (fd == -1) {
		file_close (open_file);
	}

	return fd; // fd 반환
}

/* 파일 크기를 알려주는 시스템 콜 함수 */
int
filesize (int fd) {
	struct file *open_file = find_file_by_fd (fd); // find_file_by_fd() 함수를 이용하여 파일 디스크립터 테이블에서 열려있는 파일 검색

	// 파일을 찾지 못하거나 내부 메모리 할당에 실패하여 파일을 열 수 없는 경우 -1 반환
	if (open_file == NULL) {
		return -1;
	}

	return file_length (open_file); // file_length() 함수를 이용하여 찾은 파일의 크기를 bytes 단위로 반환
}

/* 열린 파일의 데이터를 읽는 시스템 콜 함수 */
int
read (int fd, void *buffer, unsigned size) {
	check_address (buffer); // 인자로 받은 버퍼 포인터 주소 확인

    off_t read_byte;
    uint8_t *read_buffer = buffer;

	// fd가 0인 경우(=STDIN) 키보드로부터 입력을 받아오는 input_getc 함수를 호출해 size만큼 값 read
    if (fd == 0) {
        char key;
        for (read_byte = 0; read_byte < size; read_byte++) {
            key = input_getc ();
            *read_buffer++ = key;
            if (key == '\0') {
                break;
            }
        }
    }

	// fd가 1인 경우(=STDOUT) -1 반환
    else if (fd == 1)
    {
        return -1;
    }

	// fd가 2 이상인 경우(=정상 fd) find_file_by_fd() 함수를 호출해 fd에 해당하는 파일 검색
    else
    {
        struct file *read_file = find_file_by_fd (fd); // fd로 열린 파일 검색

		// 열린 파일을 찾지 못하면, -1 반환
        if (read_file == NULL)
        {
            return -1;
        }

        lock_acquire (&filesys_lock); // 열린 파일의 데이터를 읽고 버퍼에 저장하는 과정에서 다른 파일의 접근을 막기 위해 lock 획득
        read_byte = file_read (read_file, buffer, size); // 파일에서 현재 위치부터 size 바이트 만큼 데이터를 읽어서 버퍼에 저장하는 file_read() 함수 호출
        lock_release (&filesys_lock); // 열린 파일의 데이터를 읽고 버퍼에 저장을 완료하면 lock 해제
    }
    return read_byte;
}

/* 열린 파일의 데이터를 기록하는 시스템 콜 함수 */
int
write (int fd, const void *buffer, unsigned size) {
	check_address (buffer); // 인자로 받은 버퍼 포인터 주소 확인

	int bytes_write = 0;

	// fd가 1인 경우(=STDOUT) 버퍼에 저장된 데이터를 화면에 출력하는 putbuf() 함수를 호출
	if (fd == 1) {
		putbuf (buffer, size);
		bytes_write = size;
	}

	else {
		// fdrk 0인 경우(STDIN) -1 반환
		if (fd < 2) {
			return -1;
		}

		struct file *file = find_file_by_fd (fd); // fd로 열린 파일 검색

		// 열린 파일을 찾지 못 한 경우 -1 반환
		if (file == NULL) {
			return -1;
		}

		lock_acquire (&filesys_lock); // 열린 파일의 데이터를 기록하는 과정에서 다른 파일의 접근을 막기 위해 lock 획득
		bytes_write = file_write (file, buffer, size); // 파일에서 현재 위치부터 size 바이트 만큼 버퍼에 있는 데이터를 기록
		lock_release (&filesys_lock); // 열린 파일의 데이터 기록을 완료하면 lock 해제
	}
	return bytes_write;
}

/* 열린 파일의 위치(offset)를 이동하는 시스템 콜 함수 */
void
seek (int fd, unsigned position) {
	struct file *seek_file = find_file_by_fd (fd); // fd를 이용하여 열린 파일 검색

	// 찾은 파일의 fd값이 2보다 작은 경우 리턴
	if (seek_file <= 2) {
		return;
	}

	return file_seek (seek_file, position); // 열린 파일을 찾았다면, file_seek() 함수를 이용하여 열린 파일의 위치를 position만큼 이동
}

/* 열린 파일의 위치(offset)를 알려주는 시스템 콜 함수 */
unsigned
tell (int fd) {
	struct file *tell_file = find_file_by_fd (fd); // fd를 이용하여 열린 파일 검색

	// 찾은 파일의 fd값이 2보다 작은 경우 리턴
	if (tell_file <= 2) {
		return;
	}

	return file_tell (tell_file); // 열린 파일을 찾았다면, file_tell() 함수를 이용하여 파일의 위치를 반환
}

/* 열린 파일을 닫는 시스템 콜 함수 */
void
close (int fd) {
	struct file *close_file = find_file_by_fd (fd); // fd를 이용하여 열린 파일 검색

	// 파일을 찾는데 실패한 경우 리턴
	if (close_file == NULL) {
		return;
	}

	file_close (close_file); // file_close()로 파일 닫기
	remove_file_from_fdt (fd); // remove_file_from_fdt() 함수를 이용하여 닫은 파일 삭제
}
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
