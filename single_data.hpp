#include <openssl/aes.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <stdint.h>

#include <Siv3D.hpp>
#include <iomanip>

/**
 * @brief 1つの項目のデータを定めた構造体
 * @details パスワードは暗号の状態は16進数文字列、復号された状態では通常の文字列である
 *
 */
class single_data {
 public:
  /** サービス名 */
  String service_name;
  /** ユーザー名 */
  String user_name;
  /** パスワード */
  String password;

  /**
   * @brief コンストラクタ
   *
   * @param service_name サービス名
   * @param user_name ユーザー名
   * @param password パスワード
   */
  single_data(String service_name, String user_name, String password)
      : service_name(service_name), user_name(user_name), password(password) {}

  /**
   * @brief デフォルトコンストラクタ
   *
   */
  single_data() {}

  template <class Archive>
  void SIV3D_SERIALIZE(Archive &archive) {
    archive(service_name, user_name, password);
  }

  void encrypt(String master) {
    string enc_in = password.toUTF8();
    const int input_len = enc_in.length();
    auto *enc_out = new unsigned char[SALT_LEN + IV_LEN + input_len + TAG_LEN];
    auto *salt = enc_out;
    auto *iv = salt + SALT_LEN;
    auto *ciphertext = iv + IV_LEN;
    auto *tag = ciphertext + input_len;
    unsigned char key[KEY_LEN];
    RAND_bytes(salt, SALT_LEN);
    derive_key(master.toUTF8().c_str(), master.length(), salt, key);

    int len, ciphertext_len = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    RAND_bytes(iv, IV_LEN);
    EVP_EncryptInit_ex2(ctx, EVP_CIPHER_fetch(nullptr, "AES-256-GCM", nullptr), key, iv, nullptr);
    EVP_EncryptUpdate(ctx, ciphertext, &len, (const unsigned char *)enc_in.c_str(), input_len);
    ciphertext_len += len;
    EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag);
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key, sizeof(key));

    password = binary_to_hexstring(enc_out, SALT_LEN + IV_LEN + ciphertext_len + TAG_LEN);
    delete[] enc_out;
  }

  bool decrypt(String master) {
    string hex_text_string = password.toUTF8();
    const int input_len = hex_text_string.length() / 2;
    auto *dec_in = new unsigned char[input_len];
    for (int i = 0; i < input_len; i++) {
      dec_in[i] = (unsigned char)stol(hex_text_string.substr(i * 2, 2), nullptr, 16);
    }
    int ciphertext_len = input_len - SALT_LEN - IV_LEN - TAG_LEN;
    auto *salt = dec_in;
    auto *iv = salt + SALT_LEN;
    auto *ciphertext = iv + IV_LEN;
    auto *tag = ciphertext + ciphertext_len;
    unsigned char key[KEY_LEN];
    derive_key(master.toUTF8().c_str(), master.length(), salt, key);

    int len, plaintext_len = 0;
    auto *dec_out = new unsigned char[ciphertext_len];
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex2(ctx, EVP_CIPHER_fetch(nullptr, "AES-256-GCM", nullptr), key, iv, nullptr);
    EVP_DecryptUpdate(ctx, dec_out, &len, ciphertext, ciphertext_len);
    plaintext_len += len;
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, (void *)tag);
    int result = EVP_DecryptFinal_ex(ctx, dec_out + plaintext_len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key, sizeof(key));
    delete[] dec_in;

    if (result == 1) password = Unicode::FromUTF8(string((char *)dec_out));
    delete[] dec_out;
    return result == 1;
  }

 private:
  static constexpr int IV_LEN = 12, TAG_LEN = 16, SALT_LEN = 16, KEY_LEN = 32;

  static String binary_to_hexstring(const unsigned char *binary, int size) {
    stringstream ss;
    ss << hex << setfill('0');
    for (int i = 0; i < size; i++) ss << setw(2) << (int)binary[i];
    return Unicode::WidenAscii(ss.str());
  }

  static void derive_key(const char *master, size_t master_len, const unsigned char *salt, unsigned char *key) {
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "ARGON2ID", NULL);
    EVP_KDF_CTX *ctx = EVP_KDF_CTX_new(kdf);
    uint32_t iterations = 3, lanes = 1, memory_cost = 16384; /* KiB = 16 MiB */
    OSSL_PARAM params[] = {OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, (void *)master, master_len),
                           OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)salt, SALT_LEN),
                           OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations),
                           OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes),
                           OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memory_cost),
                           OSSL_PARAM_construct_end()};

    EVP_KDF_derive(ctx, key, KEY_LEN, params);
    EVP_KDF_CTX_free(ctx);
    EVP_KDF_free(kdf);
  }
};
