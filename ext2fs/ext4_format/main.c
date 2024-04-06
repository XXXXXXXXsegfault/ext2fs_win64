#include "../include/windows_mini.c"
#include "../include/windef.c"
#include "../include/mem.c"
#include "../include/iformat.c"
#include "../include/ext2.c"
#include "../include/crc.c"
void *dev_handle;
long int partition_size,blocks,groups,gdt_blocks,free_blocks;
struct ext2_superblock sb;
struct ext4_bgdt *gdt;
unsigned char buf[131072];
unsigned int buf_x;
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
int init_fs_info(void)
{
	long i,value;
	int has_super[3];
	partition_size=dev_size();
	if(partition_size==-1)
	{
		fatal_error("Cannot determine device size");
	}
	blocks=partition_size>>12;
	if(blocks<8192)
	{
		fatal_error("Device too small");
		return 1;
	}
	if(blocks<1048576)
	{
		groups=(blocks+32768-1024)/32768;
		if(blocks%32768<1024)
		{
			blocks=groups*32768;
		}
	}
	else
	{
		groups=(blocks+32768-8192)/32768;
		if(blocks%32768<8192)
		{
			blocks=groups*32768;
		}
	}
	if(groups>262144)
	{
		fatal_error("Device too large");
	}
	gdt_blocks=(groups+63)/64;
	gdt=malloc(gdt_blocks<<12);
	if(gdt==NULL)
	{
		fatal_error("Cannot allocate memory");
	}
	memset(gdt,0,gdt_blocks<<12);
	gdt[0].block_bitmap=1+gdt_blocks;
	gdt[0].inode_bitmap=1+gdt_blocks+1;
	gdt[0].inode_table=1+gdt_blocks+2;
	if(blocks<32768)
	{
		free_blocks=blocks-(1+gdt_blocks+514)-1-4096;
	}
	else
	{
		free_blocks=32768-(1+gdt_blocks+514)-1-4096;
	}
	gdt[0].free_blocks=free_blocks;
	gdt[0].free_inodes=8182;
	gdt[0].used_dirs=1;
	has_super[0]=1;
	has_super[1]=5;
	has_super[2]=7;
	i=1;
	while(i<groups)
	{
		value=i*32768;
		if(i==has_super[0])
		{
			value+=1+gdt_blocks;
			has_super[0]*=3;
		}
		else if(i==has_super[1])
		{
			value+=1+gdt_blocks;
			has_super[1]*=5;
		}
		else if(i==has_super[2])
		{
			value+=1+gdt_blocks;
			has_super[2]*=7;
		}
		gdt[i].block_bitmap=value;
		gdt[i].block_bitmap_hi=value>>32;
		value+=1;
		gdt[i].inode_bitmap=value;
		gdt[i].inode_bitmap_hi=value>>32;
		value+=1;
		gdt[i].inode_table=value;
		gdt[i].inode_table_hi=value>>32;
		value+=512;
		if(blocks%32768!=0&&i==groups-1)
		{
			value=blocks-value;
		}
		else
		{
			value=(i+1)*32768-value;
		}
		gdt[i].free_blocks=value;
		gdt[i].free_inodes=8192;
		free_blocks+=value;
		++i;
	}
	sb.inodes=groups*8192;
	sb.blocks=blocks;
	sb.blocks_hi=blocks>>32;
	sb.r_blocks=blocks>>6;
	sb.free_blocks=free_blocks;
	sb.free_blocks_hi=free_blocks>>32;
	sb.free_inodes=sb.inodes-10;
	sb.first_data_block=0;
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
	sb.feature_compat=0x2c;
	sb.feature_incompat=0xc2;
	sb.feature_ro_compat=0x42b;
	fill_rand(sb.uuid);
	sb.journal_ino=8;
	sb.jnl_backup_type=1;
	fill_rand(sb.hash_seed);
	sb.hash_version=1;
	sb.desc_size=64;
	sb.csum_type=1;
	sb.checksum=crc32(&sb,1020,0xffffffff);
	i=0;
	unsigned int csum_seed;
	csum_seed=crc32(sb.uuid,16,0xffffffff);
	while(i<groups)
	{
		char bitmap[4096];
		unsigned int csum;
		memset(bitmap,0,sizeof(bitmap));
		if(blocks%32768!=0&&i==groups-1)
		{
			bitmap_set(bitmap,blocks-gdt[i].free_blocks-32768*i);
			bitmap_set2(bitmap,32768-blocks%32768);
		}
		else
		{
			bitmap_set(bitmap,32768-gdt[i].free_blocks);
		}
		csum=crc32(bitmap,4096,csum_seed);
		gdt[i].block_bitmap_csum=csum;
		gdt[i].block_bitmap_csum_hi=csum>>16;
		memset(bitmap,0,sizeof(bitmap));
		if(i==0)
		{
			bitmap[0]=0xff;
			bitmap[1]=0x03;
		}
		csum=crc32(bitmap,1024,csum_seed);
		gdt[i].inode_bitmap_csum=csum;
		gdt[i].inode_bitmap_csum_hi=csum>>16;
		csum=crc32(&i,4,csum_seed);
		csum=crc32(gdt+i,64,csum);
		gdt[i].checksum=csum;
		++i;
	}
	return 0;
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
void write_journal_block(void)
{
	unsigned int val;
	unsigned char uuid[16];
	val=0x98393bc0;
	buf_write(&val,4);
	val=0x04000000;
	buf_write(&val,4);
	buf_write_zero(4);
	val=0x00100000;
	buf_write(&val,4);
	buf_write(&val,4);
	val=0x01000000;
	buf_write(&val,4);
	buf_write(&val,4);
	buf_write_zero(20);
	fill_rand(uuid);
	buf_write(uuid,16);
	buf_write_zero(4096-64);
}
void write_first_group(void)
{
	struct ext2_inode inode;
	static unsigned char buf[4096];
	struct ext2_directory *dir;
	struct ext4_extent_header *eh;
	struct ext4_extent *ee;
	unsigned int csum;
	SetFilePointerEx(dev_handle,1024,NULL,FILE_BEGIN);
	buf_write(&sb,1024);
	buf_write_zero(2048);
	buf_write(gdt,gdt_blocks*4096);
	bitmap_set(block_bitmap,1+gdt_blocks+514+1+4096);
	if(groups==1)
	{
		bitmap_set2(block_bitmap,32768-blocks);
	}
	bitmap_set(inode_bitmap,10);
	buf_write(block_bitmap,4096);
	buf_write(inode_bitmap,4096);
	buf_write_zero(256);
	memset(&inode,0,256);
	inode.mode=040755;
	inode.links=2;
	inode.flags=FLAG_EXTENTS;
	inode.size=4096;
	inode.blocks=8;
	eh=(void *)inode.block;
	eh->magic=0xf30a;
	eh->entries=1;
	eh->max_entries=4;
	ee=(void *)(eh+1);
	ee->start_lo=1+gdt_blocks+514;
	ee->len=1;
	csum=crc32(sb.uuid,16,0xffffffff);
	csum=crc32("\x02\x00\x00\x00\x00\x00\x00\x00",8,csum);
	csum=crc32(&inode,256,csum);
	inode.osd2[2]=csum&0xffff;
	buf_write(&inode,256);
	buf_write_zero(256*5);
	inode.mode=0100600;
	inode.links=1;
	inode.size=4096*4096;
	inode.blocks=4096*8;
	++ee->start_lo;
	ee->len=4096;
	inode.osd2[2]=0;
	csum=crc32(sb.uuid,16,0xffffffff);
	csum=crc32("\x08\x00\x00\x00\x00\x00\x00\x00",8,csum);
	csum=crc32(&inode,256,csum);
	inode.osd2[2]=csum&0xffff;
	buf_write(&inode,256);
	buf_write_zero(256*8184);
	dir=(void *)buf;
	dir->inode=2;
	dir->rec_len=12;
	dir->name_len=1;
	dir->file_type=2;
	dir->file_name[0]='.';
	dir=(void *)(buf+12);
	dir->inode=2;
	dir->rec_len=4072;
	dir->name_len=2;
	dir->file_type=2;
	dir->file_name[0]='.';
	dir->file_name[1]='.';
	dir=(void *)(buf+4084);
	dir->rec_len=12;
	dir->file_type=0xde;
	csum=crc32(sb.uuid,16,0xffffffff);
	csum=crc32("\x02\x00\x00\x00\x00\x00\x00\x00",8,csum);
	csum=crc32(buf,4084,csum);
	*(unsigned int *)dir->file_name=csum;
	buf_write(buf,4096);
	write_journal_block();
	buf_write_zero(4095*4096);
	buf_flush();
}
void write_group(unsigned long n,int has_super)
{
	SetFilePointerEx(dev_handle,n<<27,NULL,FILE_BEGIN);
	if(has_super)
	{
		buf_write(&sb,1024);
		buf_write_zero(3072);
		buf_write(gdt,gdt_blocks*4096);
	}
	memset(block_bitmap,0,4096);
	if(n==groups-1&&blocks%32768!=0)
	{
		bitmap_set(block_bitmap,blocks-gdt[n].free_blocks-32768*n);
		bitmap_set2(block_bitmap,32768-blocks%32768);
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
	int i;
	unsigned int has_super[3];
	if(argc<2)
	{
		puts("Format a drive to ext4\n");
		puts("Usage: ./ext4_format.exe <device>\nPlease press any key to continue.\n");
		getch();
		return 1;
	}
	dev_handle=CreateFileA(argv[1],GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(dev_handle==INVALID_HANDLE_VALUE)
	{
		fatal_error("Cannot open device");
	}
	if(init_fs_info())
	{
		return 1;
	}
	memset(inode_bitmap+1024,0xff,3072);
	write_first_group();
	memset(inode_bitmap,0,1024);
	i=1;
	has_super[0]=1;
	has_super[1]=5;
	has_super[2]=7;
	print_progress(groups,1);
	while(i<groups)
	{
		if(i==has_super[0])
		{
			has_super[0]*=3;
			write_group(i,1);
		}
		else if(i==has_super[1])
		{
			has_super[1]*=5;
			write_group(i,1);
		}
		else if(i==has_super[2])
		{
			has_super[2]*=7;
			write_group(i,1);
		}
		else
		{
			write_group(i,0);
		}
		++i;
		print_progress(groups,i);
	}
	puts("\nSuccess\n");
	return 0;
}
