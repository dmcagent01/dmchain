#ifdef linux

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/syscall.h>
#include <linux/random.h>

extern "C" {

void __explicit_bzero_chk(void* dst, size_t len, size_t dstlen)
{
    memset(dst, '\0', len);
    /* Compiler barrier.  */
    asm volatile("" ::
                     : "memory");
}

static int getrandom_wrapper(void* buf, size_t buflen, unsigned int flags)
{
    return (syscall(SYS_getrandom, buf, buflen, flags));
}

int getentropy(void* buffer, size_t len)
{
    int cs, ret = 0;
    char* pos = (char*)buffer;

    if (len > 256) {
        errno = EIO;
        return -1;
    }

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cs);

    while (len) {
        ret = getrandom_wrapper(pos, len, 0);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else
                break;
        }
        pos += ret;
        len -= ret;
        ret = 0;
    }

    pthread_setcancelstate(cs, 0);

    return ret;
}

int __cxa_thread_atexit_impl(void* func, void* obj, void* dso_symbol)
{
    return 0;
}

char* secure_getenv(const char* name)
{
    return getenv(name);
}

void __chk_fail(void);
unsigned long int __fdelt_chk(unsigned long int d)
{
    if (d >= FD_SETSIZE)
        __chk_fail();

    return d / __NFDBITS;
}

int pthread_setname_np(pthread_t thread, const char* name)
{
    return 0;
}

int pthread_getname_np(pthread_t thread, char* name, size_t len)
{
    static int thread_count = 0;

    sprintf(name, "thread-%d", thread_count++);
    return 0;
}

}
#endif
