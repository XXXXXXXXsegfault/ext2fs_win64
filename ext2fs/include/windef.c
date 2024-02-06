#define INFINITE 0xffffffff
#define NULL ((void *)0)
#define INVALID_HANDLE_VALUE ((void *)0xffffffffffffffff)
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x80
#define IOCTL_DISK_GET_PARTITION_INFO_EX 0x70048
#define FILE_BEGIN 0
struct win32_find_data_a
{
	unsigned int attr;
	unsigned int ctime[2];
	unsigned int atime[2];
	unsigned int wtime[2];
	unsigned int size_hi;
	unsigned int size_lo;
	unsigned int rsv[2];
	char name[260];
	char rsv2[36];
};