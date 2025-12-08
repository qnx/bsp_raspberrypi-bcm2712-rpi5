/*
 * Notice on Software Maturity and Quality
 *
 * The software included in this repository is classified under our Software Maturity Standard as Experimental Software - Software Quality and Maturity Level (SQML) 1.
 *
 * As defined in the QNX Development License Agreement, Experimental Software represents early-stage deliverables intended for evaluation or proof-of-concept purposes.
 *
 * SQML 1 indicates that this software is provided without one or more of the following:
 *     - Formal requirements
 *     - Formal design or architecture
 *     - Formal testing
 *     - Formal support
 *     - Formal documentation
 *     - Certifications of any type
 *     - End-of-Life or End-of-Support policy
 *
 * Additionally, this software is not monitored or scanned under our Cybersecurity Management Standard.
 *
 * No warranties, guarantees, or claims are offered at this SQML level.
 */

/*
 * Copyright (c) 2024-2025, BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/iofunc.h>
#include <sys/resmgr.h>
#include <sys/dispatch.h>
#include <atomic.h>
#include "proto.h"

struct resmgr_;

typedef struct resmgr_ {
    iofunc_attr_t           iofunc_attr;
    dispatch_t             *dpp;
    dispatch_context_t     *ctp;
    resmgr_connect_funcs_t  connect_funcs;
    resmgr_io_funcs_t       io_funcs;
    int                     id;
    char                   *filename;
    pthread_mutex_t         mlock;
    pwm_fan_dev_t          *dev;
    volatile unsigned       done;
} resmgr_t;

static resmgr_t resmgr = {0};

static void sig_handler(const int signo)
{
    (void) signo;
    atomic_set(&resmgr.done, 1);
    dispatch_unblock(resmgr.ctp);
}

static void sig_init(void)
{
    struct sigaction sa = { 0 };

    /* Register exit handler */
    (void)sigemptyset(&sa.sa_mask);
    (void)sigaddset(&sa.sa_mask, SIGTERM);
    sa.sa_handler = sig_handler;
    sa.sa_flags = SA_SIGINFO;
    (void)sigaction(SIGTERM, &sa, NULL);
}

static int io_read (resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb)
{
    size_t      nleft;
    size_t      nbytes;
    int         nparts;
    int         status;
    char        buf[PWM_CTRL_MAX_STRLEN] = { 0 };

    status = iofunc_read_verify (ctp, msg, ocb, NULL);
    if (status != EOK) {
        return (status);
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) {
        return (ENOSYS);
    }

    // Nothing to read.
    if (msg->i.nbytes == 0U) {
        return (EOK);
    }


    ocb->attr->nbytes = snprintf(buf, sizeof(buf), "%u\n", pwm_fan_get(resmgr.dev));
    if (ocb->attr->nbytes < 0) {
        return (EINVAL);
    }

    /*
     *  On all reads (first and subsequent), calculate
     *  how many bytes we can return to the client,
     *  based upon the number of bytes available (nleft)
     *  and the client's buffer size
     */
    nleft = ocb->attr->nbytes - ocb->offset;
    nbytes = min(_IO_READ_GET_NBYTES(msg), nleft);
    if (nbytes > 0) {
        /* set up the return data IOV */
        SETIOV (ctp->iov, buf + ocb->offset, nbytes);

        /* set up the number of bytes (returned by client's read()) */
        _IO_SET_READ_NBYTES (ctp, nbytes);

        /* advance the offset by the number of bytes returned to the client. */
        ocb->offset += (off_t) nbytes;
        nparts = 1;
    } else {
        /* they've asked for zero bytes or they've already previously read everything */
        _IO_SET_READ_NBYTES (ctp, 0);
        ocb->offset = 0;
        nparts = 0;
    }

    /* mark the access time as invalid (we just accessed it) */
    ocb->attr->flags |= IOFUNC_ATTR_ATIME;

    return (_RESMGR_NPARTS (nparts));
}

