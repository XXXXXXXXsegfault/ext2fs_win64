HANDLE dev_handle;
unsigned long long dev_size;
unsigned char ext2_superblock_data[4096];
struct ext2_superblock *ext2_sb;
unsigned long long int ext2_desc_size;
struct ext4_bgdt *ext2_bgdt_array;
int ext2_bgdt_blocks;
unsigned long long ext2_groups;
unsigned long long read_raw_blocks(void *buf,unsigned long long off,unsigned long long size)
{
	unsigned int ret;
	LARGE_INTEGER pos;
	if(off>=dev_size)
	{
		return 0;
	}
	if(off+size>=dev_size)
	{
		size=dev_size-off;
	}
	pos.QuadPart = off << 12;
	SetFilePointerEx(dev_handle,pos,NULL,FILE_BEGIN);
	ReadFile(dev_handle,buf,size<<12,&ret,NULL);
	if(ret!=size<<12)
	{
		fatal_error("I/O error while reading blocks");
	}
	return size;
}
unsigned long long write_raw_blocks(void *buf,unsigned long long off,unsigned long long size)
{
	unsigned int ret;
	LARGE_INTEGER pos;
	if(off>=dev_size)
	{
		return 0;
	}
	if(off+size>=dev_size)
	{
		size=dev_size-off;
	}
	pos.QuadPart = off << 12;
	SetFilePointerEx(dev_handle, pos, NULL, FILE_BEGIN);
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
	unsigned long long buf[128];
	unsigned long long blocks;
	int n,block;
	dev_handle=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(dev_handle==INVALID_HANDLE_VALUE)
	{
		fatal_error("Cannot open device");
	}
	if(DeviceIoControl(dev_handle,IOCTL_DISK_GET_PARTITION_INFO_EX,NULL,0,(void *)buf,sizeof(buf),NULL,NULL))
	{
		dev_size=buf[2]>>12;
	}
	else if(GetFileSizeEx(dev_handle,(void *)buf))
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
	if(ext2_sb->magic!=0xef53||ext2_sb->blocks==0&&ext2_sb->blocks_hi==0)
	{
		fatal_error("Device does not contain an ext2 filesystem");
	}
	if(ext2_sb->block_size>2)
	{
		fatal_error("Unsupported block size");
	}
	if(ext2_sb->inode_size!=128&&ext2_sb->inode_size!=256)
	{
		fatal_error("Unsupported inode size");
	}
	if(ext2_sb->blocks_per_group>1<<ext2_sb->block_size+13||
	ext2_sb->inodes_per_group>1<<ext2_sb->block_size+13)
	{
		fatal_error("Bad block or inode number");
	}
	if(ext2_sb->feature_incompat&~0x22c2||ext2_sb->feature_ro_compat&~0x46b)
	{
		fatal_error("Filesystem has unsupported features");
	}
	cache_init();
	blocks=(unsigned long long)ext2_sb->blocks;
	if(ext2_sb->feature_incompat&FEATURE_64BIT)
	{
		blocks+=(unsigned long long)ext2_sb->blocks_hi<<32;
	}
	ext2_groups=(blocks-1)/ext2_sb->blocks_per_group+1;
	ext2_desc_size=ext2_sb->desc_size;
	if(ext2_desc_size<32)
	{
		ext2_desc_size=32;
	}
	ext2_bgdt_blocks=((ext2_groups*ext2_desc_size-1)>>ext2_sb->block_size+10)+1;
	ext2_bgdt_array=xmalloc(ext2_bgdt_blocks<<ext2_sb->block_size+10);
	n=0;
	block=1;
	if(ext2_sb->block_size==0)
	{
		block=2;
	}
	while(n<ext2_bgdt_blocks)
	{
		ext2_read_block(block,(char *)ext2_bgdt_array+(n<<ext2_sb->block_size+10));
		++n;
		++block;
	}
}
unsigned long long ext2_blocks(void)
{
	unsigned long long val;
	val=ext2_sb->blocks;
	if(ext2_sb->feature_incompat&FEATURE_64BIT)
	{
		val|=(unsigned long long)ext2_sb->blocks_hi<<32;
	}
	return val;
}
struct ext2_file
{
	unsigned long long int ninode;
	unsigned long long int write;
	struct ext2_inode inode;
	unsigned long long cache_write[6];
	unsigned long long cache_block[6];
	unsigned long long cache_crc[6];
	unsigned long long crc_seed;
	unsigned int cache[1024*6];
};
void block_update_crc(void *block,unsigned int init)
{
	unsigned int *crc;
	unsigned int size;
	size=(1<<ext2_sb->block_size+10)/12*12;
	crc=(void *)((char *)block+size);
	*crc=crc32(block,size,init);
}
struct ext2_file *ext2_file_load(unsigned int ninode,unsigned long long int write,unsigned long long int create)
{
	unsigned char buf[4096];
	struct ext2_file *file;
	unsigned long long ctime,ctime_extra;
	unsigned long long inode_group,inode_off,inode_block;
	
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
	struct ext4_bgdt *desc;
	desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*inode_group);
	inode_block=(inode_off>>ext2_sb->block_size+10)+desc->inode_table;
	if(ext2_desc_size>=64)
	{
		inode_block+=(unsigned long long)desc->inode_table_hi<<32;
	}
	inode_off&=(1<<ext2_sb->block_size+10)-1;
	ctime=time(NULL);
	ctime_extra=ctime>>32&3;
	ctime&=0xffffffff;
	ext2_read_block(inode_block,buf);
	if(!create)
	{
		if(ext2_sb->inode_size<sizeof(struct ext2_inode))
		{
			memset(&file->inode,0,sizeof(struct ext2_inode));
			memcpy(&file->inode,buf+inode_off,ext2_sb->inode_size);
		}
		else
		{
			memcpy(&file->inode,buf+inode_off,sizeof(struct ext2_inode));
		}
	}
	else
	{
		file->inode.atime=ctime;
		file->inode.ctime=ctime;
		file->inode.mtime=ctime;
		file->inode.crtime=ctime;
		file->inode.atime_extra=ctime_extra;
		file->inode.ctime_extra=ctime_extra;
		file->inode.mtime_extra=ctime_extra;
		file->inode.crtime_extra=ctime_extra;
		memset(buf+inode_off,0,ext2_sb->inode_size);
		if(ext2_sb->feature_incompat&FEATURE_EXTENTS)
		{
			file->inode.flags=FLAG_EXTENTS;
			memcpy(file->inode.block,"\x0a\xf3\x00\x00\x04\x00\x00\x00",8);
		}
		ext2_write_block(inode_block,buf);
	}
	unsigned int crc;
	if(ext2_sb->feature_incompat&FEATURE_CSUM_SEED)
	{
		crc=ext2_sb->csum_seed;
	}
	else
	{
		crc=crc32(ext2_sb->uuid,16,0xffffffff);
	}
	crc=crc32(&ninode,4,crc);
	crc=crc32(&file->inode.generation,4,crc);
	if(ext2_sb->feature_incompat&FEATURE_EXTENTS&&file->inode.flags&FLAG_EXTENTS)
	{
		file->cache_crc[0]=1;
		file->cache_crc[1]=1;
		file->cache_crc[2]=1;
		file->cache_crc[3]=1;
		file->cache_crc[4]=1;
		file->cache_crc[5]=1;
	}
	file->crc_seed=crc;
	file->ninode=ninode;
	file->write=write;
	return file;
}
void ext2_dir_write_csum(struct ext2_file *dir);
void file_store_csum(struct ext2_file *file)
{
	int n;
	n=0;
	while(n<6)
	{
		if(file->cache_write[n])
		{
			if(file->cache_crc[n])
			{
				block_update_crc(file->cache+n*1024,file->crc_seed);
			}
			ext2_write_block(file->cache_block[n],file->cache+n*1024);
		}
		++n;
	}
}
void ext2_file_release(struct ext2_file *file)
{
	unsigned char buf[4096];
	unsigned int ninode;
	unsigned long long inode_group,inode_off,inode_block;
	int n;
	if(!file->write)
	{
		free(file);
		return;
	}
	if((file->inode.mode&0170000)==040000)
	{
		ext2_dir_write_csum(file);
	}
	ninode=file->ninode;
	inode_group=(ninode-1)/ext2_sb->inodes_per_group;
	inode_off=(ninode-1)%ext2_sb->inodes_per_group*ext2_sb->inode_size;
	struct ext4_bgdt *desc;
	desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*inode_group);
	inode_block=(inode_off>>ext2_sb->block_size+10)+desc->inode_table;
	if(ext2_desc_size>=64)
	{
		inode_block+=(unsigned long long)desc->inode_table_hi<<32;
	}
	inode_off&=(1<<ext2_sb->block_size+10)-1;
	ext2_read_block(inode_block,buf);
	if(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM)
	{
		unsigned int csum;
		if(ext2_sb->feature_incompat&FEATURE_CSUM_SEED)
		{
			csum=ext2_sb->csum_seed;
		}
		else
		{
			csum=crc32(ext2_sb->uuid,16,0xffffffff);
		}
		csum=crc32(&ninode,4,csum);
		csum=crc32(&file->inode.generation,4,csum);
		file->inode.osd2[2]=0;
		file->inode.checksum_hi&=0xffff;
		csum=crc32(&file->inode,ext2_sb->inode_size,csum);
		file->inode.osd2[2]=csum&0xffff;
		if(file->inode.checksum_hi>=4)
		{
			file->inode.checksum_hi|=csum&0xffff0000;
		}
	}
	if(ext2_sb->inode_size<sizeof(struct ext2_inode))
	{
		memcpy(buf+inode_off,&file->inode,ext2_sb->inode_size);
	}
	else
	{
		memcpy(buf+inode_off,&file->inode,sizeof(struct ext2_inode));
	}
	
	
	ext2_write_block(inode_block,buf);
	file_store_csum(file);
	free(file);
}
unsigned long long int ext2_file_block_old(struct ext2_file *file,unsigned long long int off)
{
	unsigned long long n,n1,n2,off1,off2,off3;
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
				if(file->cache_crc[0])
				{
					block_update_crc(file->cache+0*1024,file->crc_seed);
				}
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
				if(file->cache_crc[1])
				{
					block_update_crc(file->cache+1*1024,file->crc_seed);
				}
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
				if(file->cache_crc[2])
				{
					block_update_crc(file->cache+2*1024,file->crc_seed);
				}
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
				if(file->cache_crc[3])
				{
					block_update_crc(file->cache+3*1024,file->crc_seed);
				}
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
				if(file->cache_crc[4])
				{
					block_update_crc(file->cache+4*1024,file->crc_seed);
				}
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
				if(file->cache_crc[5])
				{
					block_update_crc(file->cache+5*1024,file->crc_seed);
				}
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
unsigned long long int _ext2_file_block_extent(struct ext2_file *file,void *array,int entries,int depth,unsigned long long int off)
{
	struct ext4_extent_header *eh;
	struct ext4_extent *ee;
	struct ext4_extent_index *ei;
	int i;
	unsigned long long val;
	if(depth>5)
	{
		return 0;
	}
	eh=array;
	if(eh->magic!=0xf30a)
	{
		return 0;
	}
	if(eh->entries==0)
	{
		return 0;
	}
	if(eh->entries<entries)
	{
		entries=eh->entries;
	}
	ee=(void *)(eh+1);
	i=1;
	while(i<entries)
	{
		if(ee[i].lblock>off)
		{
			break;
		}
		++i;
	}
	--i;
	if(eh->depth)
	{
		ei=(void *)(ee+i);
		val=ei->block_hi&0xffff;
		val=val<<32|ei->block_lo;
		if(val==0)
		{
			return 0;
		}
		if(file->cache_block[depth]!=val)
		{
			if(file->cache_write[depth])
			{
				if(file->cache_crc[depth])
				{
					block_update_crc(file->cache+depth*1024,file->crc_seed);
				}
				file->cache_write[depth]=0;
				ext2_write_block(file->cache_block[depth],file->cache+depth*1024);
			}
			ext2_read_block(val,file->cache+depth*1024);
			file->cache_block[depth]=val;
		}
		return _ext2_file_block_extent(file,file->cache+depth*1024,(1<<ext2_sb->block_size+10)/12-1,depth+1,off);
	}
	else
	{
		ee+=i;
		val=ee->start_hi;
		val=val<<32|ee->start_lo;
		if(off-ee->lblock>=ee->len)
		{
			return 0;
		}
		return off-ee->lblock+val;
	}
}
unsigned long long int ext2_file_block_extent(struct ext2_file *file,unsigned long long int off)
{
	return _ext2_file_block_extent(file,file->inode.block,4,0,off);
}

unsigned long long ext2_file_block(struct ext2_file *file,unsigned long long int off)
{
	if(ext2_sb->feature_incompat&FEATURE_EXTENTS&&file->inode.flags&FLAG_EXTENTS)
	{
		return ext2_file_block_extent(file,off);
	}
	return ext2_file_block_old(file,off);
}
unsigned long long ext2_file_size_get(struct ext2_file *file)
{
	unsigned long long size;
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
void ext2_file_size_set(struct ext2_file *file,unsigned long long size)
{
	file->inode.size=size;
	if((file->inode.mode&0170000)==0100000)
	{
		file->inode.dir_acl=size>>32;
	}
}
int ext2_file_read(struct ext2_file *file,unsigned long long off,void *buf,unsigned long long int size)
{
	unsigned long long int bn,block,ret,size1;
	unsigned long long fsize;
	unsigned char buf2[4096];
	unsigned long long ctime,ctime_extra;
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
	ctime=time(NULL);
	ctime_extra=ctime>>32&3;
	ctime&=0xffffffff;
	file->inode.atime=ctime;
	file->inode.atime_extra=ctime_extra;
	return ret;
}
unsigned long long int ext2_inode_alloc_group(unsigned long long group)
{
	unsigned char buf[4096];
	unsigned long long int ret;
	unsigned long long val;
	int n;
	struct ext4_bgdt *desc;
	desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*group);
	val=desc->inode_bitmap;
	if(ext2_desc_size>=64)
	{
		val+=(unsigned long long)desc->inode_bitmap_hi<<32;
	}
	if(desc->flags&0x1)
	{
		memset(buf,0xff,4096);
		memset(buf,0,ext2_sb->inodes_per_group>>3);
		desc->flags&=0xfffe;
	}
	else
	{
		ext2_read_block(desc->inode_bitmap,buf);
	}
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
	ext2_write_block(val,buf);
	if(desc->free_inodes==0&&ext2_desc_size>=64)
	{
		--desc->free_inodes_hi;
	}
	
	--desc->free_inodes;
	if(ret-group*ext2_sb->inodes_per_group+desc->unused_inodes>ext2_sb->inodes_per_group)
	{
		if(desc->unused_inodes==0&&ext2_desc_size>=64)
		{
			--desc->unused_inodes_hi;
		}
		--desc->unused_inodes;
	}
	--ext2_sb->free_inodes;
	return ret;
}
unsigned long long int ext2_inode_alloc(unsigned long long dir_inode)
{
	unsigned long long group,n;
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
			struct ext4_bgdt *desc;
			desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*n);
			if(desc->free_inodes||ext2_desc_size>=64&&desc->free_inodes_hi)
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
			struct ext4_bgdt *desc;
			desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*group);
			unsigned long long value;
			value=desc->free_inodes;
			if(ext2_desc_size>=64)
			{
				value+=(unsigned long long)desc->free_inodes_hi;
			}
			if(value)
			{
				return ext2_inode_alloc_group(group);
			}
			++group;
		}
		group=0;
		while(group<ext2_groups)
		{
			struct ext4_bgdt *desc;
			desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*group);
			unsigned long long value;
			value=desc->free_inodes;
			if(ext2_desc_size>=64)
			{
				value+=(unsigned long long)desc->free_inodes_hi;
			}
			if(value)
			{
				return ext2_inode_alloc_group(group);
			}
			++group;
		}
	}
	return 0;
}
void ext2_inode_release(unsigned long long inode)
{
	unsigned long long int group;
	unsigned char buf[4096];
	unsigned long long int n,block;
	if(inode==0||inode>ext2_sb->inodes)
	{
		fatal_error("Inode number out of range");
	}
	group=(inode-1)/ext2_sb->inodes_per_group;
	n=(inode-1)%ext2_sb->inodes_per_group;
	struct ext4_bgdt *desc;
	desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*group);
	unsigned long long value;
	value=desc->inode_bitmap;
	if(ext2_desc_size>=64)
	{
		value+=(unsigned long long)desc->inode_bitmap_hi<<32;
	}
	ext2_read_block(value,buf);
	bitmap_clr(buf,n);
	ext2_write_block(value,buf);
	n*=ext2_sb->inode_size;
	value=desc->inode_table;
	if(ext2_desc_size>=64)
	{
		value+=(unsigned long long)desc->inode_table_hi<<32;
	}
	block=value+(n>>ext2_sb->block_size+10);
	n&=(1<<ext2_sb->block_size+10)-1;
	ext2_read_block(block,buf);
	memset(buf+n,0,ext2_sb->inode_size);
	ext2_write_block(block,buf);
	++desc->free_inodes;
	++ext2_sb->free_inodes;
	if(ext2_desc_size>=64&&desc->free_inodes==0)
	{
		++desc->free_inodes;
	}
}
unsigned long long int ext2_block_alloc_group(unsigned long long group)
{
	unsigned char buf[4096];
	unsigned long long int ret;
	int n;
	struct ext4_bgdt *desc;
	desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*group);
	unsigned long long value,value2;
	value=desc->block_bitmap;
	if(ext2_desc_size>=64)
	{
		value+=(unsigned long long)desc->block_bitmap_hi<<32;
	}
	if(desc->flags&0x2)
	{
		memset(buf,0xff,4096);
		memset(buf,0,ext2_sb->blocks_per_group>>3);
		value2=desc->free_blocks;
		if(ext2_desc_size>=64)
		{
			value2+=(unsigned long long)desc->free_blocks_hi<<16;
		}
		if(group==ext2_groups-1&&ext2_blocks()%ext2_sb->blocks_per_group)
		{
			value2=ext2_blocks()%ext2_sb->blocks_per_group-value2;
		}
		else
		{
			value2=ext2_sb->blocks_per_group-value2;
		}
		if(value2>8<<ext2_sb->block_size+10)
		{
			value2=8<<ext2_sb->block_size+10;
		}
		memset(buf,0xff,value2>>3);
		buf[value2>>3]|=(1<<(value2&7))-1;
		desc->flags&=0xfffd;
	}
	else
	{
		ext2_read_block(value,buf);
	}
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
	if(ret>=ext2_blocks())
	{
		return 0;
	}
	bitmap_set(buf,n);
	ext2_write_block(value,buf);
	if(desc->free_blocks==0&&ext2_desc_size>=64)
	{
		--desc->free_blocks_hi;
	}
	--desc->free_blocks;
	if(ext2_sb->free_blocks==0)
	{
		--ext2_sb->free_blocks_hi;
	}
	--ext2_sb->free_blocks;
	return ret;
}
unsigned long long int ext2_block_alloc(unsigned long long inode)
{
	unsigned long long group,n;
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
		struct ext4_bgdt *desc;
		desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*n);
		unsigned long long value;
		value=desc->free_blocks;
		if(ext2_desc_size>=64)
		{
			value+=(unsigned long long)desc->free_blocks_hi<<16;
		}
		if(value)
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
void ext2_block_release(unsigned long long int block)
{
	unsigned long long int group;
	unsigned char buf[4096];
	unsigned long long int n;
	if(block>=ext2_blocks())
	{
		fatal_error("Block number out of range");
	}
	if(ext2_sb->block_size==0)
	{
		--block;
	}
	group=block/ext2_sb->blocks_per_group;
	n=block%ext2_sb->blocks_per_group;
	struct ext4_bgdt *desc;
	desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*group);
	unsigned long long value;
	value=desc->block_bitmap;
	if(ext2_desc_size>=64)
	{
		value+=(unsigned long long)desc->block_bitmap_hi<<32;
	}
	ext2_read_block(value,buf);
	bitmap_clr(buf,n);
	ext2_write_block(value,buf);
	
	++desc->free_blocks;
	if(desc->free_blocks==0&&ext2_desc_size>=64)
	{
		++desc->free_blocks_hi;
	}
	++ext2_sb->free_blocks;
	if(ext2_sb->free_blocks==0)
	{
		++ext2_sb->free_blocks_hi;
	}
}
unsigned long long int ext2_file_alloc_block_old(struct ext2_file *file,unsigned long long int off)
{
	unsigned long long n,n1,n2,off1,off2,off3,ret;
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
				if(file->cache_crc[0])
				{
					block_update_crc(file->cache+0*1024,file->crc_seed);
				}
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
				if(file->cache_crc[1])
				{
					block_update_crc(file->cache+1*1024,file->crc_seed);
				}
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
				if(file->cache_crc[2])
				{
					block_update_crc(file->cache+2*1024,file->crc_seed);
				}
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
				if(file->cache_crc[3])
				{
					block_update_crc(file->cache+3*1024,file->crc_seed);
				}
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
				if(file->cache_crc[4])
				{
					block_update_crc(file->cache+4*1024,file->crc_seed);
				}
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
				if(file->cache_crc[5])
				{
					block_update_crc(file->cache+5*1024,file->crc_seed);
				}
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
unsigned long long int _ext2_file_alloc_block_extent(struct ext2_file *file,void *array,int entries,int depth,unsigned long long int off,int depth2)
{
	struct ext4_extent_header *eh,*eh2;
	struct ext4_extent *ee,*ee2;
	struct ext4_extent_index *ei,*ei2;
	unsigned long long block;
	unsigned int *checksum;
	int i,alloc;
	unsigned long long val,ret;
	
	if(depth>5)
	{
		return 0;
	}
	eh=array;
	if(depth2==-1)
	{
		depth2=eh->depth;
	}
	if(eh->magic!=0xf30a)
	{
		return 0;
	}
	if(eh->entries<entries)
	{
		entries=eh->entries;
	}
	if(entries==0)
	{
		return 0;
	}
	ee=(void *)(eh+1);
	i=1;
	while(i<entries)
	{
		if(ee[i].lblock>off)
		{
			break;
		}
		++i;
	}
	--i;
	if(eh->depth)
	{
		alloc=0;
		ei=(void *)(ee+i);
		val=ei->block_hi&0xffff;
		val=val<<32|ei->block_lo;
		eh2=(void *)(file->cache+depth*1024);
		if(val==0)
		{
			val=ext2_block_alloc(file->ninode);
			if(val==0)
			{
				return 0;
			}
			file->inode.blocks+=1<<ext2_sb->block_size+1;
			ei->block_lo=val;
			ei->block_hi=val>>32&0xffff;
			if(file->cache_write[depth])
			{
				block_update_crc(file->cache+depth*1024,file->crc_seed);
				ext2_write_block(file->cache_block[depth],file->cache+depth*1024);
				file->cache_write[depth]=0;
			}
			
			file->cache_block[depth]=val;
			memset(file->cache+depth*1024,0,4096);
			alloc=1;
			eh2->magic=0xf30a;
			eh2->entries=1;
			eh2->max_entries=(1<<ext2_sb->block_size+10)/12-1;
			ei2=(void *)(eh2+1);
			ei2->lblock=off;
		}
		else
		{
			if(file->cache_write[depth]&&file->cache_block[depth]!=val)
			{
				block_update_crc(file->cache+depth*1024,file->crc_seed);
				ext2_write_block(file->cache_block[depth],file->cache+depth*1024);
				file->cache_write[depth]=0;	
				ext2_read_block(val,file->cache+depth*1024);
				file->cache_block[depth]=val;
			}
		}
		
		block=_ext2_file_alloc_block_extent(file,eh2,(1<<ext2_sb->block_size+10)/12-1,depth+1,off,depth2-1);
		if(block==0)
		{
			if(eh2->entries<eh2->max_entries)
			{
				++eh2->entries;
				ei2=(void *)(eh2+eh2->entries);
				ei2->lblock=off;
				block=_ext2_file_alloc_block_extent(file,eh2,(1<<ext2_sb->block_size+10)/12-1,depth+1,off,depth2-1);
				if(block!=0)
				{
					file->cache_write[depth]=1;
					return block;
				}
				memset(ei2,0,sizeof(*ei2));
				--eh2->entries;
			}
			if(alloc)
			{
				ext2_block_release(val);
				file->inode.blocks-=1<<ext2_sb->block_size+1;
				ei->block_lo=0;
				ei->block_hi=0;
			}
			return 0;
		}
		file->cache_write[depth]=1;
		return block;
	}
	else
	{
		ee+=i;
		val=ee->start_hi;
		val=val<<32|ee->start_lo;
		if(val!=0)
		{
			if(off>=ee->lblock&&off<ee->lblock+ee->len)
			{
				return val+off-ee->lblock;
			}
			if(ee->len>=32767)
			{
				return 0;
			}
			block=ext2_block_alloc(file->ninode);
			if(block==0)
			{
				return 0;
			}
			file->inode.blocks+=1<<ext2_sb->block_size+1;
			if(block!=val+ee->len)
			{
				file->inode.blocks-=1<<ext2_sb->block_size+1;
				ext2_block_release(block);
				return 0;
			}
			ee->len+=1;
			return block;
		}
		val=ext2_block_alloc(file->ninode);
		if(val==0)
		{
			return 0;
		}
		file->inode.blocks+=1<<ext2_sb->block_size+1;
		ee->lblock=off;
		ee->start_lo=val;
		ee->start_hi=val>>32;
		ee->len=1;
		return val;
	}
}
unsigned long long int ext2_file_alloc_block_extent(struct ext2_file *file,unsigned long long int off)
{
	unsigned long long ret,block;
	struct ext4_extent_header *eh,*eh2;
	struct ext4_extent_index *ei;
	ret=_ext2_file_alloc_block_extent(file,file->inode.block,4,0,off,-1);
	if(ret!=0)
	{
		return ret;
	}
	eh=(void *)file->inode.block;
	if(eh->entries<4)
	{
		++eh->entries;
		ei=(void *)(eh+eh->entries);
		memset(ei,0,sizeof(*ei));
		ei->lblock=off;
		ret=_ext2_file_alloc_block_extent(file,file->inode.block,4,0,off,-1);
		if(ret!=0)
		{
			return ret;
		}
		memset(ei,0,sizeof(*ei));
		--eh->entries;
		return 0;
	}
	block=ext2_block_alloc(file->ninode);
	if(block==0)
	{
		return 0;
	}
	file->inode.blocks+=1<<ext2_sb->block_size+1;
	file_store_csum(file);
	memset(file->cache_block,0,48);
	memset(file->cache_write,0,48);
	memset(file->cache,0,4096);
	memcpy(file->cache,file->inode.block,60);
	eh=(void *)file->cache;
	eh->entries=5;
	eh->max_entries=(1<<ext2_sb->block_size+10)/12-1;
	ei=(void *)(eh+eh->entries);
	ei->lblock=off;
	ret=_ext2_file_alloc_block_extent(file,eh,(1<<ext2_sb->block_size+10)/12-1,0,off,-1);
	if(ret==0)
	{
		ext2_block_release(block);
		file->inode.blocks-=1<<ext2_sb->block_size+1;
		return 0;
	}
	file->cache_block[0]=block;
	file->cache_write[0]=1;
	eh2=(void *)file->inode.block;
	memset(eh2,0,60);
	eh2->magic=0xf30a;
	eh2->entries=1;
	eh2->max_entries=4;
	eh2->depth=eh->depth+1;
	ei=(void *)(eh2+1);
	ei->lblock=0;
	ei->block_lo=block;
	ei->block_hi=block>>32&0xffff;
	return ret;
}
unsigned long long ext2_file_alloc_block(struct ext2_file *file,unsigned long long int off)
{
	if(ext2_sb->feature_incompat&FEATURE_EXTENTS&&file->inode.flags&FLAG_EXTENTS)
	{
		return ext2_file_alloc_block_extent(file,off);
	}
	return ext2_file_alloc_block_old(file,off);
}
int ext2_file_write(struct ext2_file *file,unsigned long long off,void *buf,unsigned long long int size)
{
	unsigned long long int bn,block,ret,size1;
	unsigned long long fsize,start;
	unsigned char buf2[4096];
	unsigned long long ctime,ctime_extra;
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
	ctime=time(NULL);
	ctime_extra=ctime>>32&3;
	ctime&=0xffffffff;
	file->inode.atime=ctime;
	file->inode.ctime=ctime;
	file->inode.atime_extra=ctime_extra;
	file->inode.ctime_extra=ctime_extra;
	return ret;
}
void ext2_sync(void)
{
	unsigned char sb[4096];
	unsigned long long int sb_block,sb_off;
	int n;
	sb_block=0;
	sb_off=1024;
	if(ext2_sb->block_size==0)
	{
		sb_block=1;
		sb_off=0;
	}
	ext2_read_block(sb_block,sb);
	if(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM)
	{
		ext2_sb->checksum=crc32(ext2_sb,1020,0xffffffff);
	}
	memcpy(sb+sb_off,ext2_sb,1024);
	ext2_write_block(sb_block,sb);
	n=0;
	while(n<ext2_groups)
	{
		struct ext4_bgdt *desc;
		desc=(void *)((char *)ext2_bgdt_array+n*ext2_desc_size);
		desc->checksum=0;
		unsigned int value,size;
		unsigned long long block;
		unsigned char buf[4096];

		block=desc->block_bitmap;
		if(ext2_desc_size>=64)
		{
			block|=(unsigned long long)desc->block_bitmap_hi<<32;
		}
		ext2_read_block(block,buf);
		size=ext2_sb->blocks_per_group;
		if(size>32768)
		{
			size=32768;
		}
		if(ext2_sb->feature_incompat&FEATURE_CSUM_SEED)
		{
			value=ext2_sb->csum_seed;
		}
		else
		{
			value=crc32(ext2_sb->uuid,16,0xffffffff);
		}
		value=crc32(buf,size>>3,value);
		if(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM)
		{
			desc->block_bitmap_csum=value;
			if(ext2_desc_size>=64)
			{
				desc->block_bitmap_csum_hi=value>>16;
			}
		}

		block=desc->inode_bitmap;
		if(ext2_desc_size>=64)
		{
			block|=(unsigned long long)desc->inode_bitmap_hi<<32;
		}
		ext2_read_block(block,buf);
		size=ext2_sb->inodes_per_group;
		if(size>32768)
		{
			size=32768;
		}
		if(ext2_sb->feature_incompat&FEATURE_CSUM_SEED)
		{
			value=ext2_sb->csum_seed;
		}
		else
		{
			value=crc32(ext2_sb->uuid,16,0xffffffff);
		}
		value=crc32(buf,size>>3,value);
		if(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM)
		{
			desc->inode_bitmap_csum=value;
			if(ext2_desc_size>=64)
			{
				desc->inode_bitmap_csum_hi=value>>16;
			}
		}

		if(ext2_sb->feature_incompat&FEATURE_CSUM_SEED)
		{
			value=ext2_sb->csum_seed;
		}
		else
		{
			value=crc32(ext2_sb->uuid,16,0xffffffff);
		}
		value=crc32(&n,4,value);
		value=crc32(desc,32,value);
		if(ext2_desc_size>=64)
		{
			value=crc32((char *)desc+32,32,value);
		}
		if(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM)
		{
			desc->checksum=value;
		}
		++n;
	}
	n=0;
	while(n<ext2_bgdt_blocks)
	{
		ext2_write_block(sb_block+n+1,(char *)ext2_bgdt_array+(n<<ext2_sb->block_size+10));
		++n;
	}
	global_cache_flush_all();
}

long long ext2_readdir(struct ext2_file *dir,long long off,void *dirent)
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
void ext2_dir_write_csum(struct ext2_file *dir)
{
	unsigned int crc;
	unsigned long long size,off,csumoff;
	char buf[4096];
	if(!(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM))
	{
		return;
	}
	size=ext2_file_size_get(dir);
	csumoff=(1<<ext2_sb->block_size+10)-12;
	off=0;
	while(off<size)
	{
		if(ext2_file_read(dir,off<<ext2_sb->block_size+10,buf,1<<ext2_sb->block_size+10)==0)
		{
			return;
		}
		crc=crc32(buf,csumoff,dir->crc_seed);
		*(unsigned int *)(buf+csumoff+8)=crc;
		ext2_file_write(dir,off<<ext2_sb->block_size+10,buf,1<<ext2_sb->block_size+10);
		off+=1;
	}
}
unsigned long long int ext2_search(unsigned long long int inode,char *name)
{
	char buf[4096];
	struct ext2_file *dir;
	struct ext2_directory *dirent;
	long long off;
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
		
		if(dirent->inode&&(unsigned long long int)dirent->name_len==len&&!memcmp(dirent->file_name,name,len))
		{
			ext2_file_release(dir);
			return dirent->inode;
		}
	}
	ext2_file_release(dir);
	return 0;
}
int ext2_search_path(unsigned long long int start_inode,char *name,unsigned int *inode)
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
unsigned long long int ext2_get_dname(unsigned int inode,char *name)
{
	char buf[4096];
	struct ext2_file *dir;
	struct ext2_directory *dirent;
	long long off;
	int len;
	unsigned long long int parent_inode;
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
			memcpy(name,dirent->file_name,(unsigned long long int)dirent->name_len);
			name[(unsigned long long int)dirent->name_len]=0;
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
	unsigned long long int inode;
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
int ext2_mkdir(unsigned long long int dir,unsigned long long int inode)
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
	if(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM)
	{
		dirent->rec_len=(1<<ext2_sb->block_size+10)-24;
		dirent=(void *)((char *)dirent+dirent->rec_len);
		dirent->inode=0;
		dirent->rec_len=12;
		dirent->name_len=0;
		dirent->file_type=0xde;
	}
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
	long long off;
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
				if(dirent->inode&&(unsigned long long int)dirent->name_len==x&&!memcmp(dirent->file_name,buf,x))
				{
					return -1;
				}
			}
			off=0;
			reclen=(x-1>>2)+3<<2;
			while(off=ext2_readdir(dir,off,dirent))
			{
				if(dirent->inode==0&&dirent->rec_len>=reclen&&dirent->file_type!=0xde)
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
					unsigned long long int l;
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
			if(ext2_sb->feature_ro_compat&FEATURE_METADATA_CSUM)
			{
				dirent->rec_len-=12;
				dirent=(void *)((char *)dirent+dirent->rec_len);
				dirent->inode=0;
				dirent->rec_len=12;
				dirent->name_len=0;
				dirent->file_type=0xde;
			}
			if(ext2_file_write(dir,off,buf2,1<<ext2_sb->block_size+10)!=1<<ext2_sb->block_size+10)
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
	long long off;
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
	if(file->inode.links==65535||file->inode.links==1)
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
	struct ext4_bgdt *desc;
	desc=(void *)((char *)ext2_bgdt_array+ext2_desc_size*group);
	if(desc->used_dirs==65535)
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
	++desc->used_dirs;
	ext2_file_release(file);
	return 0;
}
void ext2_release_blocks_old(unsigned long long int n,unsigned int level)
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
			ext2_release_blocks_old(buf[x],level-1);
			++x;
		}
	}
	ext2_block_release(n);
}
void ext2_release_file_blocks_old(struct ext2_file *file)
{
	int x;
	x=0;
	while(x<12)
	{
		ext2_release_blocks_old(file->inode.block[x],0);
		++x;
	}
	ext2_release_blocks_old(file->inode.block[12],1);
	ext2_release_blocks_old(file->inode.block[13],2);
	ext2_release_blocks_old(file->inode.block[14],3);
	
}
void ext2_release_blocks_extent(unsigned long long int n,int depth)
{
	char buf[4096];
	struct ext4_extent_header *eh;
	struct ext4_extent *ee;
	struct ext4_extent_index *ei;
	unsigned long long val;
	if(depth>5)
	{
		return;
	}
	int i,j;
	ext2_read_block(n,buf);
	eh=(void *)buf;
	i=0;
	if(eh->depth)
	{
		ei=(void *)(eh+1);
		while(i<eh->entries&&i<(1<<ext2_sb->block_size+10)/12-1)
		{
			val=ei[i].block_hi&0xffff;
			val=val<<32|ei[i].block_lo;
			ext2_release_blocks_extent(val,depth+1);
			++i;
		}
	}
	else
	{
		ee=(void *)(eh+1);
		while(i<eh->entries&&i<(1<<ext2_sb->block_size+10)/12-1)
		{
			val=ee[i].start_hi&0xffff;
			val=val<<32|ee[i].start_lo;
			if(!(ee[i].len&0x8000))
			{
				j=0;
				while(j<ee[i].len)
				{
					ext2_block_release(val+j);
					++j;
				}
			}
			++i;
		}
	}
	ext2_block_release(n);
}
void ext2_release_file_blocks_extent(struct ext2_file *file)
{
	struct ext4_extent_header *eh;
	struct ext4_extent *ee;
	struct ext4_extent_index *ei;
	unsigned long long val;
	int i,j;
	eh=(void *)file->inode.block;
	i=0;
	if(eh->depth)
	{
		ei=(void *)(eh+1);
		while(i<eh->entries&&i<4)
		{
			val=ei[i].block_hi&0xffff;
			val=val<<32|ei[i].block_lo;
			ext2_release_blocks_extent(val,0);
			++i;
		}
	}
	else
	{
		ee=(void *)(eh+1);
		while(i<eh->entries&&i<4)
		{
			val=ee[i].start_hi&0xffff;
			val=val<<32|ee[i].start_lo;
			j=0;
			if(!(ee[i].len&0x8000))
			{
				while(j<ee[i].len)
				{
					ext2_block_release(val+j);
					++j;
				}
			}
			++i;
		}
	}
}
void ext2_release_file_blocks(struct ext2_file *file)
{
	if(ext2_sb->feature_incompat&FEATURE_EXTENTS&&file->inode.flags&FLAG_EXTENTS)
	{
		ext2_release_file_blocks_extent(file);
	}
	else
	{
		ext2_release_file_blocks_old(file);
	}
	file->inode.blocks=0;
	file->inode.size=0;
	memset(file->inode.block,0,sizeof(file->inode.block));
	memset(file->cache,0,sizeof(file->cache));
	memset(file->cache_block,0,sizeof(file->cache_block));
	memset(file->cache_write,0,sizeof(file->cache_write));
}
