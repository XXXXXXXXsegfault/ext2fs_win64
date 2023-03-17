#include "../include/ext2.c"
void *dev_handle;
unsigned long dev_size;
unsigned char ext2_superblock_data[4096];
struct ext2_superblock *ext2_sb;
struct ext2_bgdt *ext2_bgdt_array;
int ext2_bgdt_blocks;
unsigned int ext2_groups;
unsigned long read_raw_blocks(void *buf,unsigned long off,unsigned long size)
{
	unsigned int ret;
	if(off>=dev_size)
	{
		return 0;
	}
	if(off+size>=dev_size)
	{
		size=dev_size-off;
	}
	SetFilePointerEx(dev_handle,off<<12,NULL,FILE_BEGIN);
	ReadFile(dev_handle,buf,size<<12,&ret,NULL);
	if(ret!=size<<12)
	{
		fatal_error("I/O error while reading blocks");
	}
	return size;
}
unsigned long write_raw_blocks(void *buf,unsigned long off,unsigned long size)
{
	unsigned int ret;
	if(off>=dev_size)
	{
		return 0;
	}
	if(off+size>=dev_size)
	{
		size=dev_size-off;
	}
	SetFilePointerEx(dev_handle,off<<12,NULL,FILE_BEGIN);
	WriteFile(dev_handle,buf,size<<12,&ret,NULL);
	if(ret!=size<<12)
	{
		fatal_error("I/O error while writing blocks");
	}
	return size;
}
#include "cache.c"
void ext2fs_init(char *name)
{
	unsigned long buf[128];
	int n,block;
	dev_handle=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(dev_handle==INVALID_HANDLE_VALUE)
	{
		fatal_error("Cannot open device");
	}
	if(DeviceIoControl(dev_handle,IOCTL_DISK_GET_PARTITION_INFO_EX,NULL,0,buf,sizeof(buf),NULL,NULL))
	{
		dev_size=buf[2]>>12;
	}
	else if(GetFileSizeEx(dev_handle,buf))
	{
		dev_size=buf[0]>>12;
	}
	else
	{
		fatal_error("Cannot determine device size");
	}
	if(read_raw_blocks(ext2_superblock_data,0,1)!=1)
	{
		fatal_error("Cannot read superblock");
	}
	ext2_sb=(void *)(ext2_superblock_data+1024);
	if(ext2_sb->magic!=0xef53||ext2_sb->blocks==0)
	{
		fatal_error("Device does not contain an ext2 filesystem");
	}
	if(ext2_sb->block_size>2)
	{
		fatal_error("Unsupported block size");
	}
	if(ext2_sb->blocks_per_group>1<<ext2_sb->block_size+13||
	ext2_sb->inodes_per_group>1<<ext2_sb->block_size+13)
	{
		fatal_error("Bad block or inode number");
	}
	if(ext2_sb->feature_incompat&~2||ext2_sb->feature_ro_compat&~3)
	{
		fatal_error("Filesystem has unsupported features");
	}
	cache_init();
	ext2_groups=(ext2_sb->blocks-1)/ext2_sb->blocks_per_group+1;
	ext2_bgdt_blocks=((ext2_groups*32-1)>>ext2_sb->block_size+10)+1;
	ext2_bgdt_array=xmalloc(ext2_bgdt_blocks<<ext2_sb->block_size+10);
	n=0;
	block=1;
	if(ext2_sb->block_size==0)
	{
		block=2;
	}
	while(n<ext2_bgdt_blocks)
	{
		ext2_read_block(block,ext2_bgdt_array+(n<<ext2_sb->block_size+5));
		++n;
		++block;
	}
}
struct ext2_file
{
	unsigned int ninode;
	unsigned int write;
	struct ext2_inode inode;
	unsigned int cache_write[6];
	unsigned int cache_block[6];
	unsigned int cache[1024*6];
};
struct ext2_file *ext2_file_load(unsigned int ninode,unsigned int write,unsigned int create)
{
	unsigned char buf[4096];
	struct ext2_file *file;
	unsigned int inode_group,inode_off,inode_block;
	if(ninode==0||ninode>ext2_sb->inodes)
	{
		return NULL;
	}

