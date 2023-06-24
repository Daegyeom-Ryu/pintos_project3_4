#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

bool lazy_load_segment(struct page *page, void *aux);
tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
void argument_stack (char **argv, int argc, struct intr_frame *_if); // parsing한 arguments를 user stack에 넣어주는 함수 선언
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
struct thread *get_child_process (int pid); // 자식이 로드될 때까지 대기하기 위해서 방금 생성한 자식 스레드를 자식 리스트에서 검색하는 함수 선언
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

struct lazy_load_arg
{
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};
#endif /* userprog/process.h */
