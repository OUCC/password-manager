#include <Siv3D.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "db_exception.hpp"
#include "single_data.hpp"

using namespace std;

/**
 * @brief DBの表面上のI/Oを担当するクラス
 *
 */
class db {
 private:
  String main_db_name;
  String key = U"";

 public:
  /**
   * @brief dbのコンストラクタ
   *
   * @param key_db_name PWマネージャーのパスワードとして使用するDBファイル名
   * @param main_db_name データの保存先として使用するDBファイル名
   */
  db(String main_db_name = U"main.dat") : main_db_name(main_db_name) {}

  /**
   * @brief PWマネージャーのパスワードが既に登録されているかを返す
   *
   * @return true 登録されている場合
   * @return false 登録されていない場合
   * @exception db_exception 複数のキーが登録されていた場合
   */
  bool is_registered() { return FileSystem::IsFile(main_db_name); }

  /**
   * @brief PWマネージャーのパスワードを登録する
   * @detail ログイン処理も行われる
   *
   * @param passwd 登録するパスワード
   * @exception db_exception パスワードが既に登録されている場合、複数のキーが登録されていた場合
   */
  void register_passwd(String passwd) { key = passwd; }

  /**
   * @brief PWマネージャーのパスワードを変更する
   * @detail
   * DBの中身の暗号も新しい暗号のものに書き換わり、以降のread_data()、write_data()は新しいパスワードでの暗号化/復号がなされます
   * ログインも済んだことになります(ログイン済みの状態で呼び出すことが想定されますが…)
   *
   * @param new_passwd 変更後のパスワード
   * @exception db_exception パスワード未登録の場合
   * @return true 変更前のパスワードが正しく、変更に成功した場合
   * @return false 変更前のパスワードが間違っていて変更に失敗した場合
   */
  bool change_passwd(String new_passwd) {
    try {
      Array<single_data> data = read_data(key);
      key = new_passwd;
      write_data(data);
    } catch (Error &e) {
      return false;
    }
    return true;
  }

  /**
   * @brief データを読み込む
   * @details バイナリファイルから読み出され、復号された状態で出てくる
   *
   * @return Array<single_data> 読み込んだデータ(データがまだ作られていない場合長さ0のArray)
   * @exception db_exception まだログインしていない場合
   * @exception Error パスワード誤り／ファイル読み取り不可
   */
  Array<single_data> read_data(String passwd) {
    key = passwd;

    if (key.empty()) throw db_exception("invalid password");
    if (!FileSystem::IsFile(main_db_name)) return Array<single_data>();
    Deserializer<BinaryReader> reader(main_db_name);
    if (!reader) throw Error(U"Failed to open `{}`"_fmt(main_db_name));
    Array<single_data> data;
    reader(data);
    for (size_t i = 0; i < data.size(); i++) {
      if (!data[i].decrypt(key)) throw Error(U"Wrong password");
    }
    return data;
  }

  /**
   * @brief データを書き込む
   * @details 自動で暗号化してバイナリファイルに書き出す
   *
   * @param data 書き込むデータ
   * @exception db_exception まだログインしていない場合
   * @exception Error ファイル書き込み不可
   */
  void write_data(Array<single_data> data) {
    if (key.empty()) throw db_exception("invalid password");
    Serializer<BinaryWriter> writer(main_db_name);
    if (!writer) throw Error(U"Failed to open `{}`"_fmt(main_db_name));
    for (size_t i = 0; i < data.size(); i++) data[i].encrypt(key);
    writer(data);
  }

  /**
   * @brief データの初期化
   *
   * @return true 削除に成功した場合
   * @return false 削除に失敗し、ファイルが残った場合
   */
  bool reset() { return !FileSystem::IsFile(main_db_name) || FileSystem::Remove(main_db_name); }
};
