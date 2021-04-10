#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <list.h>
#include "lib/user/syscall.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "devices/input.h"
#include <console.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/off_t.h"

struct file{
	struct inode *inode;
	off_t pos;
	bool deny_write;
};

static void syscall_handler(struct intr_frame *);
struct lock file_lock;

void get_argument(void *esp, uint32_t *arg, int count);
void halt(void);
void exit(int status);
pid_t exec(const char *file);
int wait(pid_t pid);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
void close(int fd);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned pos);
unsigned tell(int fd);
int fibonacci(int n);
int sum_of_four_int(int a, int b, int c, int d);

void
syscall_init(void)
{
	lock_init(&file_lock);
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
	void *esp = f->esp;
	uint32_t number = *(uint32_t *)f->esp;
	uint32_t arg[3];

	switch (number) {
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		get_argument(esp, arg, 1);
		exit((int)arg[0]);
		break;
	case SYS_EXEC:
		get_argument(esp, arg, 1);
		f->eax = exec((const char*)arg[0]);
		break;
	case SYS_WAIT:
		get_argument(esp, arg, 1);
		f->eax = wait((pid_t)arg[0]);
		break;
	case SYS_CREATE:
		get_argument(esp, arg, 2);
		if ((const char*)arg[0] == NULL) {
			exit(-1);
		}
		f->eax = create((const char*)arg[0], (unsigned)arg[1]);
		break;
	case SYS_REMOVE:
		get_argument(esp, arg, 1);
		if((const char*)arg[0] == NULL){
			exit(-1);
		}
		f->eax = remove((const char*)arg[0]);
		break;
	case SYS_OPEN:
		get_argument(esp, arg, 1);
		if ((const char*)arg[0] == NULL) {
			exit(-1);
		}
		f->eax = open((const char*)arg[0]);
		break;
	case SYS_CLOSE:
		get_argument(esp, arg, 1);
		close(arg[0]);
		break;
	case SYS_FILESIZE:
		get_argument(esp, arg, 1);
		f->eax = filesize(arg[0]);
		break;
	case SYS_READ:
		get_argument(esp, arg, 3);
		f->eax = read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
		break;
	case SYS_WRITE:
		get_argument(esp, arg, 3);
		f->eax = write((int)arg[0], (const void *)arg[1], (unsigned)arg[2]);
		break;
	case SYS_SEEK:
		get_argument(esp, arg, 2);
		seek(arg[0], (unsigned)arg[1]);
		break;
	case SYS_TELL:
		get_argument(esp, arg, 1);
		f->eax = tell(arg[0]);
		break;
	case SYS_FIBONACCI:
		get_argument(esp, arg, 1);
		f->eax = fibonacci((int)arg[0]);
		break;
	case SYS_SUM_OF_FOUR_INT:
		get_argument(esp, arg, 4);
		f->eax = sum_of_four_int((int)arg[0], (int)arg[1], (int)arg[2], (int)arg[3]);
		break;
	default:
		thread_exit();
		break;
	}
}

void get_argument(void *esp, uint32_t *arg, int count) {
	int i;
	for (i = 0; i<count; i++) {
		esp += 4;
		if (!is_user_vaddr(esp)) {
			exit(-1);
		}
		arg[i] = *(uint32_t *)esp;
	}
}

void halt(void) {
	shutdown_power_off();
}

void exit(int status) {
	struct thread *cur = thread_current();
	int i;

	printf("%s: exit(%d)\n", cur->name, status);
	cur->exit_status = status;
	for(i=3; i<131; i++){
		if(cur->file_list[i] != NULL){
			close(i);
		}
	}
	thread_exit();
}

pid_t exec(const char *cmd_line) {
	return process_execute(cmd_line);
}

int wait(pid_t pid) {
	return process_wait(pid);
}

int read(int fd, void *buffer, unsigned size) {
	int i, ret;
	struct thread *cur = thread_current();

	if (!is_user_vaddr(buffer)) {
		exit(-1);
	}
	lock_acquire(&file_lock);
	if (fd == 0) {
		for (i = 0; i<size; i++) {
			*((char*)buffer + i) = input_getc();
			if (*((char*)buffer + i) == '\0') {
				break;
			}
		}
		ret = i;
	}
	else if (fd >= 3) {
		if(cur->file_list[fd] == NULL){
			exit(-1);
		}
		ret = file_read(cur->file_list[fd], buffer, (off_t)size);
	}
	else {
		ret = -1;
	}
	lock_release(&file_lock);

	return ret;
}

int write(int fd, const void *buffer, unsigned size) {
	int ret;
	struct thread *cur = thread_current();

	if (!is_user_vaddr(buffer)) {
		exit(-1);
	}

	lock_acquire(&file_lock);
	if (fd == 1) {
		putbuf(buffer, size);
		ret = size;
	}
	else if (fd >= 3) {
		if(cur->file_list[fd] == NULL){
			lock_release(&file_lock);
			exit(-1);
		}
		if(cur->file_list[fd]->deny_write){
			file_deny_write(cur->file_list[fd]);
		}
		ret = file_write(cur->file_list[fd], buffer, (off_t)size);
	}
	else {
		ret = -1;
	}
	lock_release(&file_lock);

	return ret;
}

int fibonacci(int n) {
	int tmp, i;
	int first = 1, second = 1;

	if (n == 1 || n == 2) {
		printf("Result of fibonacci : 1\n");

		return 1;
	}
	else {
		for (i = 1; i <= n - 2; i++) {
			tmp = first + second;
			first = second;
			second = tmp;
		}
		printf("Result of fibonacci : %d\n", second);

		return second;
	}
}

int sum_of_four_int(int a, int b, int c, int d) {
	printf("Result of sum_of_four_int : %d\n", a + b + c + d);

	return a + b + c + d;
}

bool create(const char* file, unsigned initial_size) {
	return filesys_create(file, (off_t)initial_size);
}

bool remove(const char* file) {
	return filesys_remove(file);
}

int open(const char* file) {
	int i, ret = -1;
	struct file *f;
	struct thread *cur = thread_current();

	lock_acquire(&file_lock);
	f = filesys_open(file);

	if (f == NULL) {
		ret = -1;
	}
	else {
		for (i = 3; i < 131; i++) {
			if (cur->file_list[i] == NULL) {
				if (strcmp(file, cur->name) == 0) {
					file_deny_write(f);
				}
				cur->file_list[i] = f;
				ret = i;
				break;
			}
		}
	}
	lock_release(&file_lock);
	return ret;
}

void close(int fd) {
	struct thread *cur = thread_current();

	if (cur->file_list[fd] == NULL) {
		exit(-1);
	}
	else {
		file_close(cur->file_list[fd]);
		cur->file_list[fd] = NULL;
	}
}
int filesize(int fd) {
	struct thread *cur = thread_current();
	
	if(cur->file_list[fd] == NULL){
		exit(-1);
	}
	else{
		return file_length(cur->file_list[fd]);
	}
}

void seek(int fd, unsigned pos) {
	struct thread *cur = thread_current();

	if (cur->file_list[fd] == NULL) {
		exit(-1);
	}
	else {
		file_seek(cur->file_list[fd], pos);
	}
}

unsigned tell(int fd) {
	struct thread *cur = thread_current();

	if (cur->file_list[fd] == NULL) {
		exit(-1);
	}
	else {
		return file_tell(cur->file_list[fd]);
	}
}
