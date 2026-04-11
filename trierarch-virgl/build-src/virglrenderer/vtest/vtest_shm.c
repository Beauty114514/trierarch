/**************************************************************************
 *
 * Copyright (C) 2018 Chromium.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "util.h"
#include "vtest_shm.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/syscall.h>
#include <unistd.h>

static int memfd_create(const char *name, unsigned int flags)
{
#ifdef __NR_memfd_create
    return syscall(__NR_memfd_create, name, flags);
#else
    (void)name;
    (void)flags;
    return -1;
#endif
}

#ifdef __ANDROID__
#include <sys/ioctl.h>
static int vtest_new_android_ashm(size_t size)
{
   int fd, ret;
   long flags;
   fd = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
   if (fd < 0)
      return fd;
   ret = ioctl(fd, /* ASHMEM_SET_SIZE */ _IOW(0x77, 3, size_t), size);
   if (ret < 0)
      goto err;
   flags = fcntl(fd, F_GETFD);
   if (flags == -1)
      goto err;
   if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
      goto err;
   return fd;
err:
   close(fd);
   return -1;
}
#endif

int vtest_new_shm(uint32_t handle, size_t size)
{
   int fd, ret;
   int length = snprintf(NULL, 0, "vtest-res-%u", handle);
   char *str = malloc(length + 1);
   snprintf(str, length + 1, "vtest-res-%u", handle);

   fd = memfd_create(str, MFD_ALLOW_SEALING);
   free(str);
   if (fd < 0) {
#ifdef __ANDROID__
      fd = vtest_new_android_ashm(size);
      if (fd >= 0)
         return fd;
#endif
      return report_failed_call("memfd_create", -errno);
   }

   ret = ftruncate(fd, size);
   if (ret < 0) {
      close(fd);
      return report_failed_call("ftruncate", -errno);
   }

   return fd;
}

int vtest_shm_check(void)
{
#ifdef __ANDROID__
   return 1;
#endif

    int mfd = memfd_create("test", MFD_ALLOW_SEALING);

    if (mfd >= 0) {
        close(mfd);
        return 1;
    }

    return 0;
}

