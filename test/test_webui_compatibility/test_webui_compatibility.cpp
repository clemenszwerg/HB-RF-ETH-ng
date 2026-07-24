#include <unity.h>

#include "webui_compatibility.h"

void setUp(void) {}
void tearDown(void) {}

void test_matching_api_and_minimum_firmware_are_compatible()
{
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_COMPATIBLE,
        webui_check_compatibility(
            1, "2.2.5-Beta.1", 1, "2.2.5-Beta.12"));
}

void test_release_firmware_satisfies_beta_minimum()
{
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_COMPATIBLE,
        webui_check_compatibility(1, "2.2.5-Beta.12", 1, "2.2.5"));
}

void test_api_mismatch_is_rejected_in_both_directions()
{
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_API_MISMATCH,
        webui_check_compatibility(2, "2.2.5", 1, "2.3.0"));
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_API_MISMATCH,
        webui_check_compatibility(1, "2.2.5", 2, "2.3.0"));
}

void test_old_firmware_is_rejected()
{
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_FIRMWARE_TOO_OLD,
        webui_check_compatibility(
            1, "2.2.5-Beta.12", 1, "2.2.5-Beta.9"));
}

void test_invalid_contract_metadata_is_rejected()
{
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_INVALID_METADATA,
        webui_check_compatibility(0, "2.2.5", 1, "2.2.5"));
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_INVALID_METADATA,
        webui_check_compatibility(1, "not-a-version", 1, "2.2.5"));
    TEST_ASSERT_EQUAL(
        WEBUI_COMPATIBILITY_INVALID_METADATA,
        webui_check_compatibility(1, "2.2.5", 1, "unknown"));
}

void test_semver_validator_rejects_ambiguous_values()
{
    TEST_ASSERT_TRUE(webui_is_valid_semver("1.0.0"));
    TEST_ASSERT_TRUE(webui_is_valid_semver("1.0.0-Beta.12"));
    TEST_ASSERT_FALSE(webui_is_valid_semver("v1.0.0"));
    TEST_ASSERT_FALSE(webui_is_valid_semver("1.0"));
    TEST_ASSERT_FALSE(webui_is_valid_semver("1.0.0-"));
    TEST_ASSERT_FALSE(webui_is_valid_semver("1.0.0-Beta..12"));
}

void setup()
{
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_matching_api_and_minimum_firmware_are_compatible);
    RUN_TEST(test_release_firmware_satisfies_beta_minimum);
    RUN_TEST(test_api_mismatch_is_rejected_in_both_directions);
    RUN_TEST(test_old_firmware_is_rejected);
    RUN_TEST(test_invalid_contract_metadata_is_rejected);
    RUN_TEST(test_semver_validator_rejects_ambiguous_values);
    UNITY_END();
}

void loop() {}
