#include "../include/windows.c"
#include "../include/windef.c"
#include "../include/mem.c"
#include "../include/iformat.c"
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
	puts("Fatal Error: ");
	puts(str);
	puts("\nPlease press any key to continue.\n");
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
int main(int argc,char **argv)
{
	if(argc<2)
	{
		puts("Please input device name (for example, \"\\\\.\\D:\"): ");
		get_s(dev_name_buf,255);
		dev_name=dev_name_buf;
	}
	else
	{
		dev_name=argv[1];
	}
	ext2fs_init(dev_name);
	current_dir=2;
	puts("NOTE!!! You should always close this program by entering \"exit\".\nClosing this program improperly may cause data corruption.\n");
	puts("Please press any key to continue.\n");
	getch();
	puts("Type \"help\" for available commands.\n");
	while(cmd_run());
	puts("Writing changes to drive, please wait.\n");
	ext2_sync();
	CloseHandle(dev_handle);
	return 0;
}
