unsigned char crc_reverse(unsigned char val)
{
	unsigned char ret;
	ret=val&1;
	ret=ret<<1|val>>1&1;
	ret=ret<<1|val>>2&1;
	ret=ret<<1|val>>3&1;
	ret=ret<<1|val>>4&1;
	ret=ret<<1|val>>5&1;
	ret=ret<<1|val>>6&1;
	ret=ret<<1|val>>7&1;
	return ret;
}
unsigned int crc32(void *buf,unsigned int size,unsigned int init)
{
	unsigned int regs,c,x,old_regs;
	unsigned char *val;
	old_regs=init;
	regs=crc_reverse(old_regs);
	regs=regs<<8|crc_reverse(old_regs>>8);
	regs=regs<<8|crc_reverse(old_regs>>16);
	regs=regs<<8|crc_reverse(old_regs>>24);
	val=buf;
	while(size)
	{
		c=crc_reverse(*val);
		x=8;
		do
		{
			--x;
			old_regs=regs;
			regs<<=1;
			if(old_regs>>31^c>>x&1)
			{
				regs^=0x1edc6f41;
			}
		}
		while(x);
		++val;
		--size;
	}
	old_regs=regs;
	regs=crc_reverse(old_regs);
	regs=regs<<8|crc_reverse(old_regs>>8);
	regs=regs<<8|crc_reverse(old_regs>>16);
	regs=regs<<8|crc_reverse(old_regs>>24);
	return regs;
}