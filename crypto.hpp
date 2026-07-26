#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <Siv3D.hpp>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

String binary_to_hexstring(unsigned char* binary, int size) {
  stringstream ss;
  ss << hex << setfill('0');
  for (int i = 0; i < size; i++) ss << setw(2) << (int)binary[i];
  return Unicode::WidenAscii(ss.str());
}

String binary_to_hexstring(const unsigned char* binary, int size) {
  stringstream ss;
  ss << hex << setfill('0');
  for (int i = 0; i < size; i++) ss << setw(2) << (int)binary[i];
  return Unicode::WidenAscii(ss.str());
}

String sha256(String text) {
  char c[64] = {};
  unsigned char digest[SHA256_DIGEST_LENGTH];
  size_t digest_len;
  string str = text.toUTF8();

  EVP_Q_digest(NULL, "SHA256", NULL, str.c_str(), str.length(), digest, &digest_len);

  return binary_to_hexstring(digest, SHA256_DIGEST_LENGTH);
}

/** FIXME: Uses deprecated OpenSSL APIs and weak ECB (Electronic CodeBook) mode encryption */
String aes256_encrypt(String text, String key) {
  char text_c[64] = {};
  string text_string = text.toUTF8();
  strncpy(text_c, text_string.c_str(), sizeof(text_c));
  const unsigned char* text_cuc = (const unsigned char*)text_c;

  char key_c[64] = {};
  string key_string = Unicode::NarrowAscii(key);
  strncpy(key_c, key_string.c_str(), sizeof(key_c));
  const unsigned char* key_cuc = (const unsigned char*)key_c;

  unsigned char enc_out[256];
  AES_KEY enc_key;
  AES_set_encrypt_key(key_cuc, 256, &enc_key);
  AES_encrypt(text_cuc, enc_out, &enc_key);

  return binary_to_hexstring(enc_out, 256);
}

/** FIXME: Uses deprecated OpenSSL APIs and weak ECB (Electronic CodeBook) mode encryption */
String aes256_decrypt(String hex_text, String key) {
  unsigned char binary_text[256];
  string hex_text_string = Unicode::NarrowAscii(hex_text);
  for (int i = 0; i < 256; i++) {
    binary_text[i] = (unsigned char)stol(hex_text_string.substr(i * 2, 2), nullptr, 16);
  }

  char key_c[64] = {};
  string key_string = Unicode::NarrowAscii(key);
  strncpy(key_c, key_string.c_str(), sizeof(key_c));
  const unsigned char* key_cuc = (const unsigned char*)key_c;

  unsigned char dec_out[256];
  AES_KEY dec_key;
  AES_set_decrypt_key(key_cuc, 256, &dec_key);
  AES_decrypt(binary_text, dec_out, &dec_key);

  return Unicode::FromUTF8(string((char*)dec_out));
}
