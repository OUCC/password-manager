#include <Siv3D.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "crypto.hpp"
#include "db_exception.hpp"
#include "single_data.hpp"

using namespace std;

/**
 * @brief DBの表面上のI/Oを担当するクラス
 *
 */
class db {
 private:
  String key_db_name;
  String main_db_name;
  String key = U"";

  String get_db_key() {
    if (!FileSystem::IsFile(key_db_name)) return U"";
    Deserializer<BinaryReader> reader(key_db_name);
    if (!reader) {
      throw Error(U"Failed to open `{}`"_fmt(key_db_name));
    }
    String db_key;
    reader(db_key);
    return db_key;
  }

 public:
  /**
   * @brief dbのコンストラクタ
   *
   * @param key_db_name PWマネージャーのパスワードとして使用するDBファイル名
   * @param main_db_name データの保存先として使用するDBファイル名
   */
  db(String key_db_name = U"key.dat", String main_db_name = U"main.dat")
      : key_db_name(key_db_name), main_db_name(main_db_name) {
    if (!FileSystem::IsFile(key_db_name) && FileSystem::IsFile(main_db_name)) {
      FileSystem::Remove(main_db_name);
    }
  }

  /**
   * @brief PWマネージャーのパスワードが既に登録されているかを返す
   *
   * @return true 登録されている場合
   * @return false 登録されていない場合
   * @exception db_exception 複数のキーが登録されていた場合
   */
  bool is_registered() { return get_db_key() != U""; }

  /**
   * @brief PWマネージャーにすでにログインしているかを返す
   *
   * @return true 既にログインしている場合
   * @return false まだログインできていない場合
   */
  bool is_logined() { return key != U""; }

  /**
   * @brief PWマネージャーのパスワードを登録する
   * @detail ログイン処理も行われる
   *
   * @param passwd 登録するパスワード
   * @exception db_exception パスワードが既に登録されている場合、複数のキーが登録されていた場合
   */
  void register_passwd(String passwd) {
    String db_key = get_db_key();
    if (db_key != U"") {
      throw db_exception("a key has already been registered");
    }
    db_key = sha256(passwd);
    Serializer<BinaryWriter> writer(key_db_name);
    if (!writer) {
      throw Error(U"Failed to open `{}`"_fmt(key_db_name));
    }
    writer(db_key);
    key = passwd;
  }

  /**
   * @brief PWマネージャーのパスワードを変更する
   * @detail
   * DBの中身の暗号も新しい暗号のものに書き換わり、以降のread_data()、write_data()は新しいパスワードでの暗号化/復号化化がなされます
   * ログインも済んだことになります(ログイン済みの状態で呼び出すことが想定されますが…)
   *
   * @param old_passwd 変更前のパスワード
   * @param new_passwd 変更後のパスワード
   * @exception db_exception パスワード未登録の場合
   * @return true 変更前のパスワードが正しく、変更に成功した場合
   * @return false 変更前のパスワードが間違っていて変更に失敗した場合
   */
  bool change_passwd(String old_passwd, String new_passwd) {
    String old_hash = sha256(old_passwd);
    String db_key = get_db_key();
    if (db_key == U"") {
      throw db_exception("no key has been registered");
    }
    if (old_hash != db_key) {
      return false;
    }
    db_key = sha256(new_passwd);
    Serializer<BinaryWriter> writer(key_db_name);
    if (!writer) {
      throw Error(U"Failed to open `{}`"_fmt(key_db_name));
    }
    writer(db_key);

    Array<single_data> data = read_data();
    key = new_passwd;
    write_data(data);
    return true;
  }

  /**
   * @brief PWマネージャーにログインする
   *
   * @param passwd ログインパスワード
   * @return true パスワードが正しく、ログインに成功した場合
   * @return false パスワードが間違っていてログインに失敗した場合
   * @exception db_exception パスワードが未登録の場合、複数のキーが登録されていた場合
   */
  bool login(String passwd) {
    String hash = sha256(passwd);
    String db_key = get_db_key();
    if (db_key == U"") {
      throw db_exception("no key has been registered");
    }
    if (hash != db_key) {
      return false;
    }
    key = passwd;
    return true;
  }

  /**
   * @brief データを読み込む
   * @details バイナリファイルから読み出され、復号化された状態で出てくる
   *
   * @return Array<single_data> 読み込んだデータ(データがまだ作られていない場合長さ0のArray)
   * @exception db_exception まだログインしていない場合
   */
  Array<single_data> read_data() {
    if (!is_logined()) {
      throw db_exception("not logined");
    }
    if (!FileSystem::IsFile(main_db_name)) return Array<single_data>();
    Deserializer<BinaryReader> reader(main_db_name);
    if (!reader) {
      throw Error(U"Failed to open `{}`"_fmt(main_db_name));
    }
    Array<single_data> data;
    reader(data);
    for (int i = 0; i < data.size(); i++) {
      data[i].decrypt(key);
    }
    return data;
  }

  /**
   * @brief データを書き込む
   * @details 自動で暗号化してバイナリファイルに書き出す
   *
   * @param data 書き込むデータ
   * @exception db_exception まだログインしていない場合
   */
  void write_data(Array<single_data> data) {
    if (!is_logined()) {
      throw db_exception("not logined");
    }
    Serializer<BinaryWriter> writer(main_db_name);
    if (!writer) {
      throw Error(U"Failed to open `{}`"_fmt(main_db_name));
    }
    for (int i = 0; i < data.size(); i++) {
      data[i].encrypt(key);
    }
    writer(data);
  }

  /**
   * @brief データの初期化
   *
   * @return true 削除に成功した場合
   * @return false 削除に失敗し、ファイルが残った場合
   */
  bool reset() {
    String targets[] = {key_db_name, main_db_name};
    for (int i = 0; i < 2; i++) {
      if (FileSystem::IsFile(targets[i])) {
        if (!FileSystem::Remove(targets[i])) {
          return false;
        }
      }
    }
    return true;
  }
};
