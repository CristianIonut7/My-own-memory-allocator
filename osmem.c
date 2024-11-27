// SPDX-License-Identifier: BSD-3-Clause
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "block_meta.h"
#include "osmem.h"
#define PAGE_SIZE (4 * 1024)
#define META_SIZE sizeof(struct block_meta)
#define MMAP_THRESHOLD (128 * 1024)
size_t threshold = MMAP_THRESHOLD;
struct block_meta *head;

struct block_meta *find_block(size_t size)
{
	if (head == NULL) {
		head = sbrk(MMAP_THRESHOLD);
		if (head == (void *)-1) {
			DIE(head == (void *)-1, "heap_prealloc");
			return NULL;
		}
		head->size = MMAP_THRESHOLD - META_SIZE;
		head->next = NULL;
		head->prev = NULL;
		head->status = 0;
	}
	struct block_meta *block = head;
	struct block_meta *use = NULL;
	int difmin;

	while (1) {
		while (block->next != NULL) {
			if ((block->next->status) == 0 && block->status == 0) {
				struct block_meta *n = block->next;

				block->next = n->next;
				block->size = block->size + n->size + META_SIZE;
				if (block->next)
					block->next->prev = block;
			} else {
				break;
			}
		}
		if (block->next)
			block = block->next;
		else
			break;
	}
	block = head;
	while (1) {
		int dif = (int)block->size - (((int)size + 7) / 8) * 8;

		if (block->status == 0 && dif == 0) {
			block->status = 1;
			return block;
		}
		if (block->next)
			block = block->next;
		else
			break;
	}
	block = head;
	while (1) {
		int dif = (int)(block->size) - (((int)size + 7) / 8) * 8;

		if (block->status == 0 && dif > 0) {
			if (dif > 0 && (dif < difmin || use == NULL)) {
				use = block;
				difmin = dif;
			}
		}
		if (block->next)
			block = block->next;
		else
			break;
	}
	if (use != NULL && difmin >= 40) {
		struct block_meta *split;

		block = use;
		split = (struct block_meta *)((void *)block + ((size + 7) / 8) * 8 + META_SIZE);
		split->status = 0;
		split->prev = block;
		split->next = block->next;
		size_t aux =  block->size - ((size + 7) / 8) * 8 - META_SIZE;

		block->size = ((size + 7) / 8) * 8;
		split->size = aux;
		block->next = split;
		while (split->next != NULL) {
			if ((split->next->status) == 0) {
				struct block_meta *n = split->next;

				split->next = n->next;
				split->size = split->size + n->size + META_SIZE;
				if (split->next)
					split->next->prev = split;
			} else {
				break;
			}
		}
		block->status = 1;
		return block;
	}
	if (use) {
		use->status = 1;
		return use;
	}
	if (block->status == 0 && block->next == NULL) {
		size_t aux = ((size + 7) / 8) * 8 - block->size;

		block->size = block->size + aux;
		block->status = 1;
		void *mem = sbrk(aux);

		if (mem == (void *)-1) {
			DIE(mem == (void *)-1, "etend_block");
			return NULL;
		}
		return block;
	}
	void *mem = sbrk(((size + 7) / 8) * 8 + META_SIZE);

	if (mem == (void *)-1) {
		DIE(mem == (void *)-1, "new_block");
		return NULL;
	}

	struct block_meta *new_block = (struct block_meta *)mem;

