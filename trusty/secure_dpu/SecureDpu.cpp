/*
 * Copyright 2024 NXP.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

#define LOG_TAG "secure_dpu"
#include <trusty/secure_dpu/SecureDpu.h>
#include <log/log.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <trusty/tipc.h>
#include <android-base/unique_fd.h>
#include <BufferAllocator/BufferAllocator.h>
#include <sys/uio.h>

#define TRUSTY_DEVICE_NAME "/dev/trusty-ipc-dev0"
/* req buffer size is equal to the size of allocate buffer request */
#define REQ_BUFFER_SIZE 8
static uint8_t req_buffer[REQ_BUFFER_SIZE];
static int handle_ = -1;

using android::base::unique_fd;

static int secure_dpu_connect() {
    int rc = tipc_connect(TRUSTY_DEVICE_NAME, SECURE_DPU_PORT_NAME);
    if (rc < 0) {
        ALOGE("TIPC Connect failed (%d)!", rc);
        return rc;
    }

    handle_ = rc;
    return 0;
}

static void secure_dpu_disconnect() {
    if (handle_ >= 0) {
        tipc_close(handle_);
    }
    handle_ = -1;
}

static int secure_dpu_ipc_response(struct iovec* tx, size_t tx_num,
                                   struct trusty_shm *shm, size_t shm_num) {
    int rc;
    rc = tipc_send(handle_, tx, tx_num, shm, shm_num);
    if (rc < 0) {
        ALOGE("send response failed rc = %d", rc);
        return -1;
    }
    return 0;
}

static int secure_dpu_ipc_get_msg(struct secure_dpu_req *msg, void *req_buf, size_t req_buf_len) {
    ssize_t rc;
    struct iovec iovs[2] = {
        {msg, sizeof(*msg)},
        {req_buf, req_buf_len}
    };

    rc = readv(handle_, iovs, 2);
    if (rc < 0) {
        ALOGE("failed to read request: %s\n", strerror(errno));
        return rc;
    }

    /* check for minimum size */
    if ((size_t)rc < sizeof(*msg)) {
        ALOGE("message is too short (%zu bytes received)\n", rc);
        return -1;
    }

    /* check the total size */
    if ((size_t)rc > sizeof(*msg)) {
        if ((size_t)rc != (sizeof(*msg) + req_buf_len)) {
            ALOGE("get message is not correct");
            return -1;
        }
    }

    return rc - sizeof(*msg);
}

static int start_secure_dpu() {
    /* nothing to do */
    secure_dpu_resp hdr = {
        .cmd = SECURE_DPU_CMD_START_SECURE_DISPLAY | SECURE_DPU_CMD_RESP_BIT,
        .status = SECURE_DPU_ERROR_OK,
    };
    struct iovec iovs = {
        &hdr, sizeof(hdr)
    };

    return secure_dpu_ipc_response(&iovs, 1, NULL, 0);
}

static int stop_secure_dpu() {
    /* nothing to do */
    secure_dpu_resp hdr = {
        .cmd = SECURE_DPU_CMD_STOP_SECURE_DISPLAY | SECURE_DPU_CMD_RESP_BIT,
        .status = SECURE_DPU_ERROR_OK,
    };
    struct iovec iovs = {
         &hdr, sizeof(hdr)
    };

    return secure_dpu_ipc_response(&iovs, 1, NULL, 0);
}

static int secure_dpu_allocate_secure_framebuffer(const void* req, size_t req_len) {

    if (req_len != sizeof(struct secure_dpu_allocate_buffer_req))
        return SECURE_DPU_ERROR_PARAMETERS;

    secure_dpu_allocate_buffer_req *allocate_req = (struct secure_dpu_allocate_buffer_req*)req;
    long page_size = sysconf(_SC_PAGESIZE);
    off64_t buffer_size, buffer_page_offset, buffer_page_size;
    buffer_size = allocate_req->buffer_len;
    int32_t status = SECURE_DPU_ERROR_OK, shm_num = 0;

    /* The dmabuf size needs to be a multiple of the page size */
    buffer_page_offset = buffer_size & (page_size - 1);
    if (buffer_page_offset) {
        buffer_page_offset = page_size - buffer_page_offset;
    }
    if (__builtin_add_overflow(buffer_size, buffer_page_offset, &buffer_page_size)) {
        ALOGE("Failed to page-align buffer size");
        return SECURE_DPU_ERROR_PARAMETERS;
    }

    BufferAllocator alloc;
    unique_fd dmabuf_fd(alloc.Alloc("secure", buffer_page_size));
    if (!dmabuf_fd.ok()) {
        ALOGE("Error creating dmabuf for secure framebuffer %lu\n", allocate_req->buffer_len);
        status = SECURE_DPU_ERROR_NO_MEMORY;
    }

    secure_dpu_resp hdr = {
        .cmd = SECURE_DPU_CMD_ALLOCATE_BUFFER | SECURE_DPU_CMD_RESP_BIT,
        .status = status,
    };

    secure_dpu_allocate_buffer_resp resp = {
        .buffer_len = static_cast<uint64_t>(buffer_page_size),
    };

    struct iovec tx[2] = {
        {&hdr, sizeof(hdr)},
        {&resp, sizeof(resp)}
    };

    trusty_shm shm = {
        .fd = dmabuf_fd,
        .transfer = TRUSTY_LEND,
    };

    return secure_dpu_ipc_response(tx, 2, &shm, 1);

}

static int handle_req(struct secure_dpu_req* msg, const void* req, size_t req_len) {
    int rc;
    struct secure_dpu_resp resp;
    struct iovec tx;

    switch (msg->cmd) {
        case SECURE_DPU_CMD_START_SECURE_DISPLAY:
            rc = start_secure_dpu();
            break;
        case SECURE_DPU_CMD_STOP_SECURE_DISPLAY:
            rc = stop_secure_dpu();
            break;
        case SECURE_DPU_CMD_ALLOCATE_BUFFER:
            rc = secure_dpu_allocate_secure_framebuffer(req, req_len);
            break;
        default:
            ALOGE("unhandled command 0x%x\n", msg->cmd);
            rc = SECURE_DPU_ERROR_PARAMETERS;
            break;
    }

    if (rc) {
         /* response was sent in handler */
         resp.status = rc;
         resp.cmd = msg->cmd | SECURE_DPU_CMD_RESP_BIT;
         tx.iov_base = &resp;
         tx.iov_len = sizeof(resp);
         rc = secure_dpu_ipc_response(&tx, 1, NULL, 0);
    }

    return rc;
}

static int start_loop(void) {
    ssize_t rc;
    struct secure_dpu_req msg;

    /* enter main message handling loop */
    while (true) {
        /* get incoming message */
        rc = secure_dpu_ipc_get_msg(&msg, req_buffer, REQ_BUFFER_SIZE);
        if (rc < 0)
            return rc;
        /* handle request */
        rc = handle_req(&msg, req_buffer, rc);
        if (rc < 0)
            return rc;
    }

    return 0;
}

int main() {
    int rc;

    /* connect to Trusty secure dpu server */
    rc = secure_dpu_connect();
    if (rc < 0) {
        ALOGE("connect secure dpu server failed with status (%d)", rc);
        return EXIT_FAILURE;
    }

    /* enter main loop */
    rc = start_loop();
    ALOGE("exiting secure dpu loop with status (%d)\n", rc);

    secure_dpu_disconnect();
    return (rc < 0) ? EXIT_FAILURE: 0;
}

