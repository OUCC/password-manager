#include <Siv3D.hpp>

/**
 * @brief 1つの項目のデータを定めた構造体
 * @details パスワードは暗号の状態は16進数文字列、復号化された状態では通常の文字列である
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
   * @param id ID
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
  void SIV3D_SERIALIZE(Archive& archive) {
    archive(service_name, user_name, password);
  }

  void encrypt(String key) {
    String tmp = U"";
    for (int i = 0; i < (password.length() + 15) / 16; i++) {
      tmp += aes256_encrypt(password.substr(i * 16, 16), key);
    }
    password = tmp;
  }

  void decrypt(String key) {
    String tmp = U"";
    for (int i = 0; i < password.length() / 512; i++) {
      tmp += aes256_decrypt(password.substr(i * 512, 512), key).substr(0, 16);
    }
    password = tmp;
  }
};
