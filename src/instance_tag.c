#include <libotr/instag.h>
#include <string.h>

#define OTRV4_INSTANCE_TAG_PRIVATE

#include "instance_tag.h"
#include "str.h"

API otrv4_bool_t otrv4_instag_get(otrv4_instag_t *otrv4_instag,
                                  const char *account, const char *protocol,
                                  FILE *filename) {

  OtrlUserState us = otrl_userstate_create();

  if (otrl_instag_read_FILEp(us, filename)) {
    otrl_userstate_free(us);
    return otrv4_false;
  }

  OtrlInsTag *tmp_instag;
  tmp_instag = otrl_instag_find(us, account, protocol);

  if (!tmp_instag) {
    if (otrl_instag_generate_FILEp(us, filename, account, protocol)) {
      otrl_userstate_free(us);
      return otrv4_false;
    }
    tmp_instag = otrl_instag_find(us, account, protocol);
  }

  otrv4_instag->account = otrv4_strdup(tmp_instag->accountname);
  otrv4_instag->protocol = otrv4_strdup(tmp_instag->protocol);
  otrv4_instag->value = tmp_instag->instag;

  otrl_userstate_free(us);

  return otrv4_true;
}

API void otrv4_instag_free(otrv4_instag_t *instag) {
  if (!instag)
    return;

  free(instag->account);
  instag->account = NULL;

  free(instag->protocol);
  instag->protocol = NULL;

  free(instag);
  instag = NULL;
}