	file=malloc(sizeof(*file));
	if(file==NULL)
	{
		return NULL;
	}
	memset(file,0,sizeof(*file));
	inode_group=(ninode-1)/ext2_sb->inodes_per_group;
	inode_off=(ninode-1)%ext2_sb->inodes_per_group*ext2_sb->inode_size;
	inode_block=(inode_off>>ext2_sb->block_size+10)+ext2_bgdt_array[inode_group].inode_table;
	inode_off&=(1<<ext2_sb->block_size+10)-1;
	ext2_read_block(inode_block,buf);
	if(!create)
	{
		memcpy(&file->inode,buf+inode_off,128);
	}
	else
	{
		memset(buf+inode_off,0,ext2_sb->inode_size);
		ext2_write_block(inode_block,buf);
	}
	file->ninode=ninode;
	file->write=write;
	return file;
}
void ext2_file_release(struct ext2_file *file)
{
	unsigned char buf[4096];
	unsigned int ninode;
	unsigned int inode_group,inode_off,inode_block;
	int n;
	if(!file->write)
	{
		free(file);
		return;
	}
	ninode=file->ninode;
	inode_group=(ninode-1)/ext2_sb->inodes_per_group;
	inode_off=(ninode-1)%ext2_sb->inodes_per_group*ext2_sb->inode_size;
	inode_block=(inode_off>>ext2_sb->block_size+10)+ext2_bgdt_array[inode_group].inode_table;
	inode_off&=(1<<ext2_sb->block_size+10)-1;
	ext2_read_block(inode_block,buf);
	memcpy(buf+inode_off,&file->inode,128);
	ext2_write_block(inode_block,buf);
	n=0;
	while(n<6)
	{
		if(file->cache_block[n])
		{
			ext2_write_block(file->cache_block[n],file->cache+n*1024);
		}
		++n;
	}
	free(file);
}
unsigned int ext2_file_block(struct ext2_file *file,unsigned int off)
{
	unsigned long n,n1,n2,off1,off2,off3;
	if(off<12)
	{
		return file->inode.block[off];
	}
	off-=12;
	if(off<1<<ext2_sb->block_size+8)
	{
		n=file->inode.block[12];
		if(n==0)
		{
			return 0;
		}
		if(file->cache_block[0]!=n)
		{
			if(file->cache_write[0]&&file->cache_block[0]!=0)
			{
				ext2_write_block(file->cache_block[0],file->cache);
				file->cache_write[0]=0;
			}
			ext2_read_block(n,file->cache);
			file->cache_block[0]=n;
		}
		return file->cache[off];
	}
	off-=1<<ext2_sb->block_size+8;
	if(off<1<<2*ext2_sb->block_size+16)
	{
		off1=off>>ext2_sb->block_size+8;
		off2=off&(1<<ext2_sb->block_size+8)-1;
		n=file->inode.block[13];
		if(n==0)
		{
			return 0;
		}
		if(file->cache_block[1]!=n)
		{
			if(file->cache_write[1]&&file->cache_block[1]!=0)
			{
				ext2_write_block(file->cache_block[1],file->cache+1024*1);
				file->cache_write[1]=0;
			}
			ext2_read_block(n,file->cache+1024*1);
			file->cache_block[1]=n;
		}
		n1=file->cache[1024*1+off1];
		if(n1==0)
		{
			return 0;
		}
		if(file->cache_block[2]!=n1)
		{
			if(file->cache_write[2]&&file->cache_block[2]!=0)
			{
				ext2_write_block(file->cache_block[2],file->cache+1024*2);
				file->cache_write[2]=0;
			}
			ext2_read_block(n1,file->cache+1024*2);
			file->cache_block[2]=n1;
		}
		return file->cache[1024*2+off2];
	}
	off-=1<<2*ext2_sb->block_size+16;
	if(off<1<<3*ext2_sb->block_size+24)
	{
		off1=off>>2*ext2_sb->block_size+16;
		off2=off>>ext2_sb->block_size+8&(1<<ext2_sb->block_size+8)-1;
		off3=off&(1<<ext2_sb->block_size+8)-1;
		n=file->inode.block[14];
		if(n==0)
		{
			return 0;
		}
		if(file->cache_block[3]!=n)
		{
			if(file->cache_write[3]&&file->cache_block[3]!=0)
			{
				ext2_write_block(file->cache_block[3],file->cache+1024*3);
				file->cache_write[3]=0;
			}
			ext2_read_block(n,file->cache+1024*3);
			file->cache_block[3]=n;
		}
		n1=file->cache[1024*3+off1];
		if(n1==0)
		{
			return 0;
		}
		if(file->cache_block[4]!=n1)
		{
			if(file->cache_write[4]&&file->cache_block[4]!=0)
			{
				ext2_write_block(file->cache_block[4],file->cache+1024*4);
				file->cache_write[4]=0;
			}
			ext2_read_block(n1,file->cache+1024*4);
			file->cache_block[4]=n1;
		}
		n2=file->cache[1024*4+off2];
		if(n2==0)
		{
			return 0;
		}
		if(file->cache_block[5]!=n2)
		{
			if(file->cache_write[5]&&file->cache_block[5]!=0)
			{
				ext2_write_block(file->cache_block[5],file->cache+1024*5);
				file->cache_write[5]=0;
			}
			ext2_read_block(n2,file->cache+1024*5);
			file->cache_block[5]=n2;
		}
		return file->cache[1024*5+off3];
	}
	return 0;
}
unsigned long ext2_file_size_get(struct ext2_file *file)
{
	unsigned long size;
	if((file->inode.mode&0170000)==0100000)
	{
		size=file->inode.dir_acl;
		size=size<<32|file->inode.size;
	}
	else
	{
		size=file->inode.size;
	}
	return size;
}
void ext2_file_size_set(struct ext2_file *file,unsigned long size)
{
	file->inode.size=size;
	if((file->inode.mode&0170000)==0100000)
	{
		file->inode.dir_acl=size>>32;
	}
}
int ext2_file_read(struct ext2_file *file,unsigned long off,void *buf,unsigned int size)
{
	unsigned int bn,block,ret,size1;
	unsigned long fsize;
	unsigned char buf2[4096];
	fsize=ext2_file_size_get(file);
	if(off>=fsize)
	{
		return 0;
	}
	if(off+size>=fsize)
	{
		size=fsize-off;
	}
	bn=off>>ext2_sb->block_size+10;
	off&=(1<<ext2_sb->block_size+10)-1;
	ret=size;
	while(size)
	{
		size1=size;
		if(size1>(1<<ext2_sb->block_size+10)-off)
		{
			size1=(1<<ext2_sb->block_size+10)-off;
		}
		block=ext2_file_block(file,bn);
		++bn;
		if(block==0)
		{
			memset(buf,0,size1);
		}
		else
		{
			ext2_read_block(block,buf2);
			memcpy(buf,buf2+off,size1);
		}
		size-=size1;
		buf=(char *)buf+size1;
		off=0;
	}
	return ret;
}
unsigned int ext2_inode_alloc_group(unsigned int group)
{
	unsigned char buf[4096];
	unsigned int ret;
	int n;
	ext2_read_block(ext2_bgdt_array[group].inode_bitmap,buf);
	n=bitmap_find(buf,ext2_sb->inodes_per_group>>3);
	if(n==-1)
	{
		return 0;
	}
	ret=group*ext2_sb->inodes_per_group+n+1;
	if(ret>ext2_sb->inodes)
	{
		return 0;
	}
	bitmap_set(buf,n);
	ext2_write_block(ext2_bgdt_array[group].inode_bitmap,buf);
	--ext2_bgdt_array[group].free_inodes;
	--ext2_sb->free_inodes;
	return ret;
}
unsigned int ext2_inode_alloc(unsigned int dir_inode)
{
	unsigned int group,n;
	if(dir_inode>ext2_sb->inodes)
	{
		fatal_error("Inode number out of range when allocating inode");
	}
	if(ext2_sb->free_inodes==0)
	{
		return 0;
	}
	if(dir_inode)
	{
		group=(dir_inode-1)/ext2_sb->inodes_per_group;
		n=group;
		do
		{
			if(ext2_bgdt_array[n].free_inodes)
			{
				return ext2_inode_alloc_group(n);
			}
			++n;
			if(n==ext2_groups)
			{
				n=0;
			}
		}
		while(n!=group);
	}
	else
	{
		group=0;
		while(group<ext2_groups)
		{
			if(ext2_bgdt_array[group].free_inodes>ext2_sb->free_inodes/ext2_groups)
			{
				return ext2_inode_alloc_group(group);
			}
			++group;
		}
		group=0;
		while(group<ext2_groups)
		{
			if(ext2_bgdt_array[group].free_inodes)
			{
				return ext2_inode_alloc_group(group);
			}
			++group;
		}
	}
	return 0;
}
void ext2_inode_release(unsigned int inode)
{
	unsigned int group;
	unsigned char buf[4096];
	unsigned int n,block;
	if(inode==0||inode>ext2_sb->inodes)
	{
		fatal_error("Inode number out of range");
	}
	group=(inode-1)/ext2_sb->inodes_per_group;
	n=(inode-1)%ext2_sb->inodes_per_group;
	ext2_read_block(ext2_bgdt_array[group].inode_bitmap,buf);
	bitmap_clr(buf,n);
	ext2_write_block(ext2_bgdt_array[group].inode_bitmap,buf);
	n*=ext2_sb->inode_size;
	block=ext2_bgdt_array[group].inode_table+(n>>ext2_sb->block_size+10);
	n&=(1<<ext2_sb->block_size+10)-1;
	ext2_read_block(block,buf);
	memset(buf+n,0,ext2_sb->inode_size);
	ext2_write_block(block,buf);
	++ext2_bgdt_array[group].free_inodes;
	++ext2_sb->free_inodes;
}
unsigned int ext2_block_alloc_group(unsigned int group)
{
	unsigned char buf[4096];
	unsigned int ret;
	int n;
	ext2_read_block(ext2_bgdt_array[group].block_bitmap,buf);
	n=bitmap_find(buf,ext2_sb->blocks_per_group>>3);
	if(n==-1)
	{
		return 0;
	}
	ret=group*ext2_sb->blocks_per_group+n;
	if(ext2_sb->block_size==0)
	{
		++ret;
	}
	if(ret>=ext2_sb->blocks)
	{
		return 0;
	}
	bitmap_set(buf,n);
	ext2_write_block(ext2_bgdt_array[group].block_bitmap,buf);
	--ext2_bgdt_array[group].free_blocks;
	--ext2_sb->free_blocks;
	return ret;
}
unsigned int ext2_block_alloc(unsigned int inode)
{
	unsigned int group,n;
	if(inode==0||inode>ext2_sb->inodes)
	{
		fatal_error("Inode number out of range when allocating block");
	}
	if(ext2_sb->free_blocks==0)
	{
		return 0;
	}
	group=(inode-1)/ext2_sb->inodes_per_group;
	n=group;
	do
	{
		if(ext2_bgdt_array[n].free_blocks)
		{
			return ext2_block_alloc_group(n);
		}
		++n;
		if(n==ext2_groups)
		{
			n=0;
		}
	}
	while(n!=group);
	return 0;
}
void ext2_block_release(unsigned int block)
{
	unsigned int group;
	unsigned char buf[4096];
	unsigned int n;
	if(block>=ext2_sb->blocks)
	{
		fatal_error("Block number out of range");
	}
	if(ext2_sb->block_size==0)
	{
		--block;
	}
	group=block/ext2_sb->blocks_per_group;
	n=block%ext2_sb->blocks_per_group;
	ext2_read_block(ext2_bgdt_array[group].block_bitmap,buf);
	bitmap_clr(buf,n);
	ext2_write_block(ext2_bgdt_array[group].block_bitmap,buf);
	++ext2_bgdt_array[group].free_blocks;
	++ext2_sb->free_blocks;
}
unsigned int ext2_file_alloc_block(struct ext2_file *file,unsigned int off)
{
	unsigned long n,n1,n2,off1,off2,off3,ret;
	int s;
	if(file->inode.blocks>=0xffffffe8)
	{
		return 0;
	}
	if(off<12)
	{
		ret=file->inode.block[off];
		if(ret==0)
		{
			ret=ext2_block_alloc(file->ninode);
			if(ret==0)
			{
				return 0;
			}
			file->inode.block[off]=ret;
			file->inode.blocks+=2<<ext2_sb->block_size;
		}
		return ret;
	}
	off-=12;
	if(off<1<<ext2_sb->block_size+8)
	{
		n=file->inode.block[12];
		s=0;
		if(n==0)
		{
			n=ext2_block_alloc(file->ninode);
			if(n==0)
			{
				return 0;
			}
			file->inode.block[12]=n;
			file->inode.blocks+=2<<ext2_sb->block_size;
			s=1;
		}
		if(file->cache_block[0]!=n)
		{
			if(file->cache_write[0]&&file->cache_block[0]!=0)
			{
				ext2_write_block(file->cache_block[0],file->cache);
				file->cache_write[0]=0;
			}
			if(s)
			{
				memset(file->cache,0,4096);
				ext2_write_block(n,file->cache);
				file->cache_write[0]=1;
			}
			else
			{
				ext2_read_block(n,file->cache);
			}
			file->cache_block[0]=n;
		}
		ret=file->cache[off];
		if(ret==0)
		{
			ret=ext2_block_alloc(file->ninode);
			if(ret==0)
			{
				return 0;
			}
			file->cache[off]=ret;
			file->cache_write[0]=1;
			file->inode.blocks+=2<<ext2_sb->block_size;
		}
		return ret;
	}
	off-=1<<ext2_sb->block_size+8;
	if(off<1<<2*ext2_sb->block_size+16)
	{
		off1=off>>ext2_sb->block_size+8;
		off2=off&(1<<ext2_sb->block_size+8)-1;
		n=file->inode.block[13];
		s=0;
		if(n==0)
		{
			n=ext2_block_alloc(file->ninode);
			if(n==0)
			{
				return 0;
			}
			file->inode.block[13]=n;
			file->inode.blocks+=2<<ext2_sb->block_size;
			s=1;
		}
		if(file->cache_block[1]!=n)
		{
			if(file->cache_write[1]&&file->cache_block[1]!=0)
			{
				ext2_write_block(file->cache_block[1],file->cache+1024*1);
				file->cache_write[1]=0;
			}
			if(s)
			{
				memset(file->cache+1024*1,0,4096);
				ext2_write_block(n,file->cache+1024*1);
				file->cache_write[1]=1;
			}
			else
			{
				ext2_read_block(n,file->cache+1024*1);
			}
			file->cache_block[1]=n;
		}
		n1=file->cache[1024*1+off1];
		s=0;
		if(n1==0)
		{
			n1=ext2_block_alloc(file->ninode);
			if(n1==0)
			{
				return 0;
			}
			file->cache[1024*1+off1]=n1;
			file->cache_write[1]=1;
			file->inode.blocks+=2<<ext2_sb->block_size;
			s=1;
		}
		if(file->cache_block[2]!=n1)
		{
			if(file->cache_write[2]&&file->cache_block[2]!=0)
			{
				ext2_write_block(file->cache_block[2],file->cache+1024*2);
				file->cache_write[2]=0;
			}
			if(s)
			{
				memset(file->cache+1024*2,0,4096);
				ext2_write_block(n1,file->cache+1024*2);
				file->cache_write[2]=1;
			}
			else
			{
				ext2_read_block(n1,file->cache+1024*2);
			}
			file->cache_block[2]=n1;
		}
		ret=file->cache[1024*2+off2];
		if(ret==0)
		{
			ret=ext2_block_alloc(file->ninode);
			if(ret==0)
			{
				return 0;
			}
			file->cache[1024*2+off2]=ret;
			file->cache_write[2]=1;
			file->inode.blocks+=2<<ext2_sb->block_size;
		}
		return ret;
	}
	off-=1<<2*ext2_sb->block_size+16;
	if(off<1<<3*ext2_sb->block_size+24)
	{
		off1=off>>2*ext2_sb->block_size+16;
		off2=off>>ext2_sb->block_size+8&(1<<ext2_sb->block_size+8)-1;
		off3=off&(1<<ext2_sb->block_size+8)-1;
		n=file->inode.block[14];
		s=0;
		if(n==0)
		{
			n=ext2_block_alloc(file->ninode);
			if(n==0)
			{
				return 0;
			}
			file->inode.block[14]=n;
			file->inode.blocks+=2<<ext2_sb->block_size;
			s=1;
		}
		if(file->cache_block[3]!=n)
		{
			if(file->cache_write[3]&&file->cache_block[3]!=0)
			{
				ext2_write_block(file->cache_block[3],file->cache+1024*3);
				file->cache_write[3]=0;
			}
			if(s)
			{
				memset(file->cache+1024*3,0,4096);
				ext2_write_block(n,file->cache+1024*3);
				file->cache_write[3]=1;
			}
			else
			{
				ext2_read_block(n,file->cache+1024*3);
			}
			file->cache_block[3]=n;
		}
		n1=file->cache[1024*3+off1];
		s=0;
		if(n1==0)
		{
			n1=ext2_block_alloc(file->ninode);
			if(n1==0)
			{
				return 0;
			}
			file->cache[1024*3+off1]=n1;
			file->cache_write[3]=1;
			file->inode.blocks+=2<<ext2_sb->block_size;
			s=1;
		}
		if(file->cache_block[4]!=n1)
		{
			if(file->cache_write[4]&&file->cache_block[4]!=0)
			{
				ext2_write_block(file->cache_block[4],file->cache+1024*4);
				file->cache_write[4]=0;
			}
			if(s)
			{
				memset(file->cache+1024*4,0,4096);
				ext2_write_block(n1,file->cache+1024*4);
				file->cache_write[4]=1;
			}
			else
			{
				ext2_read_block(n1,file->cache+1024*4);
			}
			file->cache_block[4]=n1;
		}
		n2=file->cache[1024*4+off2];
		s=0;
		if(n2==0)
		{
			n2=ext2_block_alloc(file->ninode);
			if(n2==0)
			{
				return 0;
			}
			file->cache[1024*4+off2]=n2;
			file->cache_write[4]=1;
			file->inode.blocks+=2<<ext2_sb->block_size;
			s=1;
		}
		if(file->cache_block[5]!=n2)
		{
			if(file->cache_write[5]&&file->cache_block[5]!=0)
			{
				ext2_write_block(file->cache_block[5],file->cache+1024*5);
				file->cache_write[5]=0;
			}
			if(s)
			{
				memset(file->cache+1024*5,0,4096);
				ext2_write_block(n2,file->cache+1024*5);
				file->cache_write[5]=1;
			}
			else
			{
				ext2_read_block(n2,file->cache+1024*5);
			}
			file->cache_block[5]=n2;
		}
		ret=file->cache[1024*5+off3];
		if(ret==0)
		{
			ret=ext2_block_alloc(file->ninode);
			if(ret==0)
			{
				return 0;
			}
			file->cache[1024*5+off3]=ret;
			file->cache_write[5]=1;
			file->inode.blocks+=2<<ext2_sb->block_size;
		}
		return ret;
	}
	return 0;
}
int ext2_file_write(struct ext2_file *file,unsigned long off,void *buf,unsigned int size)
{
	unsigned int bn,block,ret,size1;
	unsigned long fsize,start;
	unsigned char buf2[4096];
	fsize=ext2_file_size_get(file);
	start=off;
	bn=off>>ext2_sb->block_size+10;
	off&=(1<<ext2_sb->block_size+10)-1;
	ret=0;
	while(size)
	{
		size1=size;
		if(size1>(1<<ext2_sb->block_size+10)-off)
		{
			size1=(1<<ext2_sb->block_size+10)-off;
		}
		block=ext2_file_alloc_block(file,bn);
		++bn;
		if(block==0)
		{
			break;
		}
		else
		{
			if(size1!=1<<ext2_sb->block_size+10)
			{
				ext2_read_block(block,buf2);
			}
			memcpy(buf2+off,buf,size1);
			ext2_write_block(block,buf2);
		}
		ret+=size1;
		size-=size1;
		buf=(char *)buf+size1;
		off=0;
	}
	if(start+ret>fsize)
	{
		ext2_file_size_set(file,start+ret);
	}
	return ret;
}
void ext2_sync(void)
{
	unsigned char sb[4096];
	unsigned int sb_block,sb_off;
	int n;
	sb_block=0;
	sb_off=1024;
	if(ext2_sb->block_size==0)
	{
		sb_block=1;
		sb_off=0;
	}
	ext2_read_block(sb_block,sb);
	memcpy(sb+sb_off,ext2_sb,1024);
	ext2_write_block(sb_block,sb);
	n=0;
	while(n<ext2_bgdt_blocks)
	{
		ext2_write_block(sb_block+n+1,ext2_bgdt_array+(n<<ext2_sb->block_size+5));
		++n;
	}
	global_cache_flush_all();
}
long ext2_readdir(struct ext2_file *dir,long off,void *dirent)
{
	char buf[8];
	struct ext2_directory *dent;
	dent=(void *)buf;
	if(ext2_file_read(dir,off,buf,8)!=8)
	{
		return 0;
	}
	if(dent->rec_len<8||dent->rec_len>4096)
	{
		return 0;
	}
	if(ext2_file_read(dir,off,dirent,dent->rec_len)!=dent->rec_len)
	{
		return 0;
	}
	return off+dent->rec_len;
}
unsigned int ext2_search(unsigned int inode,char *name)
{
	char buf[4096];
	struct ext2_file *dir;
	struct ext2_directory *dirent;
	long off;
	int len;
	if(inode==0||inode>ext2_sb->inodes)
	{
		return 0;
	}
	dir=ext2_file_load(inode,0,0);
	if(dir==NULL)
	{
		return 0;
	}
	if((dir->inode.mode&0170000)!=040000)
	{
		ext2_file_release(dir);
		return 0;
	}
	off=0;
	len=strlen(name);
	dirent=(void *)buf;
	while((off=ext2_readdir(dir,off,dirent))!=0)
	{
		
		if(dirent->inode&&(unsigned int)dirent->name_len==len&&!memcmp(dirent->file_name,name,len))
		{
			ext2_file_release(dir);
			return dirent->inode;
		}
	}
	ext2_file_release(dir);
	return 0;
}
int ext2_search_path(unsigned int start_inode,char *name,unsigned int *inode)
{
	char buf[256],c;
	int x;
	*inode=0;
	if(*name=='/')
	{
		start_inode=2;
		while(*name=='/')
		{
			++name;
		}
	}
	while(*name)
	{
		x=0;
		while((c=*name)&&c!='/')
		{
			if(x>=255)
			{
				return -1;
			}
			buf[x]=c;
			++x;
			++name;
		}
		buf[x]=0;
		while(*name=='/')
		{
			++name;
		}
		start_inode=ext2_search(start_inode,buf);
		if(start_inode==0)
		{
			return -2;
		}
	}
	*inode=start_inode;
	return 0;
}
int ext2_detect_file(unsigned int start_inode,char *name)
{
	char buf[256],c;
	int x;
	if(*name=='/')
	{
		start_inode=2;
		while(*name=='/')
		{
			++name;
		}
	}
	while(*name)
	{
		x=0;
		while((c=*name)&&c!='/')
		{
			if(x>=255)
			{
				return -1;
			}
			buf[x]=c;
			++x;
			++name;
		}
		buf[x]=0;
		while(*name=='/')
		{
			++name;
		}
		start_inode=ext2_search(start_inode,buf);
		if(start_inode==0)
		{
			if(*name)
			{
				return -2;
			}
			return 0;
		}
	}
	return -1;
}
unsigned int ext2_get_dname(unsigned int inode,char *name)
{
	char buf[4096];
	struct ext2_file *dir;
	struct ext2_directory *dirent;
	long off;
	int len;
	unsigned int parent_inode;
	parent_inode=ext2_search(inode,"..");
	if(parent_inode==0)
	{
		return 0;
	}
	dir=ext2_file_load(parent_inode,0,0);
	if(dir==NULL)
	{
		return 0;
	}
	if((dir->inode.mode&0170000)!=040000)
	{
		ext2_file_release(dir);
		return 0;
	}
	off=0;
	len=strlen(name);
	dirent=(void *)buf;
	while((off=ext2_readdir(dir,off,dirent))!=0)
	{
		if(dirent->inode==inode)
		{
			memcpy(name,dirent->file_name,(unsigned int)dirent->name_len);
			name[(unsigned int)dirent->name_len]=0;
			ext2_file_release(dir);
			return parent_inode;
		}
	}
	ext2_file_release(dir);
	return 0;
}
unsigned int ext2_mknod(unsigned int dir,int mode,int links)
{
	struct ext2_file *file;
	unsigned int inode;
	inode=ext2_inode_alloc(dir);
	if(inode==0)
	{
		return 0;
	}
	file=ext2_file_load(inode,1,1);
	if(file==NULL)
	{
		ext2_inode_release(inode);
		return 0;
	}
	file->inode.mode=mode;
	file->inode.links=links;
	ext2_file_release(file);
	return inode;
}
int ext2_mkdir(unsigned int dir,unsigned int inode)
{
	struct ext2_file *file;
	struct ext2_directory *dirent;
	char buf[4096];
	dirent=(void *)buf;
	memset(buf,0,sizeof(buf));
	dirent->inode=inode;
	dirent->rec_len=12;
	dirent->name_len=1;
	dirent->file_type=2;
	dirent->file_name[0]='.';
	dirent=(void *)(buf+12);
	dirent->inode=dir;
	dirent->rec_len=(1<<ext2_sb->block_size+10)-12;
	dirent->name_len=2;
	dirent->file_type=2;
	dirent->file_name[0]='.';
	dirent->file_name[1]='.';
	file=ext2_file_load(inode,1,0);
	if(file==NULL)
	{
		return -1;
	}
	if(ext2_file_write(file,0,buf,1<<ext2_sb->block_size+10)!=1<<ext2_sb->block_size+10)
	{
		ext2_file_release(file);
		return -1;
	}
	ext2_file_release(file);
	return 0;
}
int ext2_link(struct ext2_file *dir,unsigned int inode,char *name,int file_type)
{
	char buf[256],c;
	int x,reclen;
	char buf2[4096];
	struct ext2_directory *dirent;
	long off;
	if(*name=='/')
	{
		while(*name=='/')
		{
			++name;
		}
	}
	while(*name)
	{
		x=0;
		while((c=*name)&&c!='/')
		{
			if(x>=255)
			{
				return -1;
			}
			buf[x]=c;
			++x;
			++name;
		}
		buf[x]=0;
		while(*name=='/')
		{
			++name;
		}
		if(!*name)
		{
			dirent=(void *)buf2;
			off=0;
			while(off=ext2_readdir(dir,off,dirent))
			{
				if(dirent->inode&&(unsigned int)dirent->name_len==x&&!memcmp(dirent->file_name,buf,x))
				{
					return -1;
				}
			}
			off=0;
			reclen=(x-1>>2)+3<<2;
			while(off=ext2_readdir(dir,off,dirent))
			{
				if(dirent->inode==0&&dirent->rec_len>=reclen)
				{
					off-=dirent->rec_len;
					dirent->inode=inode;
					dirent->name_len=x;
					if(ext2_sb->feature_incompat&2)
					{
						dirent->file_type=file_type;
					}
					else
					{
						dirent->file_type=0;
					}
					memcpy(dirent->file_name,buf,x);
					ext2_file_write(dir,off,dirent,dirent->rec_len);
					return 0;
				}
				else if(dirent->inode)
				{
					unsigned int l;
					l=dirent->name_len;
					l=(l-1>>2)+3<<2;
					if(dirent->rec_len>=l+reclen)
					{
						off-=dirent->rec_len;
						reclen=dirent->rec_len-l;
						dirent->rec_len=l;
						ext2_file_write(dir,off,dirent,l);
						dirent=(void *)((char *)dirent+l);
						off+=l;
						dirent->inode=inode;
						dirent->rec_len=reclen;
						dirent->name_len=x;
						if(ext2_sb->feature_incompat&2)
						{
							dirent->file_type=file_type;
						}
						else
						{
							dirent->file_type=0;
						}
						memcpy(dirent->file_name,buf,x);
						ext2_file_write(dir,off,dirent,dirent->rec_len);
						return 0;
					}
				}
			}
			off=ext2_file_size_get(dir);
			memset(dirent,0,4096);
			dirent->inode=inode;
			dirent->rec_len=1<<ext2_sb->block_size+10;
			dirent->name_len=x;
			if(ext2_sb->feature_incompat&2)
			{
				dirent->file_type=file_type;
			}
			else
			{
				dirent->file_type=0;
			}
			memcpy(dirent->file_name,buf,x);
			if(ext2_file_write(dir,off,dirent,dirent->rec_len)!=dirent->rec_len)
			{
				return -1;
			}
			return 0;
		}
	}
	return -1;
}

