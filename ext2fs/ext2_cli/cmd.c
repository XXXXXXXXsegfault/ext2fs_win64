#define INPUT_SIZE 65536
#define ARGC_MAX 8
void cmd_help(int argc,char **argv)
{
	if(argc>1)
	{
		if(!strcmp(argv[1],"help"))
		{
			puts("Usage:\n");
			puts("help -- display list of commands\n");
			puts("help <command> -- display information about a specific command\n");
			return;
		}
		if(!strcmp(argv[1],"exit"))
		{
			puts("Usage:\n");
			puts("exit -- save changes and exit\n");
			return;
		}
		if(!strcmp(argv[1],"ls"))
		{
			puts("Usage:\n");
			puts("ls -- list files in the current directory\n");
			puts("ls <dir> -- list files\n");
			return;
		}
		if(!strcmp(argv[1],"cd"))
		{
			puts("Usage:\n");
			puts("cd <dir> -- change current directory\n");
			return;
		}
		if(!strcmp(argv[1],"pwd"))
		{
			puts("Usage:\n");
			puts("pwd -- display current path\n");
			return;
		}
		if(!strcmp(argv[1],"pull"))
		{
			puts("Usage:\n");
			puts("pull <ext2Path> <windowsPath> -- export file\n");
			return;
		}
		if(!strcmp(argv[1],"mkdir"))
		{
			puts("Usage:\n");
			puts("mkdir <name> -- create directory\n");
			return;
		}
		if(!strcmp(argv[1],"push"))
		{
			puts("Usage:\n");
			puts("push <windowsPath> <ext2Path> -- import file\n");
			return;
		}
		if(!strcmp(argv[1],"du"))
		{
			puts("Usage:\n");
			puts("du <name> -- display space usage of file or directory\n");
			return;
		}
		if(!strcmp(argv[1],"remove"))
		{
			puts("Usage:\n");
			puts("remove <name> -- remove file or directory\n");
			return;
		}
		if(!strcmp(argv[1],"move"))
		{
			puts("Usage:\n");
			puts("move <name> <newname> -- rename or move file\n");
			return;
		}
		puts("No such command.\n");
		return;
	}
	puts("Commands:\n");
	puts("cd -- change current directory\n");
	puts("du -- display storage space usage\n");
	puts("exit -- save changes and exit\n");
	puts("help -- display list of commands or information about a specific command\n");
	puts("ls -- list files\n");
	puts("mkdir -- create directory\n");
	puts("move -- rename or move file\n");
	puts("pull -- export file\n");
	puts("push -- import file\n");
	puts("pwd -- display current path\n");
	puts("remove -- remove file or directory\n");
}
struct cmd_ls_sort
{
	struct cmd_ls_sort *next;
	char name[256];
	long is_dir;
};
void cmd_ls(int argc,char **argv)
{
	struct ext2_file *dir,*file;
	long off;
	char buf[4096];
	struct ext2_directory *dirent;
	struct cmd_ls_sort *list,*list_node,*p,*cur;
	int x;
	if(argc<2)
	{
		dir=ext2_file_load(current_dir,0,0);
		if(dir==NULL)
		{
			puts("Cannot read directory\n");
			return;
		}
	}
	else
	{
		unsigned int inode;
		int ret;
		ret=ext2_search_path(current_dir,argv[1],&inode);
		if(ret==-1)
		{
			puts("File name too long\n");
			return;
		}
		if(ret==-2)
		{
			puts("Error opening directory\n");
			return;
		}
		dir=ext2_file_load(inode,0,0);
		if(dir==NULL)
		{
			puts("Cannot read directory\n");
			return;
		}
	}
	if((dir->inode.mode&0170000)!=040000)
	{
		puts("Not a directory\n");
		ext2_file_release(dir);
		return;
	}
	dirent=(void *)buf;
	off=0;
	list=NULL;
	while((off=ext2_readdir(dir,off,dirent))!=0)
	{
		if(dirent->inode)
		{
			list_node=malloc(sizeof(*list_node));
			if(list_node)
			{
				memset(list_node,0,sizeof(*list_node));
				memcpy(list_node->name,dirent->file_name,(unsigned int)dirent->name_len);
				file=ext2_file_load(dirent->inode,0,0);
				if(file)
				{
					if((file->inode.mode&0170000)==040000)
					{
						list_node->is_dir=1;
					}
					ext2_file_release(file);
				}
				p=NULL;
				cur=list;
				while(cur&&strcmp(cur->name,list_node->name)<0)
				{
					p=cur;
					cur=cur->next;
				}
				list_node->next=cur;
				if(p)
				{
					p->next=list_node;
				}
				else
				{
					list=list_node;
				}
			}
		}
	}
	ext2_file_release(dir);
	while(list)
	{
		x=0;
		while(list->name[x])
		{
			if(list->name[x]==' '||list->name[x]=='\\')
			{
				putchar('\\');
			}
			putchar(list->name[x]);
			++x;
		}
		if(list->is_dir)
		{
			putchar('/');
		}
		p=list;
		list=p->next;
		free(p);
		puts("   ");
	}
	putchar('\n');
}
void cmd_cd(int argc,char **argv)
{
	struct ext2_file *file;
	if(argc<2)
	{
		puts("Too few arguments\n");
		return;
	}
	unsigned int inode;
	int ret;
	ret=ext2_search_path(current_dir,argv[1],&inode);
	if(ret==-1)
	{
		puts("File name too long\n");
		return;
	}
	if(ret==-2)
	{
		puts("Error opening directory\n");
		return;
	}
	file=ext2_file_load(inode,0,0);
	if(file==NULL)
	{
		puts("Error opening directory\n");
		return;
	}
	if((file->inode.mode&0170000)!=040000)
	{
		ext2_file_release(file);
		puts("Not a directory\n");
		return;
	}
	ext2_file_release(file);
	current_dir=inode;
}
struct cmd_pwd_stack
{
	char name[256];
	struct cmd_pwd_stack *next;
};
void cmd_pwd(int argc,char **argv)
{
	struct cmd_pwd_stack *list,*node;
	unsigned int inode;
	if(current_dir==2)
	{
		puts("/\n");
		return;
	}
	list=NULL;
	inode=current_dir;
	while(inode!=2)
	{
		node=malloc(sizeof(*node));
		if(node==NULL)
		{
			while(list)
			{
				node=list;
				list=node->next;
				free(node);
			}
			puts("Cannot allocate memory\n");
			return;
		}
		inode=ext2_get_dname(inode,node->name);
		if(inode==0)
		{
			free(node);
			while(list)
			{
				node=list;
				list=node->next;
				free(node);
			}
			puts("Error reading directory\n");
			return;
		}
		node->next=list;
		list=node;
	}
	while(list)
	{
		puts("/");
		puts(list->name);
		node=list;
		list=node->next;
		free(node);
	}
	puts("\n");
}
void file_name_append(char *buf,char *name)
{
	char c;
	buf+=strlen(buf);
	while(c=*name)
	{
		if(c=='\?'||c=='*'||c=='<'||c=='>'||c=='/'||c=='\\'||c=='|'||c==':'||c=='\"'||c<32||c>126)
		{
			c='_';
		}
		*buf=c;
		++buf;
		++name;
	}
	*buf=0;
}
int pull_file(char *src,char *dst)
{
	void *buf;
	struct ext2_file *file;
	unsigned int inode,n;
	void *fp;
	long off;
	char buf2[256];
	
	if(ext2_search_path(current_dir,src,&inode))
	{
		puts(src);
		puts(": Cannot open file for reading\n");
		return 0;
	}
	file=ext2_file_load(inode,0,0);
	if(file==NULL)
	{
		puts(src);
		puts(": Cannot open file for reading\n");
		return 0;
	}
	if((file->inode.mode&0170000)==0100000)
	{
		buf=malloc(65536);
		if(buf==NULL)
		{
			puts("Cannot allocate memory\n");
			ext2_file_release(file);
			return -1;
		}
		if(fp=fopen(dst,"rb"))
		{
			fclose(fp);
			free(buf);
			puts(dst);
			puts(": Cannot create file\n");
			ext2_file_release(file);
			return 0;
		}
		if((fp=fopen(dst,"wb"))==NULL)
		{
			free(buf);
			puts(dst);
			puts(": Cannot create file\n");
			ext2_file_release(file);
			return 0;	
		}
		off=0;
		while(n=ext2_file_read(file,off,buf,65536))
		{
			if(fwrite(buf,1,n,fp)!=n)
			{
				fclose(fp);
				ext2_file_release(file);
				free(buf);
				puts(dst);
				puts(": No space available\n");
				return -1;
			}
			off+=n;
		}
		fclose(fp);
		ext2_file_release(file);
		free(buf);
		return 0;
	}
	if((file->inode.mode&0170000)==040000)
	{
		buf=malloc(4096);
		if(buf==NULL)
		{
			puts("Cannot allocate memory\n");
			ext2_file_release(file);
			return -1;
		}
		struct ext2_directory *dirent;
		dirent=buf;
		if(!CreateDirectoryA(dst,NULL))
		{
			free(buf);
			puts(dst);
			puts(": Cannot create directory\n");
			ext2_file_release(file);
			return 0;
		}
		char *new_src,*new_dst;
		new_src=malloc(strlen(src)+264);
		if(new_src==NULL)
		{
			puts("Cannot allocate memory\n");
			ext2_file_release(file);
			free(buf);
			return -1;
		}
		new_dst=malloc(strlen(dst)+264);
		if(new_dst==NULL)
		{
			puts("Cannot allocate memory\n");
			ext2_file_release(file);
			free(new_src);
			free(buf);
			return -1;
		}
		off=0;
		while(off=ext2_readdir(file,off,dirent))
		{
			if(dirent->inode)
			{
				memcpy(buf2,dirent->file_name,(unsigned int)dirent->name_len);
				buf2[(unsigned int)dirent->name_len]=0;
				if(strcmp(buf2,".")&&strcmp(buf2,".."))
				{
					strcpy(new_src,src);
					strcat(new_src,"/");
					strcat(new_src,buf2);
					strcpy(new_dst,dst);
					strcat(new_dst,"\\");
					file_name_append(new_dst,buf2);
					if(pull_file(new_src,new_dst))
					{
						ext2_file_release(file);
						free(new_src);
						free(new_dst);
						free(buf);
						return -1;
					}
				}
			}
		}
		ext2_file_release(file);
		free(new_src);
		free(new_dst);
		free(buf);
		return 0;
	}
	puts(src);
	puts(": Special file ignored\n");
	return 0;
}
void cmd_pull(int argc,char **argv)
{
	if(argc<3)
	{
		puts("Too few arguments\n");
		return;
	}
	pull_file(argv[1],argv[2]);
}
void cmd_mkdir(int argc,char **argv)
{
	if(argc<2)
	{
		puts("Too few arguments\n");
		return;
	}
	if(ext2_mkdir_path(current_dir,argv[1]))
	{
		puts("Failed to create directory\n");
	}
	ext2_sync();
}
int push_file(char *src,char *dst)
{
	void *fp;
	char *buf;
	struct ext2_file *file,*file1;
	fp=fopen(src,"rb");
	if(fp==NULL)
	{
		char *new_src,*new_dst;
		void *dh;
		struct win32_find_data_a fdata;
		buf=malloc(strlen(src)+20);
		if(buf==NULL)
		{
			puts("Cannot allocate memory\n");
			return -1;
		}
		strcpy(buf,src);
		strcat(buf,"/*");
		if((dh=FindFirstFileA(buf,&fdata))!=INVALID_HANDLE_VALUE)
		{
			free(buf);
			if(ext2_mkdir_path(current_dir,dst))
			{
				puts(dst);
				puts(": Cannot create directory\n");
			}
			new_src=malloc(strlen(src)+264);
			if(new_src==NULL)
			{
				puts("Cannot allocate memory\n");
				return -1;
			}
			new_dst=malloc(strlen(dst)+264);
			if(new_dst==NULL)
			{
				free(new_src);
				puts("Cannot allocate memory\n");
				return -1;
			}
			
			
			do
			{
				if(strcmp(fdata.name,".")&&strcmp(fdata.name,".."))
				{
					strcpy(new_src,src);
					strcat(new_src,"/");
					strcat(new_src,fdata.name);
					strcpy(new_dst,dst);
					strcat(new_dst,"/");
					strcat(new_dst,fdata.name);
					if(push_file(new_src,new_dst))
					{
						FindClose(dh);
						free(new_src);
						free(new_dst);
						return -1;
					}
				}
			}
			while(FindNextFileA(dh,&fdata));
			FindClose(dh);
			free(new_src);
			free(new_dst);
			return 0;
		}
		free(buf);
		puts(src);
		puts(": Cannot open file for reading\n");
		return 0;
	}
	buf=malloc(65536);
	if(buf==NULL)
	{
		fclose(fp);
		puts("Cannot allocate memory\n");
		return -1;
	}
	unsigned int diri,inode;
	if(ext2_detect_file(current_dir,dst))
	{
		puts(dst);
		puts(": Cannot create file\n");
		fclose(fp);
		free(buf);
		return 0;
	}
	diri=ext2_dir_inode(current_dir,dst);
	if(diri==0)
	{
		puts(dst);
		puts(": Cannot create file\n");
		fclose(fp);
		free(buf);
		return 0;
	}
	file=ext2_file_load(diri,1,0);
	if(file==NULL)
	{
		puts(dst);
		puts(": Cannot create file\n");
		fclose(fp);
		free(buf);
		return 0;
	}
	inode=ext2_mknod(diri,0100644,1);
	if(inode==0)
	{
		puts(dst);
		puts(": Cannot create file\n");
		ext2_file_release(file);
		fclose(fp);
		free(buf);
		return 0;
	}
	if(ext2_link(file,inode,dst,1))
	{
		puts(dst);
		puts(": Cannot create file\n");
		ext2_file_release(file);
		ext2_inode_release(inode);
		fclose(fp);
		free(buf);
		return 0;
	}
	file1=ext2_file_load(inode,1,0);
	if(file1==NULL)
	{
		puts(dst);
		puts(": Cannot create file\n");
		ext2_unlink(file,dst);
		ext2_file_release(file);
		ext2_inode_release(inode);
		fclose(fp);
		free(buf);
		return 0;
	}
	long off,n;
	off=0;
	while(n=fread(buf,1,65536,fp))
	{
		if(ext2_file_write(file1,off,buf,n)!=n)
		{
			puts("No space available\n");
			ext2_file_release(file);
			ext2_file_release(file1);
			fclose(fp);
			free(buf);
			return -1;
		}
		off+=n;
	}
	ext2_file_release(file);
	ext2_file_release(file1);
	fclose(fp);
	free(buf);
	return 0;
}
void cmd_push(int argc,char **argv)
{
	if(argc<3)
	{
		puts("Too few arguments\n");
		return;
	}
	push_file(argv[1],argv[2]);
	ext2_sync();
}
struct file_inode_tab
{
	unsigned int inode;
	unsigned int pad;
	struct file_inode_tab *next;
};
long file_space_usage(unsigned int inode,struct file_inode_tab **tab,unsigned int *inodes)
{
	struct file_inode_tab *node;
	struct ext2_file *file;
	long size;
	node=tab[inode%1021];
	while(node)
	{
		if(node->inode==inode)
		{
			return 0;
		}
		node=node->next;
	}
	node=malloc(sizeof(*node));
	if(node==NULL)
	{
		puts("Cannot allocate memory\n");
		return -1;
	}
	node->inode=inode;
	node->next=tab[inode%1021];
	tab[inode%1021]=node;
	file=ext2_file_load(inode,0,0);
	if(file==NULL)
	{
		puts("Cannot open file\n");
		return -1;
	}
	size=file->inode.blocks;
	if((file->inode.mode&0170000)==040000)
	{
		struct ext2_directory *dirent;
		long off,size1;
		dirent=malloc(4096);
		if(dirent==NULL)
		{
			ext2_file_release(file);
			puts("Cannot allocate memory\n");
			return -1;
		}
		off=0;
		while(off=ext2_readdir(file,off,dirent))
		{
			if(dirent->inode&&(dirent->name_len!=2||dirent->file_name[0]!='.'||dirent->file_name[1]!='.'))
			{
				size1=file_space_usage(dirent->inode,tab,inodes);
				if(size1==-1)
				{
					ext2_file_release(file);
					return -1;
				}
				size+=size1;
			}
		}
		free(dirent);
	}
	ext2_file_release(file);
	++*inodes;
	return size;
}
void cmd_du(int argc,char **argv)
{
	if(argc<2)
	{
		puts("Too few arguments\n");
		return;
	}
	struct file_inode_tab *tab[1021],*node;
	unsigned int inodes,inode,x;
	long size;
	char buf[32];
	memset(tab,0,sizeof(tab));
	if(ext2_search_path(current_dir,argv[1],&inode))
	{
		puts("Cannot open file\n");
		return;
	}
	inodes=0;
	size=file_space_usage(inode,tab,&inodes);
	x=0;
	while(x<1021)
	{
		while(node=tab[x])
		{
			tab[x]=node->next;
			free(node);
		}
		++x;
	}
	if(size==-1)
	{
		return;
	}
	puts("512-Byte Blocks Used: ");
	buf[0]=0;
	sprinti(buf,size,1);
	puts(buf);
	puts("\nInodes Used: ");
	buf[0]=0;
	sprinti(buf,inodes,1);
	puts(buf);
	puts("\n");
}
int remove_file(unsigned int inode)
{
	struct ext2_file *file;
	int result,ret;
	if(inode==2)
	{
		puts("Cannot remove root directory\n");
		return -1;
	}
	if(inode==current_dir)
	{
		puts("Cannot remove current directory\n");
		return -1;
	}
	file=ext2_file_load(inode,1,0);
	if(file==NULL)
	{
		puts("Cannot open file\n");
		return -1;
	}
	result=0;
	ret=0;
	if((file->inode.mode&0170000)==040000)
	{
		struct ext2_directory *dirent;
		long off;
		char buf[256];
		unsigned int x;
		int val;
		if((dirent=malloc(4096))==NULL)
		{
			ext2_file_release(file);
			puts("Cannot allocate memory\n");
			return -1;
		}
		off=0;
		while(off=ext2_readdir(file,off,dirent))
		{
			if(dirent->inode)
			{
				if(!(dirent->name_len==1&&dirent->file_name[0]=='.')&&
				!(dirent->name_len==2&&dirent->file_name[0]=='.'&&dirent->file_name[1]=='.'))
				{
					if((val=remove_file(dirent->inode))<0)
					{
						result=1;
					}
					else
					{
						if(val==1)
						{
							--file->inode.links;
							--ext2_bgdt_array[(dirent->inode-1)/ext2_sb->inodes_per_group].used_dirs;
						}
						x=0;
						while(x<(unsigned int)dirent->name_len)
						{
							buf[x]=dirent->file_name[x];
							++x;
						}
						buf[x]=0;
						ext2_unlink(file,buf);
					}
				}
			}
		}
		free(dirent);
		ret=1;
	}
	else if((file->inode.mode&0170000)!=0100000)
	{
		ext2_file_release(file);
		puts("Cannot remove special file\n");
		return -1;
	}
	else if(file->inode.links!=1)
	{
		ext2_file_release(file);
		puts("Cannot remove hard link\n");
		return -1;
	}
	if(result)
	{
		ext2_file_release(file);
		return -1;
	}
	ext2_release_file_blocks(file);
	ext2_file_release(file);
	ext2_inode_release(inode);
	return ret;
}
void cmd_remove(int argc,char **argv)
{
	struct ext2_file *dir;
	int val;
	if(argc<2)
	{
		puts("Too few arguments\n");
		return;
	}
	unsigned int inode,parent_inode;
	if(ext2_is_special_entry(argv[1]))
	{
		puts("Cannot remove file\n");
		return;
	}
	if(ext2_search_path(current_dir,argv[1],&inode))
	{
		puts("Cannot open file\n");
		return;
	}
	parent_inode=ext2_dir_inode(current_dir,argv[1]);
	if(parent_inode==0)
	{
		puts("Cannot open file\n");
		return;
	}
	dir=ext2_file_load(parent_inode,1,0);
	if(dir==NULL)
	{
		puts("Cannot open file\n");
		return;
	}
	if((val=remove_file(inode))>=0)
	{
		if(val==1)
		{
			--dir->inode.links;
			--ext2_bgdt_array[(inode-1)/ext2_sb->inodes_per_group].used_dirs;
		}
		ext2_unlink(dir,argv[1]);
	}
	ext2_file_release(dir);
	ext2_sync();
}
void cmd_move(int argc,char **argv)
{
	if(argc<3)
	{
		puts("Too few arguments\n");
		return;
	}
	struct ext2_file *dir,*file,*newdir;
	int type;
	char buf[4096];
	struct ext2_directory *dirent;
	unsigned int inode,parent_inode,new_parent_inode;
	if(ext2_is_special_entry(argv[1])||ext2_is_special_entry(argv[2]))
	{
		puts("Cannot move file\n");
		return;
	}
	if(ext2_detect_file(current_dir,argv[2]))
	{
		puts("Cannot move file\n");
		return;
	}
	dirent=(void *)buf;
	if(ext2_search_path(current_dir,argv[1],&inode))
	{
		puts("Cannot open file\n");
		return;
	}
	if(inode==2)
	{
		puts("Cannot move root directory\n");
		return;
	}
	parent_inode=ext2_dir_inode(current_dir,argv[1]);
	if(parent_inode==0)
	{
		puts("Cannot open file\n");
		return;
	}
	new_parent_inode=ext2_dir_inode(current_dir,argv[2]);
	if(new_parent_inode==0)
	{
		puts("Cannot move file\n");
		return;
	}
	dir=ext2_file_load(parent_inode,1,0);
	if(dir==NULL)
	{
		puts("Cannot open file\n");
		return;
	}
	file=ext2_file_load(inode,1,0);
	if(file==NULL)
	{
		puts("Cannot open file\n");
		ext2_file_release(dir);
		return;
	}
	if((file->inode.mode&0170000)==0100000)
	{
		type=1;
	}
	else if((file->inode.mode&0170000)==040000)
	{
		type=2;
	}
	else
	{
		puts("Special file ignored\n");
		ext2_file_release(file);
		ext2_file_release(dir);
		return;
	}
	if(new_parent_inode!=parent_inode)
	{
		newdir=ext2_file_load(new_parent_inode,1,0);
		if(newdir==NULL)
		{
			puts("Cannot move file\n");
			ext2_file_release(newdir);
			ext2_file_release(dir);
			return;
		}
		if(newdir->inode.links==65535)
		{
			puts("Cannot move file\n");
			ext2_file_release(file);
			ext2_file_release(newdir);
			ext2_file_release(dir);
			return;
		}
	}
	else
	{
		newdir=dir;
	}
	if(ext2_link(newdir,inode,argv[2],type))
	{
		puts("Cannot move file\n");
		ext2_file_release(file);
		if(new_parent_inode!=parent_inode)
		{
			ext2_file_release(newdir);
		}
		ext2_file_release(dir);
		return;
	}
	ext2_unlink(dir,argv[1]);
	if(type==2)
	{
		
		--dir->inode.links;
		++newdir->inode.links;
		long off;
		off=0;
		while(off=ext2_readdir(file,off,dirent))
		{
			if(dirent->inode==parent_inode)
			{
				dirent->inode=new_parent_inode;
				ext2_file_write(file,off-dirent->rec_len,dirent,dirent->rec_len);
				break;
			}
		}
	}
	
	ext2_file_release(file);
	if(new_parent_inode!=parent_inode)
	{
		ext2_file_release(newdir);
	}
	ext2_file_release(dir);
	ext2_sync();
}
int cmd_run(void)
{
	static char input[INPUT_SIZE];
	char *argv[ARGC_MAX];
	int argc,x,x1;
	puts("EXT2FS> ");
	get_s(input,INPUT_SIZE);
	argc=0;
	memset(argv,0,sizeof(argv));
	x=0;
	while(input[x]==' ')
	{
		++x;
	}
	if(input[x]==0)
	{
		return 1;
	}
	while(argc<ARGC_MAX&&input[x])
	{
		x1=x;
		while(input[x]&&input[x]!=' ')
		{
			if(input[x]=='\\')
			{
				memmove(input+x,input+x+1,INPUT_SIZE-x-1);
			}
			++x;
		}
		while(input[x]==' ')
		{
			input[x]=0;
			++x;
		}
		argv[argc]=input+x1;
		++argc;
	}
	if(!strcmp(argv[0],"exit"))
	{
		return 0;
	}
	if(!strcmp(argv[0],"help"))
	{
		cmd_help(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"ls"))
	{
		cmd_ls(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"cd"))
	{
		cmd_cd(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"pwd"))
	{
		cmd_pwd(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"pull"))
	{
		cmd_pull(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"mkdir"))
	{
		cmd_mkdir(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"push"))
	{
		cmd_push(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"du"))
	{
		cmd_du(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"remove"))
	{
		cmd_remove(argc,argv);
		return 1;
	}
	if(!strcmp(argv[0],"move"))
	{
		cmd_move(argc,argv);
		return 1;
	}
	puts("Unknown command \"");
	puts(argv[0]);
	puts("\", type \"help\" for available commands.\n");
	return 1;
}
