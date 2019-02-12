#include "test_bft.h"
#include "test_bit_util.h"
#include "test_cluster.h"
#include "test_disk.h"
#include "test_enc.h"
#include "test_fuse_ops.h"
#include "test_keytab.h"
#include "test_matrix.h"
#include "test_stego.h"
#include "test_vector.h"
#include <check.h>
#include <stdlib.h>

int main() {
  SRunner* runner = srunner_create(bft_suite());
  srunner_add_suite(runner, bit_util_suite());
  srunner_add_suite(runner, cluster_suite());
  srunner_add_suite(runner, enc_suite());
  srunner_add_suite(runner, fuse_ops_suite());
  srunner_add_suite(runner, keytab_suite());
  srunner_add_suite(runner, stego_suite());
  srunner_add_suite(runner, disk_suite());
  srunner_add_suite(runner, vector_suite());
  srunner_add_suite(runner, matrix_suite());

  srunner_set_fork_status(runner, CK_NOFORK);
  srunner_run_all(runner, CK_NORMAL);
  int failed_tests = srunner_ntests_failed(runner);
  srunner_free(runner);

  return failed_tests ? EXIT_FAILURE : EXIT_SUCCESS;
}
