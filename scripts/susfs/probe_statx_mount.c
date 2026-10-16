/* Freestanding ARM64 read-only statx/fdinfo probe; no libc or root required.
 * probe_statx_uapi.h is generated from this kernel's actual statx declarations.
 */
typedef unsigned int __u32;
typedef int __s32;
typedef unsigned short __u16;
typedef unsigned long long __u64;
typedef long long __s64;
#include "probe_statx_uapi.h"
_Static_assert(sizeof(struct statx)==256, "statx size");
_Static_assert(__builtin_offsetof(struct statx,stx_mnt_id)==144, "mount ID offset");

static long sc(long nr,long a,long b,long c,long d,long e,long f)
{
    register long x8 __asm__("x8")=nr;
    register long x0 __asm__("x0")=a;
    register long x1 __asm__("x1")=b;
    register long x2 __asm__("x2")=c;
    register long x3 __asm__("x3")=d;
    register long x4 __asm__("x4")=e;
    register long x5 __asm__("x5")=f;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8),"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5) : "memory","cc");
    return x0;
}
static unsigned length(const char *s) { unsigned n=0; while(s[n]) n++; return n; }
static void emit(const char *s) { sc(64,1,(long)s,length(s),0,0,0); }
static unsigned decimal(char *s,__u64 n)
{
    char reverse[24]; unsigned count=0;
    do { reverse[count++]='0'+n%10; n/=10; } while(n);
    for(unsigned i=0;i<count;i++) s[i]=reverse[count-i-1];
    s[count]=0; return count;
}
static void number(__u64 n) { char s[24]; decimal(s,n); emit(s); }
static int fd_mount_id(long fd,__u64 *id)
{
    char path[64],text[1024]; const char *prefix="/proc/self/fdinfo/";
    unsigned n=length(prefix);
    for(unsigned i=0;i<n;i++) path[i]=prefix[i];
    decimal(path+n,fd);
    long info=sc(56,-100,(long)path,0x80000,0,0,0);
    if(info<0) return 0;
    long bytes=sc(63,info,(long)text,sizeof(text)-1,0,0,0);
    sc(57,info,0,0,0,0,0);
    if(bytes<=0) return 0;
    text[bytes]=0;
    const char *key="mnt_id:";
    for(long i=0;i+7<bytes;i++) {
        unsigned j=0; while(key[j] && text[i+j]==key[j]) j++;
        if(key[j]) continue;
        i+=j; while(text[i]==' ' || text[i]=='\t') i++;
        if(text[i]<'0' || text[i]>'9') return 0;
        *id=0;
        while(text[i]>='0' && text[i]<='9') *id=*id*10+(text[i++]-'0');
        return 1;
    }
    return 0;
}
static int check(const char *path)
{
    struct statx named,descriptor;
    long a=sc(291,-100,(long)path,0,0x17ff,(long)&named,0);
    long fd=sc(56,-100,(long)path,0x280000,0,0,0); /* O_PATH | O_CLOEXEC */
    long b=fd<0 ? fd : sc(291,fd,(long)"",0x1000,0x17ff,(long)&descriptor,0);
    __u64 info_id=0;
    int have_info=fd>=0 && fd_mount_id(fd,&info_id);
    if(fd>=0) sc(57,fd,0,0,0,0,0);
    int supported=a==0 && b==0 && (named.stx_mask&0x1000) && (descriptor.stx_mask&0x1000);
    int same=supported && have_info && named.stx_mnt_id==descriptor.stx_mnt_id &&
             named.stx_mnt_id==info_id && named.stx_ino==descriptor.stx_ino;
    emit("{\"path\":\""); emit(path); emit("\",\"path_errno\":"); number(a<0?-a:0);
    emit(",\"fd_errno\":"); number(b<0?-b:0); emit(",\"mount_id_supported\":"); emit(supported?"true":"false");
    emit(",\"path_id\":"); number(a==0?named.stx_mnt_id:0);
    emit(",\"fd_id\":"); number(b==0?descriptor.stx_mnt_id:0);
    emit(",\"fdinfo_id\":"); number(info_id); emit(",\"consistent\":"); emit(same?"true":"false"); emit("}\n");
    return same;
}
__attribute__((noreturn)) void _start(void)
{
    const char *paths[]={"/","/system","/vendor","/data/local/tmp","/proc"};
    int ok=1;
    for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++) if(!check(paths[i])) ok=0;
    sc(93,ok?0:1,0,0,0,0,0);
    __builtin_unreachable();
}
