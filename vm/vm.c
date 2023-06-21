/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"



/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */

/* 커널이 새로운 페이지 요청을 받으면 호출하는 함수 */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	
		// 1. spt에 page가 있는지 확인하고 없으면
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *p = (struct page *)malloc(sizeof(struct page));
		bool (*page_initializer)(struct page *, enum vm_type, void *);
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			page_initializer = anon_initializer;
			break;
		case VM_FILE:
			page_initializer = file_backed_initializer;
			break;
		}
		// 2. uninit_new: 페이지 구조체 생성 후 uninit type으로 만들고, 페이지 type에 맞는 초기화 함수를 담아둠
		uninit_new(p, upage, init, type, aux, page_initializer);
		p->writable = writable;
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, p);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	// va값을 받아서 VPN으로 변환 후, 이를 요소로 갖는 (page->va) dummy page 만들기
	page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;
	// va에 해당하는 hash_elem 찾기
	page->va = pg_round_down(va);						// page의 시작 주소 (hash_find -> find_elem -> page_less에서 page->va를 쓰므로 필요함)
	e = hash_find (&spt->spt_hash, &page->hash_elem);	// va에 해당하는 hash_elem 찾기
	free(page);
	// 있으면 e에 해당하는 페이지 반환	
	return e != NULL ? hash_entry(e,struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	// int succ = false;
	/* TODO: Fill this function. */
	return hash_insert (&spt->spt_hash, &page->hash_elem)==NULL ? true : false;	// hash_insert가 NULL을 반환했다는건(spt에서 기존에 삽입되지 않음), 삽입 성공 의미		
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	void *kva = palloc_get_page(PAL_USER);					// availiable page(frame)를 kernel의 USER_POOL에서 가져오고 해당 커널 가상 주소 반환
	if (kva==NULL)											// USER_POOL memory가 꽉 차서 이용할 수 없다면 frame 축출해서 그 frame 가져옴
	{
		PANIC("todo");
		// struct frame *victim = vm_evict_frame();
		// victim->page = NULL;
		// return victim;
	}
	frame = (struct frame *)malloc(sizeof(struct frame));	// frame 할당
	frame->kva=kva;											// 프레임 멤버 초기화?

	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
/* Lazy Loading 디자인 적용 : 메모리 로딩을 필요한 시점까지 미루는 디자인 패턴
   page fault가 발생했을 때만 page를 frame에 올린다. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// 1. 유효한 주소가 아니면 page fault가 아님
	if (addr == NULL)
		return false;
	if (is_kernel_vaddr(addr))
		return false;
	// 2. page가 frame에 적재되지 않았으면(not_present=1) 
	if (not_present)
	{
		// 3. spt에서 page를 찾음
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;
		// 4. read-only 페이지에 write 요청할 경우
		if(write==1 && page->writable ==0)
			return false;
		return vm_do_claim_page(page);
	}
	// 5. 물리 프레임이 할당되어 있지만 page fault가 일어난 경우(read-only page에 write)
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* 가상 주소에 해당하는 page를 찾아서 frame과의 매핑을 요청한다 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt,va);
	if(page==NULL)
		return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* frame을 가져와서 page와 매핑한다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* page와 frame을 매핑하고 페이지 테이블 엔트리로 넣고 swap_in */
	struct thread *current=thread_current();
	pml4_set_page(current->pml4, page->va,frame->kva,page->writable);
	/* 첫 page fault 시(할당된 frame X, 당연히 내용도 X)에는 page->operations->swap_in에 담긴 uninit_initialize 호출 -> lazy_loading */
	/* page fault 시 page->operations->swap_in에 담긴 anon_swap_in | file_backed_swap_in 호출 */
	return swap_in (page, frame->kva);	
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) 
{
	// src의 spt의 항목을 순회
	struct hash_iterator i;
	hash_first(&i, &src->spt_hash);
	while (hash_next(&i))
	{
		// src_page 정보 
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = src_page->operations->type;
		void *upage = src_page->va;
		bool writable = src_page->writable;

		/* 1) type이 uninit이면 */
		if (type == VM_UNINIT)
		{ // uninit page 생성 & 초기화
			vm_initializer *init = src_page->uninit.init;
			void *aux = src_page->uninit.aux;
			vm_alloc_page_with_initializer(VM_ANON, upage, writable, init, aux);
			continue;
		}

		/* 2) type이 uninit이 아니면 */
		if (!vm_alloc_page_with_initializer(type, upage, writable, NULL, NULL)) // uninit page 생성 & 초기화
			// init(lazy_load_segment)는 page_fault가 발생할때 호출됨
			// 지금 만드는 페이지는 page_fault가 일어날 때까지 기다리지 않고 바로 내용을 넣어줘야 하므로 필요 없음
			return false;

		// vm_claim_page으로 요청해서 매핑 & 페이지 타입에 맞게 초기화
		if (!vm_claim_page(upage))
			return false;

		// 매핑된 프레임에 내용 로딩
		struct page *dst_page = spt_find_page(dst, upage);
		memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
	}
	return true;
}
void hash_page_destroy(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	destroy(page);
	free(page);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->spt_hash, hash_page_destroy); // 해시 테이블에서 모든 요소를 제거
}

/* hashed index로 변환하는 함수 */
unsigned 
page_hash(const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes (&p->va, sizeof(p->va));
}
/* hash table 내 두 페이지 요소의 가상주소값을 비교해서 작은 주소값을 반환하는 함수 */
bool 
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED){
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);
	return a->va < b->va;
}