static int io_write (resmgr_context_t *ctp, io_write_t *msg, RESMGR_OCB_T *ocb)
{
    int         status;
    size_t      nbytes;
    uint32_t    pwm_ctrl;
    char        buf[PWM_CTRL_MAX_STRLEN] = { 0 };
    uint64_t    val = 0ULL;

    status = iofunc_write_verify(ctp, msg, ocb, NULL);
    if (status != EOK) {
        return (status);
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) {
        return (ENOSYS);
    }

    /* Extract the length of the client's message. */
    nbytes = _IO_WRITE_GET_NBYTES(msg);

    /* Filter out malicious write requests that attempt to write more
       data than they provide in the message. */

    if (nbytes > ((size_t)ctp->info.srcmsglen - (size_t)ctp->offset - sizeof(io_write_t))) {
        return EBADMSG;
    }
    if (nbytes == 0U) {
        _IO_SET_WRITE_NBYTES(ctp, 0);
        return _RESMGR_NPARTS(0);
    }

    if (nbytes >= PWM_CTRL_MAX_STRLEN) {
        return EINVAL;
    }

    const ssize_t nbytes_read = resmgr_msgget(ctp, buf, nbytes, sizeof(msg->i));
    if (nbytes_read < 0) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s: resmgr_msgget failed: err=%d", __func__, errno);
        return errno;
    }
    buf[nbytes] = '\0'; /* just in case the text is not NULL terminated */

    errno = EOK;
    val = strtoul(buf, NULL, 10);
    if (errno == EOK) {
        if (val > PWM_CTRL_MAX_VAL) {
            (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Invalid value, valid range: range: 0 ~ %u", __func__, PWM_CTRL_MAX_VAL);
            return (EINVAL);
        }
        pwm_ctrl = (uint32_t)val;
    }
    else {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Invalid number string, err=%d", __func__, errno);
        return (EINVAL);
    }

    if (verbose > 0) {
        (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "%s Write: nbytes: %ld pwm_ctrl=%d", __func__, nbytes, pwm_ctrl);
    }

    status = pwm_fan_set(resmgr.dev, pwm_ctrl);
    if (status != EOK) {
        return status;
    }

    ocb->attr->flags |= IOFUNC_ATTR_ATIME | IOFUNC_ATTR_CTIME;

    // Indicate how many bytes were written
    _IO_SET_WRITE_NBYTES(ctp, nbytes);
    return _RESMGR_NPARTS(0);
}

int resmgr_loop_start()
{
    /* allocate a context structure */
    resmgr.ctp = dispatch_context_alloc(resmgr.dpp);

    /*  Start the resource manager loop */
    while (!resmgr.done) {
        if (resmgr.ctp == dispatch_block(resmgr.ctp)) {
            (void)dispatch_handler(resmgr.ctp);
        }
        else if (errno != EFAULT) {
            atomic_set(&resmgr.done, 1);
        }
    }
    return EOK;
}

void resmgr_deinit()
{
    if (resmgr_detach( resmgr.dpp, resmgr.id, 0) == -1) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Failed to remove pathname from the pathname space", __func__);
    }

    if (resmgr.filename != NULL) {
        free(resmgr.filename);
    }
}

int resmgr_init(pwm_fan_dev_t *dev)
{
    resmgr.dev = dev;

    /* Single_instance. Ensure that the driver is not already running */
    if (name_attach(NULL, RESMGR_NAME, 0) == NULL) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Is '%s' already started?", __func__, RESMGR_NAME);
        return errno;
    }
    resmgr.filename = strdup(FAN_DEV_NAME);

    /* allocate and initialize a dispatch structure for use by our main loop */
    resmgr.dpp = dispatch_create();
    if (resmgr.dpp == NULL) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Couldn't dispatch_create, errno: %d", __func__, errno);
        return errno;
    }

    /*
     * Intialize the connect functions and I/O functions tables to their defaults and then
     * override the defaults with the functions that we are providing.
     */
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &resmgr.connect_funcs, _RESMGR_IO_NFUNCS,
            &resmgr.io_funcs);

    resmgr.io_funcs.read = io_read;
    resmgr.io_funcs.write = io_write;

    iofunc_attr_init(&resmgr.iofunc_attr, S_IFCHR | 0666, NULL, NULL);
    resmgr.id = resmgr_attach(resmgr.dpp,        /* dispatch handle        */
                       NULL,                     /* resource manager attrs */
                       FAN_DEV_NAME,                 /* device name            */
                       _FTYPE_ANY,               /* open type              */
                       0,                        /* flags                  */
                       &resmgr.connect_funcs,    /* connect routines       */
                       &resmgr.io_funcs,         /* I/O routines           */
                       &resmgr.iofunc_attr);     /* handle                 */
    if (resmgr.id == -1) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Couldn't attach pathname, errno: %d", __func__, errno);
        return errno;
    }

    sig_init();

    return EOK;
}
