unsigned int crc32(void *buf,unsigned int size,unsigned int init)
{
	unsigned int regs,c,x,old_regs;
	unsigned char *val;
	regs=init;
	val=buf;
	while(size)
	{
		c=*val;
		x=0;
		do
		{
			old_regs=regs;
			regs>>=1;
			if((old_regs^c>>x)&1)
			{
				regs^=0x82f63b78;
			}
			++x;
		}
		while(x<8);
		++val;
		--size;
	}
	return regs;
}
