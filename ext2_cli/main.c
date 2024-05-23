#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include "../include/ext2.c"
#include "../include/crc.c"
int _getch(void);
char dev_name_buf[256];
char *dev_name;
void get_s(char *str,int n)
{
	int c;
	while(n>1)
	{
		c=getchar();
		if(c=='\n')
		{
			break;
		}
		*str=c;
		++str;
		--n;
	}
	*str=0;
}
void fatal_error(char *str)
{
	printf("Fatal error: %s\nPlease press any key to continue.\n ",str);
	getch();
	exit(1);
}
void *xmalloc(long size)
{
	void *ptr;
	ptr=malloc(size);
	if(ptr==NULL)
	{
		fatal_error("Cannot allocate memory");
	}
	return ptr;
}
int bitmap_get(void *bitmap,int n)
{
	return ((unsigned char *)bitmap)[n>>3]&1<<(n&7);
}
void bitmap_set(void *bitmap,int n)
{
	((unsigned char *)bitmap)[n>>3]|=1<<(n&7);
}
void bitmap_clr(void *bitmap,int n)
{
	((unsigned char *)bitmap)[n>>3]&=~(1<<(n&7));
}
int bitmap_find(void *bitmap,int bytes)
{
	unsigned int c;
	int n;
	n=0;
	while(bytes)
	{
		c=*(unsigned char *)bitmap;
		if(c!=0xff)
		{
			while(c&1)
			{
				c>>=1;
				++n;
			}
			return n;
		}
		--bytes;
		n+=8;
		bitmap=(char *)bitmap+1;
	}
	return -1;
}
#include "ext2fs.c"
unsigned int current_dir;
#include "cmd.c"
int main(void)
{
	printf("Please input device name (for example, \"\\\\.\\D:\"): ");
	get_s(dev_name_buf,255);
	dev_name=dev_name_buf;
	ext2fs_init(dev_name);
	current_dir=2;
	printf("NOTE!!! You should always close this program by entering \"exit\".\nClosing this program improperly may cause data corruption.\n");
	printf("Please press any key to continue.\n");
	getch();
	printf("Type \"help\" for available commands.\n");
	while(cmd_run());
	printf("Writing changes to drive, please wait.\n");
	ext2_sync();
	CloseHandle(dev_handle);
	return 0;
}