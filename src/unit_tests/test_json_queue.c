/*
 * Copyright (C) 2015-2019, Wazuh Inc.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../headers/shared.h"

int __wrap__minfo()
{
    return 0;
}

void __wrap__merror(const char * file, int line, const char * func, const char *msg, ...)
{
    char formatted_msg[OS_MAXSTR];
    va_list args;

    va_start(args, msg);
    vsnprintf(formatted_msg, OS_MAXSTR, msg, args);
    va_end(args);

    check_expected(formatted_msg);
}

FILE * __wrap_fopen(const char *__restrict __filename, const char *__restrict __modes) __wur
{
    return mock_type(FILE *);
}

int __wrap_fseek()
{
    return mock();
}

int __wrap_fstat()
{
    return mock();
}

void __wrap_fileno()
{
    return;
}

int __wrap_fclose(FILE *__stream)
{
    check_expected_ptr(__stream);

    return 0;
}

void __wrap_clearerr()
{
    return;
}

char * __wrap_fgets (char *__restrict __s, int __n, FILE *__restrict __stream)
{
    check_expected(__n);
    check_expected_ptr(__stream);

    strcpy(__s, mock_type(char *));

    return mock_type(char *);
}

void test_jqueue_init(void **state)
{
    (void) state;

    file_queue *fileq;

    os_calloc(1, sizeof(file_queue), fileq);

    jqueue_init(fileq);

    assert_int_equal(fileq->last_change, 0);
    assert_int_equal(fileq->year, 0);
    assert_int_equal(fileq->day, 0);
    assert_int_equal(fileq->flags, 0);
    assert_null(fileq->mon[0]);
    assert_null(fileq->file_name[0]);
    assert_null(fileq->fp);

    free(fileq);
}

void test_jqueue_open(void **state)
{
    (void) state;

    file_queue *fileq;
    FILE *fp;

    os_calloc(1, sizeof(file_queue), fileq);
    os_calloc(1, sizeof(FILE), fp);

    /* fopen fail */

    jqueue_init(fileq);

    will_return(__wrap_fopen, NULL);
    expect_string(__wrap__merror, formatted_msg, "(1103): Could not open file '/var/ossec/logs/alerts/alerts.json' due to [(0)-(Success)].");

    int ret = jqueue_open(fileq, 0);

    assert_int_equal(ret, -1);
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_null(fileq->fp);

    /* fseek fail */

    jqueue_init(fileq);

    will_return(__wrap_fopen, fp);
    will_return(__wrap_fseek, -1);
    expect_string(__wrap__merror, formatted_msg, "(1103): Could not open file '/var/ossec/logs/alerts/alerts.json' due to [(0)-(Success)].");
    expect_memory(__wrap_fclose, __stream, fp, sizeof(fp));

    ret = jqueue_open(fileq, -1);

    assert_int_equal(ret, -1);
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_null(fileq->fp);

    /* fstat fail */

    jqueue_init(fileq);

    will_return(__wrap_fopen, fp);
    will_return(__wrap_fseek, 1);
    will_return(__wrap_fstat, -1);
    expect_string(__wrap__merror, formatted_msg, "(1117): Could not retrieve informations of file '/var/ossec/logs/alerts/alerts.json' due to [(0)-(Success)].");
    expect_memory(__wrap_fclose, __stream, fp, sizeof(fp));

    ret = jqueue_open(fileq, -1);

    assert_int_equal(ret, -1);
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_null(fileq->fp);

    /* success */

    jqueue_init(fileq);

    will_return(__wrap_fopen, fp);
    will_return(__wrap_fseek, 1);
    will_return(__wrap_fstat, 1);

    ret = jqueue_open(fileq, -1);

    assert_int_equal(ret, 0);
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_ptr_equal(fileq->fp, fp);

    free(fp);
    free(fileq);
}

void test_jqueue_next(void **state)
{
    (void) state;

    file_queue *fileq;
    FILE *fp;
    cJSON *json;

    os_calloc(1, sizeof(file_queue), fileq);
    os_calloc(1, sizeof(FILE), fp);

    /* jqueue_open fail */

    jqueue_init(fileq);

    will_return(__wrap_fopen, NULL);
    expect_string(__wrap__merror, formatted_msg, "(1103): Could not open file '/var/ossec/logs/alerts/alerts.json' due to [(0)-(Success)].");

    json = jqueue_next(fileq);

    assert_null(json);
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_null(fileq->fp);

    /* fgets success with \n */

    jqueue_init(fileq);

    will_return(__wrap_fopen, fp);
    will_return(__wrap_fseek, 1);
    will_return(__wrap_fstat, 1);

    expect_value(__wrap_fgets, __n, OS_MAXSTR + 1);
    expect_memory(__wrap_fgets, __stream, fp, sizeof(fp));
    will_return(__wrap_fgets, "{\"Test\":\"Hello World 1\"}\n");
    will_return(__wrap_fgets, "ok");

    json = jqueue_next(fileq);

    assert_string_equal(cJSON_GetObjectItem(json, "Test")->valuestring, "Hello World 1");
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_ptr_equal(fileq->fp, fp);

    cJSON_Delete(json);

    /* fgets success without \n */

    jqueue_init(fileq);

    will_return(__wrap_fopen, fp);
    will_return(__wrap_fseek, 1);
    will_return(__wrap_fstat, 1);

    expect_value(__wrap_fgets, __n, OS_MAXSTR + 1);
    expect_memory(__wrap_fgets, __stream, fp, sizeof(fp));
    will_return(__wrap_fgets, "{\"Test\":\"Hello World 2\"}");
    will_return(__wrap_fgets, "ok");

    json = jqueue_next(fileq);

    assert_string_equal(cJSON_GetObjectItem(json, "Test")->valuestring, "Hello World 2");
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_ptr_equal(fileq->fp, fp);

    cJSON_Delete(json);

    free(fp);
    free(fileq);
}

void test_jqueue_close(void **state)
{
    (void) state;

    file_queue *fileq;
    FILE *fp;

    os_calloc(1, sizeof(file_queue), fileq);
    os_calloc(1, sizeof(FILE), fp);

    jqueue_init(fileq);

    will_return(__wrap_fopen, fp);
    will_return(__wrap_fseek, 1);
    will_return(__wrap_fstat, 1);

    int ret = jqueue_open(fileq, -1);

    assert_int_equal(ret, 0);
    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_ptr_equal(fileq->fp, fp);

    expect_memory(__wrap_fclose, __stream, fp, sizeof(fp));

    jqueue_close(fileq);

    assert_string_equal(fileq->file_name, "/var/ossec/logs/alerts/alerts.json");
    assert_null(fileq->fp);

    free(fp);
    free(fileq);
}

void test_jqueue_flags(void **state)
{
    (void) state;

    file_queue *fileq;

    os_calloc(1, sizeof(file_queue), fileq);

    jqueue_init(fileq);

    assert_int_equal(fileq->flags, 0);

    jqueue_flags(fileq, CRALERT_READ_ALL | CRALERT_FP_SET);

    assert_int_equal(fileq->flags, CRALERT_READ_ALL | CRALERT_FP_SET);

    free(fileq);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_jqueue_init),
        cmocka_unit_test(test_jqueue_open),
        cmocka_unit_test(test_jqueue_next),
        cmocka_unit_test(test_jqueue_close),
        cmocka_unit_test(test_jqueue_flags)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}