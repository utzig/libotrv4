#include <glib.h>

#define OTRV4_KEY_MANAGEMENT_PRIVATE
#define OTRV4_USER_PROFILE_PRIVATE
#define OTRV4_LIST_PRIVATE
#define OTRV4_OTRV4_PRIVATE
#define OTRV4_SMP_PRIVATE

#include "../otrv4.h"

// clang-format off
#include "test_helpers.h"
#include "test_fixtures.h"
// clang-format on

#include "test_api.c"
#include "test_client.c"
#include "test_dake.c"
#include "test_data_message.c"
#include "test_dh.c"
#include "test_ed448.c"
#include "test_fragment.c"
#include "test_identity_message.c"
#include "test_instance_tag.c"
#include "test_key_management.c"
#include "test_list.c"
#include "test_non_interactive_messages.c"
#include "test_otrv4.c"
#include "test_serialize.c"
#include "test_smp.c"
#include "test_tlv.c"
#include "test_user_profile.c"
#include "test_messaging.c"

int main(int argc, char **argv) {
  if (!gcry_check_version(GCRYPT_VERSION))
    return 2;

  // TODO: we are using gcry_mpi_snew, so we might need this
  // gcry_control (GCRYCTL_INIT_SECMEM, 1);
  // gcry_control (GCRYCTL_RESUME_SECMEM_WARN);
  // gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

  /* Set to quick random so we don't wait on /dev/random. */
  gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM, 0);

  g_test_init(&argc, &argv, NULL);

  g_test_add_func("/api/multiple_clients", test_api_multiple_clients);
  g_test_add_func("/otrv4/instance_tag/generates_when_file_empty",
                  test_instance_tag_generates_tag_when_file_empty);
  g_test_add_func("/otrv4/instance_tag/generates_when_file_is_full",
                  test_instance_tag_generates_tag_when_file_is_full);

  g_test_add_func("/user_state/key_management", test_userstate_key_management);

  g_test_add_func("/edwards448/api", ed448_test_ecdh);
  g_test_add_func("/edwards448/eddsa_serialization",
                  ed448_test_eddsa_serialization);
  g_test_add_func("/edwards448/eddsa_keygen", ed448_test_eddsa_keygen);
  g_test_add_func("/edwards448/scalar_serialization",
                  ed448_test_scalar_serialization);

  g_test_add_func("/dake/snizkpk", test_snizkpk_auth);
  g_test_add_func("/list/add", test_otrv4_list_add);
  g_test_add_func("/list/get", test_otrv4_list_get_last);
  g_test_add_func("/list/length", test_otrv4_list_len);
  g_test_add_func("/list/empty_size", test_list_empty_size);

  g_test_add_func("/dh/api", dh_test_api);
  g_test_add_func("/dh/serialize", dh_test_serialize);
  g_test_add_func("/dh/destroy", dh_test_keypair_destroy);

  g_test_add_func("/serialize_and_deserialize/uint", test_ser_deser_uint);
  g_test_add_func("/serialize_and_deserialize/data",
                  test_serialize_otrv4_deserialize_data);
  g_test_add_func("/serialize/dh-public-key",
                  test_otrv4_serialize_dh_public_key);
  g_test_add_func("/serialize_and_deserialize/ed448-public-key",
                  test_ser_des_otrv4_public_key);
  g_test_add_func("/serialize_and_deserialize/ed448-shared-prekey",
                  test_ser_des_otrv4_shared_prekey);
  g_test_add_func("/serialize/otrv4-symmetric-key",
                  test_serialize_otrv4_symmetric_key);

  g_test_add_func("/user_profile/create", test_user_profile_create);
  g_test_add_func("/user_profile/serialize_body",
                  test_user_profile_serializes_body);
  g_test_add_func("/user_profile/serialize", test_user_profile_serializes);
  g_test_add_func("/user_profile/deserializes",
                  test_otrv4_user_profile_deserializes);
  g_test_add_func("/user_profile/sign_and_verifies",
                  test_user_profile_signs_and_verify);
  g_test_add_func("/user_profile/build_user_profile",
                  test_otrv4_user_profile_build);

  WITH_FIXTURE("/dake/identity_message/serializes",
               test_dake_identity_message_serializes,
               identity_message_fixture_t, identity_message_fixture);
  WITH_FIXTURE("/dake/identity_message/deserializes",
               test_otrv4_dake_identity_message_deserializes,
               identity_message_fixture_t, identity_message_fixture);
  WITH_FIXTURE("/dake/identity_message/valid", test_dake_identity_message_valid,
               identity_message_fixture_t, identity_message_fixture);
  WITH_FIXTURE("/dake/prekey_message/serializes",
               test_dake_prekey_message_serializes, prekey_message_fixture_t,
               prekey_message_fixture);
  WITH_FIXTURE("/dake/prekey_message/deserializes",
               test_otrv4_dake_prekey_message_deserializes,
               prekey_message_fixture_t, prekey_message_fixture);
  WITH_FIXTURE("/dake/prekey_message/valid", test_dake_prekey_message_valid,
               prekey_message_fixture_t, prekey_message_fixture);
  g_test_add_func("/data_message/serialize", test_data_message_serializes);
  g_test_add_func("/data_message/deserialize",
                  test_otrv4_data_message_deserializes);

  g_test_add_func("/fragment/create_fragments", test_create_fragments);
  g_test_add_func("/fragment/defragment_message",
                  test_defragment_valid_message);
  g_test_add_func("/fragment/defragment_single_fragment",
                  test_defragment_single_fragment);
  g_test_add_func("/fragment/defragment_fails_without_comma",
                  test_defragment_without_comma_fails);
  g_test_add_func("/fragment/fails_for_invalid_tag",
                  test_defragment_fails_for_invalid_tag);

  g_test_add_func("/key_management/derive_ratchet_keys",
                  test_derive_ratchet_keys);
  g_test_add_func("/key_management/destroy", test_otrv4_key_manager_destroy);

  g_test_add_func("/smp/state_machine", test_smp_state_machine);
  g_test_add_func("/smp/generate_secret", test_otrv4_generate_smp_secret);
  g_test_add_func("/smp/msg_1_asprintf_null_question",
                  test_otrv4_smp_msg_1_asprintf_null_question);
  g_test_add_func("/tlv/parse", test_tlv_parse);
  g_test_add_func("/tlv/append", test_otrv4_append_tlv);
  g_test_add_func("/tlv/append_padding", test_otrv4_append_padding_tlv);

  // g_test_add_func("/otrv4/starts_protocol", test_otrv4_starts_protocol);
  // g_test_add("/otrv4/version_supports_v34", otrv4_fixture_t, NULL,
  // otrv4_fixture_set_up, test_otrv4_version_supports_v34,
  // otrv4_fixture_teardown );
  g_test_add("/otrv4/builds_query_message", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_builds_query_message,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/builds_query_message_v34", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_builds_query_message_v34,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/builds_whitespace_tag", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_builds_whitespace_tag,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/builds_whitespace_tag_v34", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_builds_whitespace_tag_v34,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_plaintext_without_ws_tag_on_start",
             otrv4_fixture_t, NULL, otrv4_fixture_set_up,
             test_otrv4_receives_plaintext_without_ws_tag_on_start,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_plaintext_without_ws_tag_not_on_start",
             otrv4_fixture_t, NULL, otrv4_fixture_set_up,
             test_otrv4_receives_plaintext_without_ws_tag_not_on_start,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_plaintext_with_ws_tag", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_receives_plaintext_with_ws_tag,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_plaintext_with_ws_tag_after_text",
             otrv4_fixture_t, NULL, otrv4_fixture_set_up,
             test_otrv4_receives_plaintext_with_ws_tag_after_text,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_plaintext_with_ws_tag_v3", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_receives_plaintext_with_ws_tag_v3,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_query_message", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_receives_query_message,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_query_message_v3", otrv4_fixture_t, NULL,
             otrv4_fixture_set_up, test_otrv4_receives_query_message_v3,
             otrv4_fixture_teardown);
  g_test_add("/otrv4/receives_invalid_instance_tag_on_identity_message",
             otrv4_fixture_t, NULL, otrv4_fixture_set_up,
             test_otrv4_receives_identity_message_validates_instance_tag,
             otrv4_fixture_teardown);
  g_test_add_func("/otrv4/destroy", test_otrv4_destroy);

  g_test_add_func("/api/interactive_conversation/v4",
                  test_api_interactive_conversation);
  // g_test_add_func("/api/interactive_conversation_bob/v4",
  //                test_api_interactive_conversation_bob);
  g_test_add_func("/api/non_interactive_conversation/v4",
                  test_api_non_interactive_conversation);
  g_test_add_func("/api/non_interactive_conversation_enc_msg/v4",
                  test_api_non_interactive_conversation_with_enc_msg);
  g_test_add_func("/api/conversation_errors", test_api_conversation_errors);
  g_test_add_func("/api/conversation/v3", test_api_conversation_v3);
  g_test_add_func("/api/smp", test_api_smp);
  g_test_add_func("/api/smp_abort", test_api_smp_abort);
  g_test_add_func("/api/messaging", test_api_messaging);
  g_test_add_func("/api/instance_tag", test_instance_tag_api);
  g_test_add_func("/api/dh_key_rotation", test_dh_key_rotation);
  g_test_add_func("/api/extra_symm_key", test_api_extra_sym_key);
  g_test_add_func("/api/ecdh_keys_destroy",
                  test_ecdh_priv_keys_destroyed_early);
  g_test_add_func("/api/unreadable", test_unreadable_flag);
  g_test_add_func("/api/heartbeat", test_heartbeat_messages);

  g_test_add_func("/client/conversation_api", test_client_conversation_api);
  g_test_add_func("/client/api", test_client_api);
  g_test_add_func("/client/get_our_fingerprint",
                  test_client_get_our_fingerprint);
  g_test_add_func("/client/fingerprint_to_human",
                  test_fingerprint_hash_to_human);
  g_test_add_func("/client/sends_fragments",
                  test_client_sends_fragmented_message);
  g_test_add_func("/client/receives_fragments",
                  test_client_receives_fragmented_message);

  g_test_add_func("/client/conversation_data_message_multiple_locations",
                  test_conversation_with_multiple_locations);
  g_test_add_func("/client/identity_message_in_waiting_auth_i",
                  test_valid_identity_msg_in_waiting_auth_i);
  g_test_add_func("/client/identity_message_in_waiting_auth_r",
                  test_valid_identity_msg_in_waiting_auth_r);
  g_test_add_func("/client/invalid_auth_r_msg_in_not_waiting_auth_r",
                  test_invalid_auth_r_msg_in_not_waiting_auth_r);
  g_test_add_func("/client/invalid_auth_i_msg_in_not_waiting_auth_i",
                  test_invalid_auth_i_msg_in_not_waiting_auth_i);

  return g_test_run();
}
