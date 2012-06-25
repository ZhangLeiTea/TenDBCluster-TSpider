/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include "includes.h"
#include "toku_os.h"
#include "checkpoint.h"

#define TESTDIR __SRCFILE__ ".dir"
#define FILENAME "test0.ft"

#include "test-ft-txns.h"

static void test_5123(void) {
    TOKULOGGER logger;
    CACHETABLE ct;
    test_setup(&logger, &ct);

    int r;

    r = toku_log_xbegin(logger, NULL, false, (TXNID) 1, TXNID_NONE);
    CKERR(r);
    r = toku_log_xbegin(logger, NULL, false, (TXNID) 3, TXNID_NONE);
    CKERR(r);
    r = toku_log_xbegin(logger, NULL, false, (TXNID) 2, TXNID_NONE);
    CKERR(r);

    r = toku_log_xcommit(logger, NULL, false, NULL, (TXNID) 2);
    CKERR(r);

    r = toku_logger_close_rollback(logger, false);
    CKERR(r);

    r = toku_cachetable_close(&ct);
    CKERR(r);
    // "Crash"
    r = toku_logger_close(&logger);
    CKERR(r);
    ct = NULL;
    logger = NULL;

    // "Recover"
    test_setup_and_recover(&logger, &ct);

    shutdown_after_recovery(&logger, &ct);
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_5123();
    return 0;
}
