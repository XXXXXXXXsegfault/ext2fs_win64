#include "../include/windows_mini.c"
#include "../include/windef.c"
#include "../include/mem.c"
#include "../include/iformat.c"
#include "../include/ext2.c"
void *dev_handle;
unsigned char buf[131072];
unsigned int buf_x;
struct ext2_superblock sb;
struct ext2_bgdt *gdt;
unsigned long groups,gt_blocks;
unsigned long size,blocks;
unsigned int last_group_size;
unsigned char block_bitmap[4096];
unsigned char inode_bitmap[4096];
void fatal_error(char *str)
{
	puts("\nFatal Error: ");
	puts(str);
	exit(1);
}
long dev_size(void)
{
	unsigned long buf[128];
	if(DeviceIoControl(dev_handle,IOCTL_DISK_GET_PARTITION_INFO_EX,NULL,0,buf,sizeof(buf),NULL,NULL))
	{
		return buf[2];
	}
	if(GetFileSizeEx(dev_handle,buf))
	{
		return buf[0];
	}
	return -1;
}
void fill_rand(unsigned char *buf)
{
	int n;
	n=16;
	do
	{
		while(rand_s(buf));
		n-=4;
		buf+=4;
	}
	while(n);
}
int init_fs_info(void)
{
	unsigned long x;
	unsigned int nb[3];
	int has_super;
	sb.block_size=2;
	sb.frag_size=2;
	sb.blocks_per_group=32768;
	sb.frags_per_group=32768;
	sb.inodes_per_group=8192;
	sb.max_mounts=0xffff;
	sb.magic=0xef53;
	sb.state=1;
	sb.errors=1;
	sb.rev=1;
	sb.first_ino=11;
	sb.inode_size=256;
	sb.feature_incompat=2;
	sb.feature_ro_compat=3;
	fill_rand(sb.uuid);
	size=dev_size();
	if(size==-1)
	{
		fatal_error("Cannot determine device size");
	}
	blocks=size>>12;
	if(blocks==0)
	{
		fatal_error("Device too small");
	}
	last_group_size=blocks&32767;
	if(last_group_size==0)
	{
		last_group_size=32768;
	}
	groups=blocks+32767>>15;
	if(groups==0)
	{
		fatal_error("Device too small");
	}
	gt_blocks=groups+127>>7;
	x=1;
	nb[0]=1;
	nb[1]=5;
	nb[2]=7;
	has_super=1;
	while(x<groups)
	{
		if(x==nb[0])
		{
			has_super=1;
			nb[0]*=3;
		}
		else if(x==nb[1])
		{
			has_super=1;
			nb[1]*=5;
		}
		else if(x==nb[2])
		{
			has_super=1;
			nb[2]*=7;
		}
		else
		{
			has_super=0;
		}
		++x;
	}
	if(has_super)
	{
		if(gt_blocks+(1+1+1+512+1)>last_group_size)
		{
			blocks-=last_group_size;
			last_group_size=32768;
			--groups;
		}
	}
	else
	{
		if(1+1+512+1>last_group_size)
		{
			blocks-=last_group_size;
			last_group_size=32768;
			--groups;
		}
	}
	if(groups==0)
	{
		fatal_error("Device too small");
	}
	if(blocks>0xffffffff)
	{
		fatal_error("Device too large");
	}
	gt_blocks=groups+127>>7;
	gdt=malloc(gt_blocks<<12);
	if(gdt==NULL)
	{
		fatal_error("Cannot allocate memory");
	}
	memset(gdt,0,gt_blocks<<12);

	sb.blocks=blocks;
	sb.inodes=groups*8192;
	sb.free_inodes=sb.inodes-10;

	gdt[0].free_inodes=8192-10;
	gdt[0].block_bitmap=1+gt_blocks;
	gdt[0].inode_bitmap=1+gt_blocks+1;
	gdt[0].inode_table=1+gt_blocks+1+1;
	gdt[0].used_dirs=1;

	if(groups==1)
	{
		gdt[0].free_blocks=last_group_size-(1+gt_blocks+1+1+512+1);
	}
	else
	{
		gdt[0].free_blocks=32768-(1+gt_blocks+1+1+512+1);
	}
	sb.free_blocks=gdt[0].free_blocks;

	x=1;
	nb[0]=1;
	nb[1]=5;
	nb[2]=7;
	while(x<groups)
	{
		if(x==nb[0])
		{
			has_super=1;
			nb[0]*=3;
		}
		else if(x==nb[1])
		{
			has_super=1;
			nb[1]*=5;
		}
		else if(x==nb[2])
		{
			has_super=1;
			nb[2]*=7;
		}
		else
		{
			has_super=0;
		}
		gdt[x].free_inodes=8192;
		if(has_super)
		{
			gdt[x].block_bitmap=(x<<15)+1+gt_blocks;
			if(x==groups-1)
			{
				gdt[x].free_blocks=last_group_size-(1+gt_blocks+1+1+512);
			}
			else
			{
				gdt[x].free_blocks=32768-(1+gt_blocks+1+1+512);
			}
		}
		else
		{
			gdt[x].block_bitmap=x<<15;
			if(x==groups-1)
			{
				gdt[x].free_blocks=last_group_size-(1+1+512);
			}
			else
			{
				gdt[x].free_blocks=32768-(1+1+512);
			}
		}
		gdt[x].inode_bitmap=gdt[x].block_bitmap+1;
		gdt[x].inode_table=gdt[x].inode_bitmap+1;
		sb.free_blocks+=gdt[x].free_blocks;
		++x;
	}
	sb.r_blocks=sb.free_blocks/128;
	memset(inode_bitmap+1024,0xff,3072);
	return 0;
}
void bitmap_set(unsigned char *bitmap,int n)
{
	int x;
	x=0;
	while(n>=8)
	{
		bitmap[x]=0xff;
		n-=8;
		++x;
	}
	bitmap[x]|=(1<<n)-1;
}
void bitmap_set2(unsigned char *bitmap,int n)
{
	int x;
	x=4096;
	while(n>=8)
	{
		--x;
		bitmap[x]=0xff;
		n-=8;
	}
	--x;
	bitmap[x]|=0x100-(1<<8-n);
}
void buf_write(void *ptr,unsigned long int size)
{
	unsigned int n;
	while(size)
	{
		if(size>=131072-buf_x)
		{
			memcpy(buf+buf_x,ptr,131072-buf_x);
			size-=131072-buf_x;
			ptr=(char *)ptr+131072-buf_x;
			buf_x=0;
			n=0;
			WriteFile(dev_handle,buf,131072,&n,NULL);
			if(n!=131072)
			{
				fatal_error("I/O Error");
			}
		}
		else
		{
			memcpy(buf+buf_x,ptr,size);
			buf_x+=size;
			ptr=(char *)ptr+size;
			size=0;
		}
	}
}
void buf_write_zero(unsigned long int size)
{
	unsigned int n;
	while(size)
	{
		if(size>=131072-buf_x)
		{
			memset(buf+buf_x,0,131072-buf_x);
			size-=131072-buf_x;
			buf_x=0;
			n=0;
			WriteFile(dev_handle,buf,131072,&n,NULL);
			if(n!=131072)
			{
				fatal_error("I/O Error");
			}
		}
		else
		{
			memset(buf+buf_x,0,size);
			buf_x+=size;
			size=0;
		}
	}
}
void buf_flush(void)
{
	unsigned int n;
	if(buf_x)
	{
		n=0;
		WriteFile(dev_handle,buf,buf_x,&n,NULL);
		if(n!=buf_x)
		{
			fatal_error("I/O Error");
		}
		buf_x=0;
	}
}

