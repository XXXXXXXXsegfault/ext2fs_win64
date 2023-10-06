asm ".cui"
asm ".entry"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "call @__init"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int __getmainargs(int *argc,char ***argv);
asm "@__getmainargs"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "pushq $0"
asm "mov 16(%rbp),%rcx"
asm "mov 24(%rbp),%rdx"
asm "xor %r8d,%r8d"
asm "mov $1,%r9d"
asm "push %r8"
asm "lea 8(%rsp),%r8"
asm "push %r8"
asm "push %r9"
asm "lea 16(%rsp),%r8"
asm "push %r8"
asm "push %rdx"
asm "push %rcx"
asm ".dllcall \"msvcrt.dll\" \"__getmainargs\""
asm "add $48,%rsp"
asm "add $256,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int _main(int argc,char **argv,void *hInstance);
asm "@_main"
asm "jmp @main"
int __init(void)
{
	int argc;
	char **argv;
	if(__getmainargs(&argc,&argv))
	{
		return -1;
	}
	return _main(argc,argv,(void *)0x400000);
}
void exit(int code);
asm "@exit"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "sub $8,%rsp"
asm "push %rcx"
asm ".dllcall \"msvcrt.dll\" \"exit\""
asm "add $16,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
void puts(char *str);
asm "@puts"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rdx"
asm "mov $@__puts__format,%rcx"
asm "push %r9"
asm "push %r8"
asm "push %rdx"
asm "push %rcx"
asm ".dllcall \"msvcrt.dll\" \"printf\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
asm "@__puts__format"
asm ".string \"%s\""
int getchar(void);
asm "@getchar"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "sub $32,%rsp"
asm ".dllcall \"msvcrt.dll\" \"getchar\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
void *CreateFileA(char *name,unsigned int access,unsigned int share,void *sa,unsigned int cd,unsigned int flags,void *template);
asm "@CreateFileA"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "mov 24(%rbp),%rdx"
asm "mov 32(%rbp),%r8"
asm "mov 40(%rbp),%r9"
asm "sub $8,%rsp"
asm "pushq 64(%rbp)"
asm "pushq 56(%rbp)"
asm "pushq 48(%rbp)"
asm "push %r9"
asm "push %r8"
asm "push %rdx"
asm "push %rcx"
asm ".dllcall \"kernel32.dll\" \"CreateFileA\""
asm "add $64,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int DeviceIoControl(void *hdev,unsigned int op,void *in,unsigned int in_size,void *out,unsigned int out_size,unsigned int *retsize,void *overlapped);
asm "@DeviceIoControl"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "mov 24(%rbp),%rdx"
asm "mov 32(%rbp),%r8"
asm "mov 40(%rbp),%r9"
asm "pushq 72(%rbp)"
asm "pushq 64(%rbp)"
asm "pushq 56(%rbp)"
asm "pushq 48(%rbp)"
asm "push %r9"
asm "push %r8"
asm "push %rdx"
asm "push %rcx"
asm ".dllcall \"kernel32.dll\" \"DeviceIoControl\""
asm "add $64,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int GetFileSizeEx(void *hf,void *size);
asm "@GetFileSizeEx"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "mov 24(%rbp),%rdx"
asm "sub $16,%rsp"
asm "push %rdx"
asm "push %rcx"
asm ".dllcall \"kernel32.dll\" \"GetFileSizeEx\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int CloseHandle(void *h);
asm "@CloseHandle"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "sub $24,%rsp"
asm "push %rcx"
asm ".dllcall \"kernel32.dll\" \"CloseHandle\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int SetFilePointerEx(void *hf,unsigned long pos,unsigned long *new_pos,unsigned int method);
asm "@SetFilePointerEx"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "mov 24(%rbp),%rdx"
asm "mov 32(%rbp),%r8"
asm "mov 40(%rbp),%r9"
asm "push %r9"
asm "push %r8"
asm "push %rdx"
asm "push %rcx"
asm ".dllcall \"kernel32.dll\" \"SetFilePointerEx\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int WriteFile(void *hf,void *buf,unsigned int size,unsigned int *retsize,void *overlapped);
asm "@WriteFile"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "mov 24(%rbp),%rdx"
asm "mov 32(%rbp),%r8"
asm "mov 40(%rbp),%r9"
asm "sub $8,%rsp"
asm "pushq 48(%rbp)"
asm "push %r9"
asm "push %r8"
asm "push %rdx"
asm "push %rcx"
asm ".dllcall \"kernel32.dll\" \"WriteFile\""
asm "add $48,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
void *malloc(unsigned long size);
asm "@malloc"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "sub $24,%rsp"
asm "push %rcx"
asm ".dllcall \"msvcrt.dll\" \"malloc\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
void free(void *ptr);
asm "@free"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "sub $24,%rsp"
asm "push %rcx"
asm ".dllcall \"msvcrt.dll\" \"free\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int rand_s(unsigned int *num);
asm "@rand_s"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "sub $24,%rsp"
asm "push %rcx"
asm ".dllcall \"msvcrt.dll\" \"rand_s\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
long time(long *p);
asm "@time"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "mov 16(%rbp),%rcx"
asm "sub $24,%rsp"
asm "push %rcx"
asm ".dllcall \"msvcrt.dll\" \"time\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
int getch(void);
asm "@getch"
asm "push %rbp"
asm "mov %rsp,%rbp"
asm "and $0xf0,%spl"
asm "push %r8"
asm "push %r9"
asm "push %r10"
asm "push %r11"
asm "sub $32,%rsp"
asm ".dllcall \"msvcrt.dll\" \"_getch\""
asm "add $32,%rsp"
asm "pop %r11"
asm "pop %r10"
asm "pop %r9"
asm "pop %r8"
asm "mov %rbp,%rsp"
asm "pop %rbp"
asm "ret"
