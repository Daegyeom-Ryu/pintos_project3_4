/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
/* file-backed page를 초기화한다. */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// 먼저 page->operations에 file-backed pages에 대한 핸들러를 설정한다.
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	// todo: page struct의 일부 정보(such as 메모리가 백업되는 파일과 관련된 정보)를 업데이트할 수도 있다.
	struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)page->uninit.aux;
	file_page->file = lazy_load_arg->file;
	file_page->ofs = lazy_load_arg->ofs;
	file_page->read_bytes = lazy_load_arg->read_bytes;
	// file_page->zero_bytes = lazy_load_arg->zero_bytes;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	// 관련된 파일을 닫음으로써 file-backed page 파괴한다.
	// 내용이 변경되었다면(dirty) 변경 사하을 파일에 기록해야 한다.
    if (pml4_is_dirty(thread_current()->pml4, page->va))
    {
        // file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
        // pml4_set_dirty(thread_current()->pml4, page->va, 0);
		file_write_at(page->file.file, page->va, page->file.read_bytes, page->file.ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
    }
    pml4_clear_page(thread_current()->pml4, page->va);
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	// file reopen
	struct file *f = file_reopen(file);
	// 매핑 성공시 파일이 매핑된 가상주소 반환하는데 사용
	void *start_addr = addr;
	// 매핑이 필요한 페이지 수
	int total_page_count = length <= PGSIZE ? 1 : length%PGSIZE ? length/PGSIZE + 1 : length/PGSIZE;
	// 읽어들일 byte 수와 0으로 채울 byte 수
	size_t read_bytes = file_length(f) < length ? file_length(f) : length;
	size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);	// 페이지 크기의 배수인지 확인
	ASSERT (pg_ofs (addr) == 0);						// addr 페이지 정렬 확인
	ASSERT (offset % PGSIZE == 0);						// offset 페이지 정렬 확인

	while (read_bytes > 0 || zero_bytes > 0) {
		// file에서 page_read_bytes만큼 읽고 최종 page_zero_bytes로 0으로 채움
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		// lazy loading을 위해 필요한 정보 구조체에 담음
		struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));
		lazy_load_arg->file = f;						// 내용이 담긴 파일 객체
		lazy_load_arg->ofs = offset;					// 내용이 담긴 파일 객체
		lazy_load_arg->read_bytes = page_read_bytes;	// 페이지에서 읽어야 하는 바이트 수
		lazy_load_arg->zero_bytes = page_zero_bytes;	// read_bytes만큼 읽고 공간이 남으면 채워야하는 바이트 수
		// 커널이 새로운 페이지 요청을 받으면 호출하는 함수
		// lazy_load_segment는 첫 페이지 폴트가 발생했을 때 호출되고, 인자 *aux로 lazy_load_arg 가짐
		if (!vm_alloc_page_with_initializer (VM_FILE, addr,
					writable, lazy_load_segment, lazy_load_arg))
			return NULL;

		struct page *p = spt_find_page(&thread_current()->spt, start_addr);
		p->mapped_page_count = total_page_count;

		/* Advance. */
		// 읽은 바이트와 0으로 채운 바이트를 추적하고 가상 주소 증가시킴
		read_bytes-=page_read_bytes;
		zero_bytes-=page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return start_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *p = spt_find_page(spt, addr);
	int count = p->mapped_page_count;
	for (int i = 0; i < count; i++)
	{
		if (p)
			spt_remove_page(spt, p);
			// destroy(p);
		addr += PGSIZE;
		p = spt_find_page(spt, addr);

	}
}

