#include <stdexcept>

using namespace std;

/**
 * @brief 想定されていない操作が行われたことを表す例外クラス
 * @details
 * 基本的にUIや実装のミスによってのみ発生する例外であるが、本アプリが複数開かれていた等ユーザーが想定外の操作をした場合にも発生する可能性がある
 *
 */
class db_exception : public runtime_error {
 public:
  db_exception(const char *message) : runtime_error(message) {}
};