void write_first_group(void)
{
	struct ext2_inode inode;
	unsigned char buf[12];
	struct ext2_directory *dir;
	unsigned long ctime,ctime_extra;
	SetFilePointerEx(dev_handle,1024,NULL,FILE_BEGIN);
	buf_write(&sb,1024);
	buf_write_zero(2048);
	buf_write(gdt,gt_blocks*4096);
	bitmap_set(block_bitmap,1+gt_blocks+1+1+512+1);
	if(groups==1)
	{
		bitmap_set2(block_bitmap,32768-last_group_size);
	}
	bitmap_set(inode_bitmap,10);
	buf_write(block_bitmap,4096);
	buf_write(inode_bitmap,4096);
	buf_write_zero(256);
	memset(&inode,0,256);
	inode.mode=040755;
	inode.links=2;
	inode.size=4096;
	inode.blocks=8;
	inode.block[0]=1+gt_blocks+1+1+512;
	ctime=time(NULL);
	ctime_extra=ctime>>32&3;
	ctime&=0xffffffff;
	inode.atime=ctime;
	inode.ctime=ctime;
	inode.mtime=ctime;
	inode.crtime=ctime;
	inode.atime_extra=ctime_extra;
	inode.ctime_extra=ctime_extra;
	inode.mtime_extra=ctime_extra;
	inode.crtime_extra=ctime_extra;
	buf_write(&inode,256);
	buf_write_zero(256*8190);
	memset(buf,0,12);
	dir=(void *)buf;
	dir->inode=2;
	dir->rec_len=12;
	dir->name_len=1;
	dir->file_type=2;
	dir->file_name[0]='.';
	buf_write(dir,12);
	dir->rec_len=4096-12;
	dir->name_len=2;
	dir->file_name[1]='.';
	buf_write(dir,12);
	buf_write_zero(4096-12-12);
	buf_flush();
}
void write_group(unsigned long n,int has_super)
{
	SetFilePointerEx(dev_handle,n<<27,NULL,FILE_BEGIN);
	if(has_super)
	{
		buf_write(&sb,1024);
		buf_write_zero(3072);
		buf_write(gdt,gt_blocks*4096);
	}
	memset(block_bitmap,0,4096);
	if(n==groups-1)
	{
		bitmap_set(block_bitmap,last_group_size-gdt[n].free_blocks);
		bitmap_set2(block_bitmap,32768-last_group_size);
	}
	else
	{
		bitmap_set(block_bitmap,32768-gdt[n].free_blocks);
	}
	buf_write(block_bitmap,4096);
	buf_write(inode_bitmap,4096);
	buf_write_zero(256*8192);
	buf_flush();
}
void print_progress(long groups,long progress)
{
	char buf[32];
	strcpy(buf,"\rFormatting Progress: ");
	sprinti(buf,progress*100/groups,1);
	strcat(buf,"%");
	puts(buf);
}
int main(int argc,char **argv)
{
	unsigned long x;
	unsigned int nb[3];
	if(argc<2)
	{
		puts("Format a drive to ext2\n");
		puts("Usage: ./ext2_format.exe <device>\nPlease press any key to continue.\n");
		getch();
		return 1;
	}
	dev_handle=CreateFileA(argv[1],GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(dev_handle==INVALID_HANDLE_VALUE)
	{
		fatal_error("Cannot open device");
	}
	init_fs_info();
	write_first_group();
	print_progress(groups,1);
	memset(inode_bitmap,0,1024);
	x=1;
	nb[0]=1;
	nb[1]=5;
	nb[2]=7;
	while(x<groups)
	{
		if(x==nb[0])
		{
			nb[0]*=3;
			write_group(x,1);
		}
		else if(x==nb[1])
		{
			nb[1]*=5;
			write_group(x,1);
		}
		else if(x==nb[2])
		{
			nb[2]*=7;
			write_group(x,1);
		}
		else
		{
			write_group(x,0);
		}
		++x;
		print_progress(groups,x);
	}
	CloseHandle(dev_handle);
	puts("\nSuccess\n");
	return 0;
}
