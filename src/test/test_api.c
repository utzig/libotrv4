#include <string.h>

#include "../protocol.h"
#include "../str.h"

void
test_api_conversation(void) {
  OTR4_INIT;

  cs_keypair_t cs_alice, cs_bob;
  cs_generate_keypair(cs_alice);
  cs_generate_keypair(cs_bob);

  otrv4_t *alice = otrv4_new(cs_alice);
  otrv4_t *bob = otrv4_new(cs_bob);

  otrv4_response_t *response_to_bob, *response_to_alice;
  string_t query_message = NULL;

  otrv4_build_query_message(&query_message, alice, "");
  otrv4_assert_cmpmem("?OTRv", query_message, 5);

  //Bob receives query message
  response_to_alice = otrv4_receive_message(bob, query_message);

  //Should reply with a pre-key
  otrv4_assert(response_to_alice);
  otrv4_assert(response_to_alice->to_display == NULL);
  otrv4_assert(response_to_alice->to_send);
  otrv4_assert_cmpmem("?OTR:AAQP", response_to_alice->to_send, 9);

  //Alice receives pre-key
  response_to_bob = otrv4_receive_message(alice, response_to_alice->to_send);

  //Alice has Bob's ephemeral keys
  otrv4_assert_ec_public_key_eq(alice->their_ecdh, bob->our_ecdh->pub);
  otrv4_assert_dh_public_key_eq(alice->their_dh, bob->our_dh->pub);

  //Should reply with a dre-auth
  otrv4_assert(response_to_bob);
  otrv4_assert(response_to_bob->to_display == NULL);
  otrv4_assert(response_to_bob->to_send);
  otrv4_assert_cmpmem("?OTR:AAQA", response_to_bob->to_send, 9);

  otrv4_free(alice);
  otrv4_free(bob);
}