	new_block->size = ((size + 7) / 8) * 8;
	new_block->status = 1;
	block->next = new_block;
	new_block->next = NULL;
	new_block->prev = block;
	return new_block;
}
void *os_malloc(size_t size)
{
	if (size <= 0)
		return NULL;
	if (size >= threshold - META_SIZE) {
		struct block_meta *p;

		void *die = ((void *)syscall(9, NULL, ((size + 7) / 8) * 8 + META_SIZE,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

		if (die == (void *)-1) {
			DIE(die == (void *)-1, "mmap");
			return NULL;
		}
		p = (struct block_meta *)die;
		p->size = size;
		p->status = 2;
		return (void *)(p) + META_SIZE;
	}
	struct block_meta *block = find_block(size);

	return (void *)(block) + META_SIZE;
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;
	struct block_meta *block = (struct block_meta *) (ptr - META_SIZE);

	if (block->status == 2) {
		block->status = 0;
		void *die = (void *)syscall(11, ptr - META_SIZE, ((block->size + 7) / 8) * 8 + META_SIZE);

		if (die == (void *)-1) {
			DIE(die == (void *)-1, "os_malloc");
			return;
		}
	} else {
		block->status = 0;
		while (block->prev != NULL) {
			if ((block->prev->status) == 0) {
				struct block_meta *p = block->prev;

				p->next = block->next;
				p->size = p->size + block->size + META_SIZE;
				if (p->next)
					p->next->prev = p;
				block = p;
			} else {
				break;
			}
		}
		while (block->next != NULL) {
			if ((block->next->status) == 0) {
				struct block_meta *n = block->next;

				block->next = n->next;
				block->size = block->size + n->size + META_SIZE;
				if (block->next)
					block->next->prev = block;
			} else {
				break;
			}
		}
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (size <= 0 || nmemb <= 0)
		return NULL;
	threshold = PAGE_SIZE;
	size_t total = size * nmemb;
	void *p = os_malloc(total);

	threshold = MMAP_THRESHOLD;
	memset(p, 0, total);
	return p;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return os_malloc(size);
	if (size <= 0) {
		os_free(ptr);
		return NULL;
	}
	struct block_meta *block;

	block = (struct block_meta *)((void *)ptr - META_SIZE);
	struct block_meta *aux = block;

	if (block->status == 0)
		return NULL;
	if (block->status == 2) {
		if (((size + 7) / 8) * 8 < threshold - META_SIZE) {
			ptr = os_malloc(size);
			memcpy(ptr, (void *)(block + 1), ((size + 7) / 8) * 8);
			os_free((void *)(block + 1));
			return ptr;
		}
		ptr = os_malloc(size);
		memcpy(ptr, (void *)(block + 1), ((size + 7) / 8) * 8);
		os_free((void *)(block + 1));
		return ptr;
	}
	if (block->status == 1 && size >= threshold - META_SIZE) {
		ptr = os_malloc(size);
		memcpy(ptr, (void *)(block + 1), block->size);
		os_free((void *)(block + 1));
		return ptr;
	}
	if (block->next == NULL && block->size < (size + 7) / 8 * 8) {
		size_t aux = ((size + 7) / 8) * 8 - block->size;

		block->size = block->size + aux;
		block->status = 1;
		void *mem = sbrk(aux);

		if (mem == (void *)-1) {
			DIE(mem == (void *)-1, "extend_block_realloc");
			return NULL;
		}
		return (void *)(block + 1);
	}
	if (block->size >= ((size + 7) / 8) * 8) {
		if (block->size - ((size + 7) / 8) * 8  >= (size_t)(8 + META_SIZE)) {
			struct block_meta *split;

			split = (struct block_meta *)((char *)block + ((size + 7) / 8) * 8 + META_SIZE);
			split->status = 0;
			size_t aux =  block->size - ((size + 7) / 8) * 8 - META_SIZE;

			split->size = aux;
			split->prev = block;
			split->next = block->next;
			block->size = ((size + 7) / 8) * 8;
			block->next = split;
			while (split->next != NULL) {
				if ((split->next->status) == 0) {
					struct block_meta *n = split->next;

					split->next = n->next;
					split->size = split->size + n->size + META_SIZE;
				if (split->next)
					split->next->prev = split;
				} else {
					break;
				}
			}
		}
		return ptr;
	}
	while (block->next != NULL && ((size + 7) / 8) * 8 > block->size) {
		if ((block->next->status) == 0) {
			struct block_meta *n = block->next;

			block->next = n->next;
			block->size = block->size + n->size + META_SIZE;
			if (block->next)
				block->next->prev = block;
		} else {
			break;
		}
	}
	if (block->size == ((size + 7) / 8) * 8) {
		block->status = 1;
		return ptr;
	}
	if (((size + 7) / 8) * 8 < block->size) {
		if (block->size - ((size + 7) / 8) * 8  >= (size_t)(8 + META_SIZE)) {
			struct block_meta *split;

			split = (struct block_meta *)((void *)block + ((size + 7) / 8) * 8 + META_SIZE);
			split->status = 0;
			split->prev = block;
			split->next = block->next;
			size_t aux =  block->size - ((size + 7) / 8) * 8 - META_SIZE;

			split->size = aux;
			block->size = ((size + 7) / 8) * 8;
			block->next = split;
			block->status = 1;
			while (split->next != NULL) {
				if ((split->next->status) == 0) {
					struct block_meta *n = split->next;

					split->next = n->next;
					split->size = split->size + n->size + META_SIZE;
				if (split->next)
					split->next->prev = split;
				} else {
					break;
				}
			}
		}
		return ptr;
	}
	void *p = os_malloc(size);

	memcpy(p, ptr, aux->size);
	os_free(ptr);
	return p;
}
