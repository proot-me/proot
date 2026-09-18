/*
 * Unit tests for compare_paths() and join_paths() (src/path/path.c).
 *
 * These are plain, non-static, dependency-free functions: real
 * candidates for testing directly, without the ptrace/rootfs
 * machinery test/GNUmakefile's black-box suite needs.
 *
 * test_compare_paths_toplevel_proc_entry below is the regression case
 * that motivated adding this layer at all: PR #438 fixed a crash
 * caused by assuming compare_paths("/proc", "/proc") returns
 * PATH1_IS_PREFIX, when it actually returns PATHS_ARE_EQUAL. This
 * test would have caught that in milliseconds, no ptrace trace or
 * ARM hardware required.
 */
#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "path/path.h"

START_TEST(test_compare_paths_toplevel_proc_entry)
{
    /* A direct child of "/proc", once its last path component is
     * stripped, compares equal to "/proc" itself: not a prefix
     * relationship. See PR #438. */
    ck_assert_int_eq(compare_paths("/proc", "/proc"), PATHS_ARE_EQUAL);
}
END_TEST

START_TEST(test_compare_paths_prefix)
{
    ck_assert_int_eq(compare_paths("/proc", "/proc/self"), PATH1_IS_PREFIX);
}
END_TEST

START_TEST(test_compare_paths_reverse_prefix)
{
    ck_assert_int_eq(compare_paths("/proc/self", "/proc"), PATH2_IS_PREFIX);
}
END_TEST

START_TEST(test_compare_paths_unrelated)
{
    /* This is the case PR #384's fix got backwards: an unrelated
     * path isn't PATH1_IS_PREFIX or PATH2_IS_PREFIX of "/proc", it's
     * PATHS_ARE_NOT_COMPARABLE, even though both paths are otherwise
     * perfectly normal. */
    ck_assert_int_eq(compare_paths("/proc", "/etc"), PATHS_ARE_NOT_COMPARABLE);
}
END_TEST

START_TEST(test_compare_paths_empty)
{
    ck_assert_int_eq(compare_paths("", "/etc"), PATHS_ARE_NOT_COMPARABLE);
}
END_TEST

START_TEST(test_compare_paths_shared_prefix_not_a_subpath)
{
    /* "/proc" is not a path-component prefix of "/proclaim": the
     * comparison must not treat a shared string prefix as a shared
     * path prefix. */
    ck_assert_int_eq(compare_paths("/proc", "/proclaim"), PATHS_ARE_NOT_COMPARABLE);
}
END_TEST

START_TEST(test_join_paths_basic)
{
    char result[PATH_MAX];
    join_paths(2, result, "/a", "b");
    ck_assert_str_eq(result, "/a/b");
}
END_TEST

START_TEST(test_join_paths_trailing_slash)
{
    char result[PATH_MAX];
    join_paths(2, result, "/a/", "b");
    ck_assert_str_eq(result, "/a/b");
}
END_TEST

START_TEST(test_join_paths_leading_slash)
{
    char result[PATH_MAX];
    join_paths(2, result, "/a", "/b");
    ck_assert_str_eq(result, "/a/b");
}
END_TEST

START_TEST(test_join_paths_three)
{
    char result[PATH_MAX];
    join_paths(3, result, "/a", "b", "c");
    ck_assert_str_eq(result, "/a/b/c");
}
END_TEST

static Suite *path_suite(void)
{
    Suite *s = suite_create("path");

    TCase *tc_compare_paths = tcase_create("compare_paths");
    tcase_add_test(tc_compare_paths, test_compare_paths_toplevel_proc_entry);
    tcase_add_test(tc_compare_paths, test_compare_paths_prefix);
    tcase_add_test(tc_compare_paths, test_compare_paths_reverse_prefix);
    tcase_add_test(tc_compare_paths, test_compare_paths_unrelated);
    tcase_add_test(tc_compare_paths, test_compare_paths_empty);
    tcase_add_test(tc_compare_paths, test_compare_paths_shared_prefix_not_a_subpath);
    suite_add_tcase(s, tc_compare_paths);

    TCase *tc_join_paths = tcase_create("join_paths");
    tcase_add_test(tc_join_paths, test_join_paths_basic);
    tcase_add_test(tc_join_paths, test_join_paths_trailing_slash);
    tcase_add_test(tc_join_paths, test_join_paths_leading_slash);
    tcase_add_test(tc_join_paths, test_join_paths_three);
    suite_add_tcase(s, tc_join_paths);

    return s;
}

int main(void)
{
    Suite *s = path_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
