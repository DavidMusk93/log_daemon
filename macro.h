#pragma once

#define _main() int main()
#define mainEx(c, v) int main(int c,char *v[])

#define _log(fp, fmt, ...) fprintf(fp,fmt "\n",##__VA_ARGS__)
#define log1(...) _log(stdout,__VA_ARGS__)
#define log2(...) _log(stderr,__VA_ARGS__)

#define alwaysFlushOutput() \
setbuf(stdout,0);\
setbuf(stderr,0)

#define _attr(...) __attribute__((__VA_ARGS__))
#define attrScopeGuard(fn) _attr(__cleanup__(fn))
#define attrCtor _attr(constructor)
#define attrDtor _attr(destructor)

#define __ctor() \
static attrCtor void initGlobalVariables()
#define __dtor() \
static attrDtor void freeGlobalVariables()

#define autoFd(name) int name attrScopeGuard(closeFd)=-1

#define _poll(rc, fn, ...) \
rc = fn(__VA_ARGS__);\
if (rc<0){\
    if (errno==EINTR || errno==EAGAIN) continue;\
    break;\
}

#define dimensionOf(a) (int)(sizeof(a)/sizeof(*a))
#define offsetOf(type, field) (size_t)(&((type*)0)->field)
#define sockAddrEx(x) (struct sockaddr*)&x,sizeof x

#define unixAddr(sa, path) \
struct sockaddr_un sa={};\
sa.sun_family=AF_UNIX;\
snprintf(sa.sun_path,sizeof(sa)-offsetOf(struct sockaddr_un,sun_path),"%s",(path))

#define sockNew(fd, domain, type) \
fd=socket(domain,type,0);\
if(fd==-1) break

#define sockOp(rc, op, fd, ...) \
rc=op(fd,__VA_ARGS__);\
if(rc==-1) break

#define _unixServer(rc, fd, type, path) \
do{\
    remove(path);\
    unixAddr(_sa,path);\
    sockNew(fd,AF_UNIX,type);\
    sockOp(rc,bind,fd,sockAddrEx(_sa));\
    if(type==SOCK_STREAM){\
        sockOp(rc,listen,fd,10);\
    }\
}while(0)

#define unixDgramServer(rc, fd, path) _unixServer(rc,fd,SOCK_DGRAM,path)
#define unixStreamServer(rc, fd, path) _unixServer(rc,fd,SOCK_STREAM,path)

#define _unixClient(rc, fd, type, path) \
do{\
    unixAddr(_sa,path);\
    sockNew(fd,AF_UNIX,type);\
    sockOp(rc,connect,fd,sockAddrEx(_sa));\
}while(0)

#define unixDgramClient(rc, fd, path) _unixClient(rc,fd,SOCK_DGRAM,path)
#define unixStreamClient(rc, fd, path) _unixClient(rc,fd,SOCK_STREAM,path)

#define __concat(x, y) x##y
#define _concat(x, y) __concat(x,y)

#define _arg17(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16) _16
#define argCount(...) _arg17(__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)
#define _opList1(op, x)    op(x)
#define _opList2(op, ...)  op(x) _opList1(op,__VA_ARGS__)
#define _opList3(op, ...)  op(x) _opList2(op,__VA_ARGS__)
#define _opList4(op, ...)  op(x) _opList3(op,__VA_ARGS__)
#define _opList5(op, ...)  op(x) _opList4(op,__VA_ARGS__)
#define _opList6(op, ...)  op(x) _opList5(op,__VA_ARGS__)
#define _opList7(op, ...)  op(x) _opList6(op,__VA_ARGS__)
#define _opList8(op, ...)  op(x) _opList7(op,__VA_ARGS__)
#define _opList9(op, ...)  op(x) _opList8(op,__VA_ARGS__)
#define _opList10(op, ...) op(x) _opList9(op,__VA_ARGS__)
#define _opList11(op, ...) op(x) _opList10(op,__VA_ARGS__)
#define _opList12(op, ...) op(x) _opList11(op,__VA_ARGS__)
#define _opList13(op, ...) op(x) _opList12(op,__VA_ARGS__)
#define _opList14(op, ...) op(x) _opList13(op,__VA_ARGS__)
#define _opList15(op, ...) op(x) _opList14(op,__VA_ARGS__)
#define _opList16(op, ...) op(x) _opList15(op,__VA_ARGS__)
#define opList(op, ...) _concat(_opList,argCount(__VA_ARGS__))(op,__VA_ARGS__)
