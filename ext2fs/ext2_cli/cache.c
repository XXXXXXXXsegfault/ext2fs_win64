#define GLOBAL_CACHE_PAGE_SIZE 32
#define GLOBAL_CACHE_PAGES_MAX 256
char global_cache_data[GLOBAL_CACHE_PAGE_SIZE*4096*GLOBAL_CACHE_PAGES_MAX];
long global_cache_start_addr[GLOBAL_CACHE_PAGES_MAX];
long global_cache_write_mark[GLOBAL_CACHE_PAGES_MAX];
int global_cache_page_find(long addr)
{
	int n,best_n;
	long best_addr,val;
	n=0;
	best_n=-1;
	best_addr=-1;
	while(n<GLOBAL_CACHE_PAGES_MAX)
	{
		val=global_cache_start_addr[n];
		if(val>=0&&val>=addr&&(best_addr<0||val<best_addr))
		{
			best_addr=val;
			best_n=n;
		}
		++n;
	}
	return best_n;
}
int global_cache_use(long addr)
{
	int n;
	n=0;
	while(n<GLOBAL_CACHE_PAGES_MAX)
	{
		if(global_cache_start_addr[n]<0)
		{
			global_cache_start_addr[n]=addr;
			return n;
		}
		++n;
	}
	return -1;
}
void global_cache_flush(void)
{
	static int i;
	int n,s,s1;
	
	s=GLOBAL_CACHE_PAGE_SIZE/4;
	s1=1;
	while(s1)
	{
		n=0;
		while(n<GLOBAL_CACHE_PAGES_MAX)
		{
			if(i<5&&global_cache_start_addr[n]>=0)
			{
				if(global_cache_write_mark[n]>s)
				{
					write_raw_blocks(global_cache_data+n*GLOBAL_CACHE_PAGE_SIZE*4096,global_cache_start_addr[n],GLOBAL_CACHE_PAGE_SIZE);
					global_cache_write_mark[n]=0;
					global_cache_start_addr[n]=-1;
					s1=0;
				}
				if(!global_cache_write_mark[n]&&i<2)
				{
					global_cache_start_addr[n]=-1;
					s1=0;
				}
			}
			++i;
			if(i==7)
			{
				i=0;
			}
			++n;
		}
		if(s)
		{
			--s;
		}
	}
}
void global_cache_flush_all(void)
{
	int n;
	n=0;
	while(n<GLOBAL_CACHE_PAGES_MAX)
	{
		if(global_cache_start_addr[n]>=0)
		{
			if(global_cache_write_mark[n])
			{
				write_raw_blocks(global_cache_data+n*GLOBAL_CACHE_PAGE_SIZE*4096,global_cache_start_addr[n],GLOBAL_CACHE_PAGE_SIZE);
				global_cache_write_mark[n]=0;
			}
			global_cache_start_addr[n]=-1;
		}
		++n;
	}
}
void ext2_read_block(unsigned long bn,void *buf)
{
	int block_size_shift;
	int bn_lo;
	int cache_n;
	
	block_size_shift=2-ext2_sb->block_size;
	bn_lo=bn%((1<<block_size_shift)*GLOBAL_CACHE_PAGE_SIZE);
	bn/=(1<<block_size_shift)*GLOBAL_CACHE_PAGE_SIZE;

	cache_n=global_cache_page_find(bn*GLOBAL_CACHE_PAGE_SIZE);
	if(cache_n>=0&&global_cache_start_addr[cache_n]==bn*GLOBAL_CACHE_PAGE_SIZE)
	{
		memcpy(buf,global_cache_data+cache_n*GLOBAL_CACHE_PAGE_SIZE*4096+bn_lo*(4096>>block_size_shift),4096>>block_size_shift);
		return;
	}
	cache_n=global_cache_use(bn*GLOBAL_CACHE_PAGE_SIZE);
	if(cache_n<0)
	{
		global_cache_flush();
		cache_n=global_cache_use(bn*GLOBAL_CACHE_PAGE_SIZE);
	}
	read_raw_blocks(global_cache_data+cache_n*GLOBAL_CACHE_PAGE_SIZE*4096,bn*GLOBAL_CACHE_PAGE_SIZE,GLOBAL_CACHE_PAGE_SIZE);
	memcpy(buf,global_cache_data+cache_n*GLOBAL_CACHE_PAGE_SIZE*4096+bn_lo*(4096>>block_size_shift),4096>>block_size_shift);
}
void ext2_write_block(unsigned long bn,void *buf)
{
	int block_size_shift;
	int bn_lo;
	int cache_n;

	block_size_shift=2-ext2_sb->block_size;
	bn_lo=bn%((1<<block_size_shift)*GLOBAL_CACHE_PAGE_SIZE);
	bn/=(1<<block_size_shift)*GLOBAL_CACHE_PAGE_SIZE;
	
	cache_n=global_cache_page_find(bn*GLOBAL_CACHE_PAGE_SIZE);
	if(cache_n>=0&&global_cache_start_addr[cache_n]==bn*GLOBAL_CACHE_PAGE_SIZE)
	{
		memcpy(global_cache_data+cache_n*GLOBAL_CACHE_PAGE_SIZE*4096+bn_lo*(4096>>block_size_shift),buf,4096>>block_size_shift);
		global_cache_write_mark[cache_n]+=1;
		return;
	}
	cache_n=global_cache_use(bn*GLOBAL_CACHE_PAGE_SIZE);
	if(cache_n<0)
	{
		global_cache_flush();
		cache_n=global_cache_use(bn*GLOBAL_CACHE_PAGE_SIZE);
	}
	read_raw_blocks(global_cache_data+cache_n*GLOBAL_CACHE_PAGE_SIZE*4096,bn*GLOBAL_CACHE_PAGE_SIZE,GLOBAL_CACHE_PAGE_SIZE);
	memcpy(global_cache_data+cache_n*GLOBAL_CACHE_PAGE_SIZE*4096+bn_lo*(4096>>block_size_shift),buf,4096>>block_size_shift);
	global_cache_write_mark[cache_n]+=1;
}
void cache_init(void)
{
	memset(global_cache_start_addr,0xcc,sizeof(global_cache_start_addr));
}