unsigned int ext2_unlink(struct ext2_file *dir,char *name)
{
	char buf[256],c;
	int x;
	char buf2[4096];
	struct ext2_directory *dirent;
	long off;
	unsigned int inode;
	if(*name=='/')
	{
		while(*name=='/')
		{
			++name;
		}
	}
	while(*name)
	{
		x=0;
		while((c=*name)&&c!='/')
		{
			if(x>=255)
			{
				return 0;
			}
			buf[x]=c;
			++x;
			++name;
		}
		buf[x]=0;
		while(*name=='/')
		{
			++name;
		}
		if(!*name)
		{
			dirent=(void *)buf2;
			off=0;
			while(off=ext2_readdir(dir,off,dirent))
			{
				if(dirent->inode&&(unsigned int)dirent->name_len==x&&!memcmp(dirent->file_name,buf,x))
				{
					off-=dirent->rec_len;
					inode=dirent->inode;
					dirent->inode=0;
					dirent->name_len=0;
					dirent->file_type=0;
					ext2_file_write(dir,off,dirent,dirent->rec_len);
					return inode;
				}
			}
			return 0;
		}
	}
	return 0;
}
unsigned int ext2_dir_inode(unsigned int start_inode,char *name)
{
	char buf[256],c;
	int x;
	if(*name=='/')
	{
		start_inode=2;
		while(*name=='/')
		{
			++name;
		}
	}
	while(*name)
	{
		x=0;
		while((c=*name)&&c!='/')
		{
			if(x>=255)
			{
				return 0;
			}
			buf[x]=c;
			++x;
			++name;
		}
		buf[x]=0;
		while(*name=='/')
		{
			++name;
		}
		if(*name)
		{
			start_inode=ext2_search(start_inode,buf);
			if(start_inode==0)
			{	
				return 0;
			}
		}
		else
		{
			return start_inode;
		}
	}
	return 0;
}
int ext2_is_special_entry(char *name)
{
	char buf[256],c;
	int x;
	if(*name=='/')
	{
		while(*name=='/')
		{
			++name;
		}
	}
	while(*name)
	{
		x=0;
		while((c=*name)&&c!='/')
		{
			if(x>=255)
			{
				return -1;
			}
			buf[x]=c;
			++x;
			++name;
		}
		buf[x]=0;
		while(*name=='/')
		{
			++name;
		}
		if(!*name)
		{
			if(!strcmp(buf,".")||!strcmp(buf,".."))
			{
				return 1;
			}
			return 0;
		}
	}
	return -1;
}
int ext2_mkdir_path(unsigned int dir,char *path)
{
	unsigned int inode,diri,group;
	struct ext2_file *file;
	if(ext2_detect_file(dir,path))
	{
		return -1;
	}
	diri=ext2_dir_inode(dir,path);
	if(diri==0)
	{
		return -1;
	}
	file=ext2_file_load(diri,1,0);
	if(file==NULL)
	{
		return -1;
	}
	if(file->inode.links==65535)
	{
		ext2_file_release(file);
		return -1;
	}
	inode=ext2_mknod(diri,040755,2);
	if(inode==0)
	{
		ext2_file_release(file);
		return -1;
	}
	group=(inode-1)/ext2_sb->inodes_per_group;
	if(ext2_bgdt_array[group].used_dirs==65535)
	{
		ext2_file_release(file);
		ext2_inode_release(inode);
		return -1;
	}
	if(ext2_link(file,inode,path,2))
	{
		ext2_file_release(file);
		ext2_inode_release(inode);
		return -1;
	}
	if(ext2_mkdir(diri,inode))
	{
		ext2_unlink(file,path);
		ext2_file_release(file);
		ext2_inode_release(inode);
		return -1;
	}
	++file->inode.links;
	++ext2_bgdt_array[group].used_dirs;
	ext2_file_release(file);
	return 0;
}
void ext2_release_blocks(unsigned int n,unsigned int level)
{
	unsigned int buf[1024];
	int x;
	if(n==0)
	{
		return;
	}
	if(level)
	{
		ext2_read_block(n,buf);
		x=0;
		while(x<1<<ext2_sb->block_size+8)
		{
			ext2_release_blocks(buf[x],level-1);
			++x;
		}
	}
	ext2_block_release(n);
}
void ext2_release_file_blocks(struct ext2_file *file)
{
	int x;
	x=0;
	while(x<12)
	{
		ext2_release_blocks(file->inode.block[x],0);
		++x;
	}
	ext2_release_blocks(file->inode.block[12],1);
	ext2_release_blocks(file->inode.block[13],2);
	ext2_release_blocks(file->inode.block[14],3);
	file->inode.blocks=0;
	file->inode.size=0;
	memset(file->inode.block,0,sizeof(file->inode.block));
	memset(file->cache,0,sizeof(file->cache));
	memset(file->cache_block,0,sizeof(file->cache_block));
	memset(file->cache_write,0,sizeof(file->cache_write));
}