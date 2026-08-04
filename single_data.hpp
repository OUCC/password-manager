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
 * @details 各要素は暗号の状態では16進数文字列、復号された状態では通常の文字列である
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
  single_data(const String &service_name, const String &user_name, const String &password)
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

  void encrypt(const String &master) {
    string svc_in = service_name.toUTF8();
    string usr_in = user_name.toUTF8();
    string pwd_in = password.toUTF8();
    string strMaster = master.toUTF8();

    unsigned char salt[SALT_LEN], key[KEY_LEN];
    RAND_bytes(salt, SALT_LEN);
    derive_key(strMaster.c_str(), strMaster.length(), salt, key);

    service_name = encrypt_string(svc_in, salt, key);
    user_name = encrypt_string(usr_in, salt, key);
    password = encrypt_string(pwd_in, salt, key);
    OPENSSL_cleanse(key, sizeof(key));
  }

  bool decrypt(const String &master) {
    string svc_in = service_name.toUTF8();
    string usr_in = user_name.toUTF8();
    string pwd_in = password.toUTF8();
    string strMaster = master.toUTF8();

    bool result = true;
    try {
      service_name = decrypt_string(svc_in, strMaster);
      user_name = decrypt_string(usr_in, strMaster);
      password = decrypt_string(pwd_in, strMaster);
    } catch (db_exception &e) {
      result = false;
    }
    return result;
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

  static String encrypt_string(const string &text, const unsigned char *salt, const unsigned char *key) {
    auto *enc_out = new unsigned char[SALT_LEN + IV_LEN + text.length() + TAG_LEN];
    memcpy(enc_out, salt, SALT_LEN);
    auto *iv = enc_out + SALT_LEN;
    auto *ciphertext = iv + IV_LEN;
    auto *tag = ciphertext + text.length();

    int len, ciphertext_len = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    RAND_bytes(iv, IV_LEN);
    EVP_EncryptInit_ex2(ctx, EVP_aes_256_gcm(), key, iv, nullptr);
    EVP_EncryptUpdate(ctx, ciphertext, &len, (const unsigned char *)text.c_str(), text.length());
    ciphertext_len += len;
    EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag);
    EVP_CIPHER_CTX_free(ctx);

    String result = binary_to_hexstring(enc_out, SALT_LEN + IV_LEN + ciphertext_len + TAG_LEN);
    delete[] enc_out;
    return result;
  }

  static String decrypt_string(const string &text, const string &master) {
    auto *dec_in = new unsigned char[text.length() / 2];
    for (size_t i = 0; i < text.length() / 2; i++) {
      dec_in[i] = (unsigned char)stol(text.substr(i * 2, 2), nullptr, 16);
    }
    int ciphertext_len = text.length() / 2 - SALT_LEN - IV_LEN - TAG_LEN;
    auto *iv = dec_in + SALT_LEN;
    auto *ciphertext = iv + IV_LEN;
    auto *tag = ciphertext + ciphertext_len;
    unsigned char key[KEY_LEN];
    derive_key(master.c_str(), master.length(), dec_in, key);

    int len, plaintext_len = 0;
    auto *dec_out = new unsigned char[ciphertext_len];
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex2(ctx, EVP_aes_256_gcm(), key, iv, nullptr);
    EVP_DecryptUpdate(ctx, dec_out, &len, ciphertext, ciphertext_len);
    plaintext_len += len;
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN, (void *)tag);
    int result = EVP_DecryptFinal_ex(ctx, dec_out + plaintext_len, &len);
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key, sizeof(key));
    delete[] dec_in;

    String plain;
    if (result == 1) plain = Unicode::FromUTF8(string((char *)dec_out, plaintext_len));
    delete[] dec_out;
    if (result == 1) {
      return plain;
    } else {
      throw db_exception("wrong password");
    }
  }
};
