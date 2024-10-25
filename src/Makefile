PKGDIR        ?= ..
L4DIR         ?= $(PKGDIR)/../..

TARGET                 = ipcbench ipcbench_parallel \
                         ipcbench_client ipcbench_server \
                         ipcbench_syscall
REQUIRES_LIBS          = libpthread
SRC_C_ipcbench         = ipcbench.c ipc_common.c
SRC_C_ipcbench_parallel = ipcbench_parallel.c ipc_common.c
SRC_C_ipcbench_client  = ipcclient.c
SRC_C_ipcbench_server  = ipcserver.c
SRC_C_ipcbench_syscall = ipcsyscall.c

include $(L4DIR)/mk/prog.mk
