void sprinti(char *str,unsigned long int a,int digits)
{
	unsigned long int n;
	int d,l,sl;
	char buf[20];
	n=10000000000000000000;
	d=20;
	while(n>a&&d>digits)
	{
		n/=10;
		--d;
	}
	l=0;
	while(n)
	{
		buf[l]=a/n+'0';
		a%=n;
		n/=10;
		++l;
	}
	sl=strlen(str);
	memcpy(str+sl,buf,l);
	str[sl+l]=0;
}
