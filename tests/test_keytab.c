#include "test_keytab.h"

#include "keytab.h"
#include <errno.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>

static bs_disk_t create_tmp_disk(void) {
  int fd = syscall(SYS_memfd_create, "keytab_test.bsf", 0);
  ck_assert_int_ne(ftruncate(fd, KEYTAB_SIZE), -1);

  bs_disk_t disk;
  ck_assert_int_eq(disk_create(fd, &disk), 0);
  return disk;
}

START_TEST(test_keytab_store_lookup_roundtrip) {
  const char* pass1 = "pass1";
  const char* pass2 = "pass2";

  const uint8_t key1[16] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
                             0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
  const uint8_t key2[16] = { 0xde, 0xad, 0xbe, 0xef, 0xbe, 0xef, 0xca, 0xfe,
                             0xde, 0xad, 0xde, 0xad, 0xde, 0xad, 0xc0, 0xde };

  bs_disk_t disk = create_tmp_disk();

  ck_assert_int_eq(keytab_store(disk, 0, pass1, key1), 0);
  ck_assert_int_eq(keytab_store(disk, 1, pass2, key2), 0);

  uint8_t recovered_key[16] = { 0 };

  ck_assert_int_eq(keytab_lookup(disk, pass1, recovered_key), 0);
  ck_assert_int_eq(memcmp(recovered_key, key1, 16), 0);

  ck_assert_int_eq(keytab_lookup(disk, pass2, recovered_key), 0);
  ck_assert_int_eq(memcmp(recovered_key, key2, 16), 0);

  ck_assert_int_eq(keytab_lookup(disk, "pass3", recovered_key), -ENOENT);

  disk_free(disk);
}
END_TEST

START_TEST(test_keytab_store_past_end) {
  const uint8_t key[16] = { 0xde, 0xad, 0xbe, 0xef, 0xbe, 0xef, 0xca, 0xfe,
                            0xde, 0xad, 0xde, 0xad, 0xde, 0xad, 0xc0, 0xde };

  bs_disk_t disk = create_tmp_disk();
  ck_assert_int_eq(
      keytab_store(disk, MAX_LEVELS, "password", key),
      -EINVAL);
  disk_free(disk);
}
END_TEST

Suite* keytab_suite(void) {
  Suite* suite = suite_create("keytab");

  TCase* roundtrip_tcase = tcase_create("roundtrip");
  tcase_add_test(roundtrip_tcase, test_keytab_store_lookup_roundtrip);
  suite_add_tcase(suite, roundtrip_tcase);

  TCase* store_tcase = tcase_create("store");
  tcase_add_test(store_tcase, test_keytab_store_past_end);
  suite_add_tcase(suite, store_tcase);

  return suite;
}
